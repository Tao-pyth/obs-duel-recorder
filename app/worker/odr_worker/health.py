from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class RuntimePaths:
    app_dir: Path
    user_data_dir: Path
    config_dir: Path
    data_dir: Path
    logs_dir: Path

    def as_json(self) -> dict[str, str]:
        return {
            "app_dir": str(self.app_dir),
            "user_data_dir": str(self.user_data_dir),
            "config_dir": str(self.config_dir),
            "data_dir": str(self.data_dir),
            "logs_dir": str(self.logs_dir),
        }


def build_health_payload(
    *,
    status: str,
    version: str,
    uptime_seconds: float,
    config_loaded: bool,
    runtime_dirs_ok: bool,
    paths: RuntimePaths,
    details: dict[str, Any] | None = None,
) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "status": status,
        "version": version,
        "uptime_seconds": uptime_seconds,
        "config_loaded": config_loaded,
        "runtime_dirs_ok": runtime_dirs_ok,
        "paths": paths.as_json(),
    }
    if details:
        payload["details"] = details
    return payload
