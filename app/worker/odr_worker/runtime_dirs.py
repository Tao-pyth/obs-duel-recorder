from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .config import get_user_data_dir


class RuntimeDirError(RuntimeError):
    pass


@dataclass(frozen=True)
class RuntimeDirs:
    user_data_dir: Path
    config_dir: Path
    data_dir: Path
    logs_dir: Path
    db_dir: Path
    videos_dir: Path
    screenshots_dir: Path
    exports_dir: Path


def _mkdir_or_raise(path: Path, label: str) -> None:
    try:
        path.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        raise RuntimeDirError(f"Failed to create {label} directory: {path}") from exc

    if not path.is_dir():
        raise RuntimeDirError(f"Expected {label} to be a directory: {path}")


def ensure_runtime_dirs(user_data_dir: Path | None = None) -> RuntimeDirs:
    """Ensure v0.2 runtime directories exist.

    This is intentionally restart-safe and idempotent:
    - creates directories if missing
    - never deletes or overwrites existing runtime data

    Default runtime root is `<repo>/user_data/`, but can be overridden using
    `ODR_USER_DATA_DIR` (see `odr_worker.config.get_user_data_dir`).
    """

    root = (user_data_dir or get_user_data_dir()).expanduser().resolve()

    config_dir = root / "config"
    data_dir = root / "data"
    logs_dir = root / "logs"

    db_dir = data_dir / "db"
    videos_dir = data_dir / "videos"
    screenshots_dir = data_dir / "screenshots"
    exports_dir = data_dir / "exports"

    _mkdir_or_raise(root, "user_data")
    _mkdir_or_raise(config_dir, "config")
    _mkdir_or_raise(data_dir, "data")
    _mkdir_or_raise(logs_dir, "logs")

    _mkdir_or_raise(db_dir, "db")
    _mkdir_or_raise(videos_dir, "videos")
    _mkdir_or_raise(screenshots_dir, "screenshots")
    _mkdir_or_raise(exports_dir, "exports")

    return RuntimeDirs(
        user_data_dir=root,
        config_dir=config_dir,
        data_dir=data_dir,
        logs_dir=logs_dir,
        db_dir=db_dir,
        videos_dir=videos_dir,
        screenshots_dir=screenshots_dir,
        exports_dir=exports_dir,
    )
