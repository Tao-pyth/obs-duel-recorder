from __future__ import annotations

import argparse
import logging
import sys

import uvicorn

from .api import create_app
from .config import load_worker_config
from .logging_setup import init_worker_logging
from .runtime_dirs import RuntimeDirError, ensure_runtime_dirs
from .version import __version__


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="odr-worker")
    parser.add_argument("--version", action="store_true", help="Print worker version and exit")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.version:
        print(__version__)
        return 0

    try:
        runtime_dirs = ensure_runtime_dirs()
    except RuntimeDirError as exc:
        print(f"Runtime directory initialization failed: {exc}", file=sys.stderr)
        return 2

    loaded_config = load_worker_config(runtime_dirs.user_data_dir)

    log_path = init_worker_logging(runtime_dirs.logs_dir)
    logger = logging.getLogger(__name__)

    logger.info("Worker startup initialized")
    logger.info("version=%s", __version__)
    logger.info("config_loaded=%s config_path=%s", loaded_config.config_loaded, loaded_config.config_path)
    logger.info(
        "paths app_dir=%s user_data_dir=%s config_dir=%s data_dir=%s logs_dir=%s",
        (runtime_dirs.user_data_dir.parent / "app").resolve(),
        runtime_dirs.user_data_dir,
        runtime_dirs.config_dir,
        runtime_dirs.data_dir,
        runtime_dirs.logs_dir,
    )

    app = create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config)

    logger.info("Starting API server host=%s port=%s", loaded_config.config.host, loaded_config.config.port)
    logger.info("logs=%s", log_path)

    uvicorn.run(
        app,
        host=loaded_config.config.host,
        port=loaded_config.config.port,
        log_config=None,
        access_log=False,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
