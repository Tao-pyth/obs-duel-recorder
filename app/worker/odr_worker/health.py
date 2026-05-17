from __future__ import annotations

import time
from dataclasses import dataclass
from pathlib import Path

from .version import __version__


API_VERSION = "0.2"
_START_TIME = time.monotonic()


@dataclass(frozen=True)
class WorkerPaths:
    app_dir: Path
    user_data_dir: Path
    config_dir: Path
    data_dir: Path
    logs_dir: Path


def _as_posix(path: Path) -> str:
    return path.resolve().as_posix()


def build_health_payload(*, paths: WorkerPaths, config_loaded: bool) -> dict[str, object]:
    uptime_seconds = int(max(0.0, time.monotonic() - _START_TIME))

    return {
        "status": "ok",
        "version": __version__,
        "api_version": API_VERSION,
        "uptime_seconds": uptime_seconds,
        "config_loaded": config_loaded,
        "runtime_dirs_ok": True,
        "paths": {
            "app_dir": _as_posix(paths.app_dir),
            "user_data_dir": _as_posix(paths.user_data_dir),
            "config_dir": _as_posix(paths.config_dir),
            "data_dir": _as_posix(paths.data_dir),
            "logs_dir": _as_posix(paths.logs_dir),
        },
    }
