from __future__ import annotations

import logging
from pathlib import Path

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from .config import LoadedWorkerConfig, get_repo_root
from .health import WorkerPaths, build_health_payload
from .runtime_dirs import RuntimeDirs


def _error_payload(*, code: str, message: str, details: object | None = None) -> dict[str, object]:
    payload: dict[str, object] = {"code": code, "message": message}
    if details is not None:
        payload["details"] = details
    return payload


def create_app(*, runtime_dirs: RuntimeDirs, loaded_config: LoadedWorkerConfig) -> FastAPI:
    app = FastAPI(title="OBS Duel Recorder Worker", version="0.2.0-dev")

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

    @app.get("/health")
    def health() -> dict[str, object]:
        return build_health_payload(paths=app.state.paths, config_loaded=app.state.config_loaded)

    @app.exception_handler(Exception)
    async def unhandled_exception_handler(request: Request, exc: Exception):
        logging.getLogger(__name__).exception("Unhandled exception", extra={"path": str(request.url.path)})
        return JSONResponse(
            status_code=500,
            content=_error_payload(code="internal_error", message="Internal server error"),
        )

    return app
