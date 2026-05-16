from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Any

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import JSONResponse

from .health import RuntimePaths, build_health_payload


@dataclass(frozen=True)
class HealthState:
    started_at_monotonic: float
    version: str
    config_loaded: bool
    runtime_dirs_ok: bool
    paths: RuntimePaths


def _error(code: str, message: str, details: dict[str, Any] | None = None) -> dict[str, Any]:
    payload: dict[str, Any] = {"code": code, "message": message}
    if details is not None:
        payload["details"] = details
    return payload


def create_app(*, health_state: HealthState) -> FastAPI:
    app = FastAPI(title="OBS Duel Recorder Worker", version=health_state.version)

    @app.exception_handler(HTTPException)
    async def http_exception_handler(_: Request, exc: HTTPException) -> JSONResponse:
        detail = exc.detail
        details: dict[str, Any] | None
        if isinstance(detail, dict):
            details = detail
        else:
            details = {"detail": detail}
        return JSONResponse(
            status_code=exc.status_code,
            content=_error(code="http_error", message=str(detail), details=details),
        )

    @app.exception_handler(Exception)
    async def unhandled_exception_handler(_: Request, exc: Exception) -> JSONResponse:
        return JSONResponse(
            status_code=500,
            content=_error(code="internal_error", message="internal server error"),
        )

    @app.get("/health")
    async def get_health() -> dict[str, Any]:
        uptime = max(0.0, time.monotonic() - health_state.started_at_monotonic)
        status = "ok" if health_state.runtime_dirs_ok else "degraded"
        return build_health_payload(
            status=status,
            version=health_state.version or "unknown",
            uptime_seconds=uptime,
            config_loaded=health_state.config_loaded,
            runtime_dirs_ok=health_state.runtime_dirs_ok,
            paths=health_state.paths,
        )

    return app
