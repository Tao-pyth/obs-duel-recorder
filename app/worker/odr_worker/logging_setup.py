from __future__ import annotations

import datetime as _dt
import logging
from pathlib import Path


def init_worker_logging(logs_dir: Path, level: int = logging.INFO) -> Path:
    """Initialize minimal Worker logging.

    v0.2 goals:
    - write logs under `user_data/logs/`
    - use per-startup-date log files
    - keep initialization idempotent (safe to call multiple times)
    """

    logs_dir = logs_dir.expanduser().resolve()
    logs_dir.mkdir(parents=True, exist_ok=True)

    log_name = f"worker-{_dt.date.today().isoformat()}.log"
    log_path = (logs_dir / log_name).resolve()

    root = logging.getLogger()
    root.setLevel(level)

    for handler in root.handlers:
        if isinstance(handler, logging.FileHandler):
            existing = getattr(handler, "baseFilename", None)
            if existing and Path(existing).resolve() == log_path:
                return log_path

    file_handler = logging.FileHandler(log_path, encoding="utf-8")
    file_handler.setLevel(level)
    file_handler.setFormatter(
        logging.Formatter(
            fmt="%(asctime)s %(levelname)s %(name)s: %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        )
    )
    root.addHandler(file_handler)

    if not any(isinstance(h, logging.StreamHandler) for h in root.handlers):
        stream_handler = logging.StreamHandler()
        stream_handler.setLevel(level)
        stream_handler.setFormatter(logging.Formatter(fmt="%(levelname)s %(name)s: %(message)s"))
        root.addHandler(stream_handler)

    return log_path
