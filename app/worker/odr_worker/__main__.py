from __future__ import annotations

import argparse
import logging
import sys

from .config import (
    LoadedWorkerConfig,
    WorkerConfig,
    WorkerConfigError,
    get_default_config_path,
    load_worker_config,
)
from .db import DbInitError, init_db
from .logging_setup import init_worker_logging
from .runtime_dirs import RuntimeDirError, ensure_runtime_dirs
from .version import __version__


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="odr-worker")
    parser.add_argument("--version", action="store_true", help="Print worker version and exit")
    parser.add_argument("--host", help="Override bind host (default: config or 127.0.0.1)")
    parser.add_argument("--port", type=int, help="Override bind port (default: config or 8787)")
    return parser


def _apply_cli_overrides(loaded_config: LoadedWorkerConfig, args: argparse.Namespace) -> LoadedWorkerConfig:
    if args.host is None and args.port is None:
        return loaded_config

    config = WorkerConfig(
        host=args.host or loaded_config.config.host,
        port=args.port if args.port is not None else loaded_config.config.port,
    )
    return LoadedWorkerConfig(
        config=config,
        config_path=loaded_config.config_path,
        config_loaded=loaded_config.config_loaded,
    )


def main(argv: list[str] | None = None) -> int:
    if argv is None:
        argv = sys.argv[1:]

    if argv and argv[0] == "update":
        from .update_system import main as update_main

        return update_main(argv[1:])

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

    log_path = init_worker_logging(runtime_dirs.logs_dir)
    logger = logging.getLogger(__name__)

    try:
        loaded_config = _apply_cli_overrides(load_worker_config(runtime_dirs.user_data_dir), args)
    except WorkerConfigError as exc:
        config_path = get_default_config_path(runtime_dirs.user_data_dir)
        logger.error("Failed to load worker config: %s", exc, extra={"config_path": str(config_path)})
        print(f"Failed to load Worker config: {config_path}\nReason: {exc}", file=sys.stderr)
        print(f"Logs: {log_path}", file=sys.stderr)
        return 3

    try:
        db_info = init_db(runtime_dirs=runtime_dirs, logger=logger)
    except DbInitError as exc:
        logger.error("SQLite initialization failed: %s", exc, extra={"db_dir": str(runtime_dirs.db_dir)})
        print(f"SQLite initialization failed: {exc}", file=sys.stderr)
        print(f"Logs: {log_path}", file=sys.stderr)
        return 3

    try:
        import uvicorn  # type: ignore
        from .api import create_app
    except Exception:  # pragma: no cover
        print(
            "Worker HTTP server dependencies are missing.\n"
            "Install dependencies: python -m pip install -e app/worker\n",
            file=sys.stderr,
        )
        return 3

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

    app = create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config, db_info=db_info)

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
