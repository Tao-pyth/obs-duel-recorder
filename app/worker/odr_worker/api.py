from __future__ import annotations

import logging
from pathlib import Path

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from .config import LoadedWorkerConfig, get_repo_root
from .db import DbInfo
from .health import API_VERSION, PID, STARTED_AT, WorkerDb, WorkerPaths, build_health_payload
from .identity import INSTANCE_ID
from .overlay import OverlayPayloadError, OverlayState, apply_overlay_update
from .queue import QueueCommandError, QueueStore
from .recording import (
    RecordingCommandError,
    apply_recording_command,
    initialize_recording_state,
    overlay_recording_state,
    recording_state_path,
    save_recording_state,
)
from .runtime_dirs import RuntimeDirs
from .version import __version__


def _error_payload(*, code: str, message: str, details: object | None = None) -> dict[str, object]:
    payload: dict[str, object] = {"code": code, "message": message}
    if details is not None:
        payload["details"] = details
    return payload


def _error_response(*, status_code: int, code: str, message: str, details: object | None = None) -> JSONResponse:
    return JSONResponse(
        status_code=status_code,
        content=_error_payload(code=code, message=message, details=details),
    )


def create_app(*, runtime_dirs: RuntimeDirs, loaded_config: LoadedWorkerConfig, db_info: DbInfo | None = None) -> FastAPI:
    app = FastAPI(title="OBS Duel Recorder Worker", version=__version__)

    app_dir = (get_repo_root() / "app").resolve()

    worker_paths = WorkerPaths(
        app_dir=app_dir,
        user_data_dir=runtime_dirs.user_data_dir,
        config_dir=runtime_dirs.config_dir,
        data_dir=runtime_dirs.data_dir,
        logs_dir=runtime_dirs.logs_dir,
    )

    app.state.paths = worker_paths
    app.state.config_loaded = loaded_config.config_loaded
    app.state.overlay_state = OverlayState()
    app.state.recording_state_path = recording_state_path(runtime_dirs.data_dir)
    app.state.recording_state = initialize_recording_state(app.state.recording_state_path)
    app.state.overlay_state = apply_overlay_update(
        app.state.overlay_state,
        {"recording_state": overlay_recording_state(app.state.recording_state)},
    )

    app.state.db = None
    app.state.queue_store = None
    app.state.queue_recovery = {"recovered": []}
    if db_info is not None:
        app.state.db = WorkerDb(
            db_path=db_info.db_path,
            schema_version=db_info.schema_version,
            applied_migrations=db_info.applied_migrations,
        )
        app.state.queue_store = QueueStore(db_info.db_path)
        app.state.queue_recovery = app.state.queue_store.recover_startup()

    @app.get("/health")
    def health() -> dict[str, object]:
        return build_health_payload(paths=app.state.paths, config_loaded=app.state.config_loaded, db=app.state.db)

    @app.get("/version")
    def version() -> dict[str, object]:
        return {
            "version": __version__,
            "api_version": API_VERSION,
            "instance_id": INSTANCE_ID,
            "pid": PID,
            "started_at": STARTED_AT,
        }

    @app.get("/overlay/state")
    def get_overlay_state() -> dict[str, str]:
        return app.state.overlay_state.as_payload()

    @app.put("/overlay/state")
    async def put_overlay_state(request: Request):
        try:
            payload = await request.json()
            app.state.overlay_state = apply_overlay_update(app.state.overlay_state, payload)
        except OverlayPayloadError as exc:
            return JSONResponse(
                status_code=400,
                content=_error_payload(
                    code="overlay_payload_invalid",
                    message="Overlay payload is invalid",
                    details=exc.details,
                ),
            )
        except ValueError:
            return JSONResponse(
                status_code=400,
                content=_error_payload(code="overlay_payload_invalid", message="Overlay payload must be JSON object"),
            )
        return app.state.overlay_state.as_payload()

    @app.get("/recording/state")
    def get_recording_state() -> dict[str, str]:
        return app.state.recording_state.as_payload()

    @app.post("/recording/command")
    async def post_recording_command(request: Request):
        try:
            payload = await request.json()
            app.state.recording_state = apply_recording_command(app.state.recording_state, payload)
            save_recording_state(app.state.recording_state_path, app.state.recording_state)
            app.state.overlay_state = apply_overlay_update(
                app.state.overlay_state,
                {"recording_state": overlay_recording_state(app.state.recording_state)},
            )
        except RecordingCommandError as exc:
            status_code = 409 if exc.code == "recording_transition_invalid" else 400
            return JSONResponse(
                status_code=status_code,
                content=_error_payload(
                    code=exc.code,
                    message="Recording command is invalid",
                    details=exc.details,
                ),
            )
        except ValueError:
            return JSONResponse(
                status_code=400,
                content=_error_payload(code="recording_command_invalid", message="Recording command must be JSON object"),
            )
        return app.state.recording_state.as_payload()

    @app.get("/queue/recovery")
    def get_queue_recovery() -> dict[str, object]:
        return app.state.queue_recovery

    @app.get("/queue/items")
    def get_queue_items(state: str | None = None) -> dict[str, object]:
        if app.state.queue_store is None:
            return _error_response(
                status_code=503,
                code="queue_unavailable",
                message="Queue storage is unavailable",
            )
        try:
            items = app.state.queue_store.list_items(state=state)
        except QueueCommandError as exc:
            return _error_response(
                status_code=400,
                code=exc.code,
                message="Queue request is invalid",
                details=exc.details,
            )
        return {"items": [item.as_payload() for item in items]}

    @app.post("/queue/items")
    async def post_queue_item(request: Request):
        if app.state.queue_store is None:
            return _error_response(
                status_code=503,
                code="queue_unavailable",
                message="Queue storage is unavailable",
            )
        try:
            payload = await request.json()
            if not isinstance(payload, dict):
                raise ValueError
            item = app.state.queue_store.create_item(payload)
        except QueueCommandError as exc:
            return _error_response(
                status_code=400,
                code=exc.code,
                message="Queue payload is invalid",
                details=exc.details,
            )
        except ValueError:
            return _error_response(
                status_code=400,
                code="queue_payload_invalid",
                message="Queue payload must be JSON object",
            )
        return item.as_payload()

    @app.get("/queue/items/{item_id}")
    def get_queue_item(item_id: int):
        if app.state.queue_store is None:
            return _error_response(
                status_code=503,
                code="queue_unavailable",
                message="Queue storage is unavailable",
            )
        try:
            return app.state.queue_store.get_item(item_id).as_payload()
        except QueueCommandError as exc:
            return _error_response(
                status_code=404 if exc.code == "queue_item_not_found" else 400,
                code=exc.code,
                message="Queue request is invalid",
                details=exc.details,
            )

    @app.post("/queue/items/{item_id}/command")
    async def post_queue_item_command(item_id: int, request: Request):
        if app.state.queue_store is None:
            return _error_response(
                status_code=503,
                code="queue_unavailable",
                message="Queue storage is unavailable",
            )
        try:
            payload = await request.json()
            if not isinstance(payload, dict):
                raise ValueError
            item = app.state.queue_store.apply_command(item_id, payload)
        except QueueCommandError as exc:
            status_code = 409 if exc.code == "queue_transition_invalid" else 400
            if exc.code == "queue_item_not_found":
                status_code = 404
            return _error_response(
                status_code=status_code,
                code=exc.code,
                message="Queue command is invalid",
                details=exc.details,
            )
        except ValueError:
            return _error_response(
                status_code=400,
                code="queue_payload_invalid",
                message="Queue command must be JSON object",
            )
        return item.as_payload()

    @app.exception_handler(Exception)
    async def unhandled_exception_handler(request: Request, exc: Exception):
        logging.getLogger(__name__).exception("Unhandled exception", extra={"path": str(request.url.path)})
        return JSONResponse(
            status_code=500,
            content=_error_payload(code="internal_error", message="Internal server error"),
        )

    return app
