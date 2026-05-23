from __future__ import annotations

import logging
from pathlib import Path

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from .config import LoadedWorkerConfig, get_repo_root
from .db import DbInfo
from .detection import DetectionRuntime, TemplateConfigError, load_template_config, load_templates
from .exports import ExportError, ExportStore
from .health import API_VERSION, PID, STARTED_AT, WorkerDb, WorkerPaths, build_health_payload
from .identity import INSTANCE_ID
from .metadata import MatchMetadataStore, MetadataError
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
from .screenshots import ScreenshotError, ScreenshotStore
from .upload import UploadCommandError, UploadStore, build_upload_settings
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
    app.state.detection_config_error = None
    try:
        app.state.detection_config = load_template_config(runtime_dirs.user_data_dir)
        app.state.detection_runtime = DetectionRuntime(
            app.state.detection_config,
            load_templates(app.state.detection_config),
        )
    except TemplateConfigError as exc:
        app.state.detection_config = None
        app.state.detection_runtime = None
        app.state.detection_config_error = exc.details

    app.state.db = None
    app.state.queue_store = None
    app.state.queue_recovery = {"recovered": []}
    app.state.screenshot_store = None
    app.state.upload_store = None
    app.state.metadata_store = None
    app.state.export_store = None
    if db_info is not None:
        app.state.db = WorkerDb(
            db_path=db_info.db_path,
            schema_version=db_info.schema_version,
            applied_migrations=db_info.applied_migrations,
        )
        app.state.queue_store = QueueStore(db_info.db_path)
        app.state.queue_recovery = app.state.queue_store.recover_startup()
        app.state.screenshot_store = ScreenshotStore(
            db_path=db_info.db_path,
            screenshots_dir=runtime_dirs.screenshots_dir,
        )
        app.state.metadata_store = MatchMetadataStore(db_info.db_path)
        app.state.upload_store = UploadStore(
            queue_store=app.state.queue_store,
            videos_dir=runtime_dirs.videos_dir,
            settings=build_upload_settings(user_data_dir=runtime_dirs.user_data_dir),
            metadata_store=app.state.metadata_store,
        )
        app.state.export_store = ExportStore(db_path=db_info.db_path, runtime_dirs=runtime_dirs)

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

    @app.get("/detection/templates")
    def get_detection_templates() -> dict[str, object]:
        if app.state.detection_config is None:
            return {
                "config_loaded": False,
                "errors": [app.state.detection_config_error or {"config": "unavailable"}],
                "templates": [],
            }
        return app.state.detection_config.as_payload()

    @app.get("/detection/state")
    def get_detection_state() -> dict[str, object]:
        if app.state.detection_runtime is None:
            return _error_response(
                status_code=503,
                code="detection_unavailable",
                message="Detection runtime is unavailable",
                details=app.state.detection_config_error,
            )
        return app.state.detection_runtime.state.as_payload()

    @app.post("/detection/frame")
    async def post_detection_frame(request: Request):
        if app.state.detection_runtime is None:
            return _error_response(
                status_code=503,
                code="detection_unavailable",
                message="Detection runtime is unavailable",
                details=app.state.detection_config_error,
            )
        try:
            payload = await request.json()
            result, recording_state = app.state.detection_runtime.evaluate(payload, app.state.recording_state)
            if recording_state != app.state.recording_state:
                app.state.recording_state = recording_state
                save_recording_state(app.state.recording_state_path, app.state.recording_state)
                app.state.overlay_state = apply_overlay_update(
                    app.state.overlay_state,
                    {"recording_state": overlay_recording_state(app.state.recording_state)},
                )
        except TemplateConfigError as exc:
            return _error_response(
                status_code=400,
                code="detection_payload_invalid",
                message="Detection payload is invalid",
                details=exc.details,
            )
        except ValueError:
            return _error_response(
                status_code=400,
                code="detection_payload_invalid",
                message="Detection payload must be JSON object",
            )
        return {
            **result,
            "recording_state": app.state.recording_state.as_payload(),
        }

    @app.get("/matches")
    def get_matches(query: str | None = None) -> dict[str, object]:
        if app.state.metadata_store is None:
            return _error_response(
                status_code=503,
                code="metadata_unavailable",
                message="Match metadata storage is unavailable",
            )
        records = app.state.metadata_store.list_matches(query=query)
        return {"items": [record.as_payload() for record in records]}

    @app.post("/matches")
    async def post_match(request: Request):
        if app.state.metadata_store is None:
            return _error_response(
                status_code=503,
                code="metadata_unavailable",
                message="Match metadata storage is unavailable",
            )
        try:
            payload = await request.json()
            if not isinstance(payload, dict):
                raise ValueError
            return app.state.metadata_store.create_match(payload).as_payload()
        except MetadataError as exc:
            return _error_response(
                status_code=400,
                code=exc.code,
                message="Match metadata request is invalid",
                details=exc.details,
            )
        except ValueError:
            return _error_response(
                status_code=400,
                code="metadata_payload_invalid",
                message="Match metadata payload must be JSON object",
            )

    @app.get("/matches/{match_id}")
    def get_match(match_id: int):
        if app.state.metadata_store is None:
            return _error_response(
                status_code=503,
                code="metadata_unavailable",
                message="Match metadata storage is unavailable",
            )
        try:
            return app.state.metadata_store.get_match(match_id).as_payload()
        except MetadataError as exc:
            return _error_response(
                status_code=404 if exc.code == "match_not_found" else 400,
                code=exc.code,
                message="Match metadata request is invalid",
                details=exc.details,
            )

    @app.put("/matches/{match_id}/metadata")
    async def put_match_metadata(match_id: int, request: Request):
        if app.state.metadata_store is None:
            return _error_response(
                status_code=503,
                code="metadata_unavailable",
                message="Match metadata storage is unavailable",
            )
        try:
            payload = await request.json()
            if not isinstance(payload, dict):
                raise ValueError
            return app.state.metadata_store.update_match(match_id, payload).as_payload()
        except MetadataError as exc:
            return _error_response(
                status_code=404 if exc.code == "match_not_found" else 400,
                code=exc.code,
                message="Match metadata request is invalid",
                details=exc.details,
            )
        except ValueError:
            return _error_response(
                status_code=400,
                code="metadata_payload_invalid",
                message="Match metadata payload must be JSON object",
            )

    @app.get("/matches/{match_id}/upload-metadata")
    def get_match_upload_metadata(match_id: int):
        if app.state.metadata_store is None:
            return _error_response(
                status_code=503,
                code="metadata_unavailable",
                message="Match metadata storage is unavailable",
            )
        try:
            return app.state.metadata_store.render_upload_metadata(match_id)
        except MetadataError as exc:
            return _error_response(
                status_code=404 if exc.code == "match_not_found" else 400,
                code=exc.code,
                message="Match upload metadata is invalid",
                details=exc.details,
            )

    @app.get("/screenshots")
    def get_screenshots(match_id: int | None = None, queue_item_id: int | None = None) -> dict[str, object]:
        if app.state.screenshot_store is None:
            return _error_response(
                status_code=503,
                code="screenshots_unavailable",
                message="Screenshot storage is unavailable",
            )
        records = app.state.screenshot_store.list_records(match_id=match_id, queue_item_id=queue_item_id)
        return {"items": [record.as_payload() for record in records]}

    @app.post("/screenshots/capture")
    async def post_screenshot_capture(request: Request):
        if app.state.screenshot_store is None:
            return _error_response(
                status_code=503,
                code="screenshots_unavailable",
                message="Screenshot storage is unavailable",
            )
        try:
            payload = await request.json()
            if not isinstance(payload, dict):
                raise ValueError
            record = app.state.screenshot_store.capture(payload)
        except ScreenshotError as exc:
            return _error_response(
                status_code=400,
                code=exc.code,
                message="Screenshot request is invalid",
                details=exc.details,
            )
        except ValueError:
            return _error_response(
                status_code=400,
                code="screenshot_payload_invalid",
                message="Screenshot payload must be JSON object",
            )
        return record.as_payload()

    @app.get("/screenshots/{screenshot_id}")
    def get_screenshot(screenshot_id: int):
        if app.state.screenshot_store is None:
            return _error_response(
                status_code=503,
                code="screenshots_unavailable",
                message="Screenshot storage is unavailable",
            )
        try:
            return app.state.screenshot_store.get_record(screenshot_id).as_payload()
        except ScreenshotError as exc:
            return _error_response(
                status_code=404 if exc.code == "screenshot_not_found" else 400,
                code=exc.code,
                message="Screenshot request is invalid",
                details=exc.details,
            )

    @app.get("/screenshots/{screenshot_id}/preview")
    def get_screenshot_preview(screenshot_id: int):
        if app.state.screenshot_store is None:
            return _error_response(
                status_code=503,
                code="screenshots_unavailable",
                message="Screenshot storage is unavailable",
            )
        try:
            return app.state.screenshot_store.preview(screenshot_id)
        except ScreenshotError as exc:
            return _error_response(
                status_code=404 if exc.code == "screenshot_not_found" else 400,
                code=exc.code,
                message="Screenshot preview is unavailable",
                details=exc.details,
            )

    @app.post("/screenshots/cleanup")
    async def post_screenshot_cleanup(request: Request):
        if app.state.screenshot_store is None:
            return _error_response(
                status_code=503,
                code="screenshots_unavailable",
                message="Screenshot storage is unavailable",
            )
        try:
            payload = await request.json()
            if not isinstance(payload, dict) or not isinstance(payload.get("queue_item_id"), int):
                raise ValueError
            return app.state.screenshot_store.cleanup_for_queue_item(payload["queue_item_id"])
        except ScreenshotError as exc:
            return _error_response(
                status_code=404 if exc.code == "queue_item_not_found" else 400,
                code=exc.code,
                message="Screenshot cleanup is invalid",
                details=exc.details,
            )
        except ValueError:
            return _error_response(
                status_code=400,
                code="screenshot_cleanup_invalid",
                message="Screenshot cleanup payload must include integer queue_item_id",
            )

    @app.get("/upload/status")
    def get_upload_status() -> dict[str, object]:
        if app.state.upload_store is None:
            return _error_response(
                status_code=503,
                code="upload_unavailable",
                message="Upload storage is unavailable",
            )
        try:
            return app.state.upload_store.status()
        except QueueCommandError as exc:
            return _error_response(
                status_code=400,
                code=exc.code,
                message="Upload status is invalid",
                details=exc.details,
            )

    @app.post("/upload/process-next")
    async def post_upload_process_next(request: Request):
        if app.state.upload_store is None:
            return _error_response(
                status_code=503,
                code="upload_unavailable",
                message="Upload storage is unavailable",
            )
        try:
            payload = await request.json()
            if not isinstance(payload, dict):
                raise ValueError
            return app.state.upload_store.process_next(payload)
        except UploadCommandError as exc:
            return _error_response(
                status_code=400,
                code=exc.code,
                message="Upload request is invalid",
                details=exc.details,
            )
        except QueueCommandError as exc:
            status_code = 409 if exc.code == "queue_transition_invalid" else 400
            if exc.code == "queue_item_not_found":
                status_code = 404
            return _error_response(
                status_code=status_code,
                code=exc.code,
                message="Upload queue transition is invalid",
                details=exc.details,
            )
        except ValueError:
            return _error_response(
                status_code=400,
                code="upload_payload_invalid",
                message="Upload payload must be JSON object",
            )

    @app.get("/exports")
    def get_exports() -> dict[str, object]:
        if app.state.export_store is None:
            return _error_response(
                status_code=503,
                code="export_unavailable",
                message="Export storage is unavailable",
            )
        return {"items": [record.as_payload() for record in app.state.export_store.list_exports()]}

    @app.post("/exports")
    async def post_export(request: Request):
        if app.state.export_store is None:
            return _error_response(
                status_code=503,
                code="export_unavailable",
                message="Export storage is unavailable",
            )
        try:
            payload = await request.json()
            if not isinstance(payload, dict):
                raise ValueError
            return app.state.export_store.create_export(payload)
        except ExportError as exc:
            return _error_response(
                status_code=400,
                code=exc.code,
                message="Export request is invalid",
                details=exc.details,
            )
        except ValueError:
            return _error_response(
                status_code=400,
                code="export_payload_invalid",
                message="Export payload must be JSON object",
            )

    @app.exception_handler(Exception)
    async def unhandled_exception_handler(request: Request, exc: Exception):
        logging.getLogger(__name__).exception("Unhandled exception", extra={"path": str(request.url.path)})
        return JSONResponse(
            status_code=500,
            content=_error_payload(code="internal_error", message="Internal server error"),
        )

    return app
