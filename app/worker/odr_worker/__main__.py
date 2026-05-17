from __future__ import annotations

import argparse
import logging
import sys
import time

from .config import get_repo_root, load_worker_config
from .health import RuntimePaths
from .logging_setup import init_worker_logging
from .runtime_dirs import RuntimeDirError, ensure_runtime_dirs
from .version import __version__


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="odr-worker")
    parser.add_argument("--version", action="store_true", help="Print worker version and exit")
    parser.add_argument("--host", help="Override bind host (default: config or 127.0.0.1)")
    parser.add_argument("--port", type=int, help="Override bind port (default: config or 8787)")
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

    loaded = load_worker_config(runtime_dirs.user_data_dir)
    host = args.host or loaded.config.host
    port = args.port or loaded.config.port

    log_path = init_worker_logging(runtime_dirs.logs_dir)
    logger = logging.getLogger(__name__)

    repo_root = get_repo_root().resolve()
    app_dir = (repo_root / "app").resolve()

    paths = RuntimePaths(
        app_dir=app_dir,
        user_data_dir=runtime_dirs.user_data_dir,
        config_dir=runtime_dirs.config_dir,
        data_dir=runtime_dirs.data_dir,
        logs_dir=runtime_dirs.logs_dir,
    )

    logger.info("Worker startup initialized")
    logger.info("version=%s", __version__)
    logger.info("host=%s port=%s", host, port)
    logger.info("config_path=%s config_loaded=%s", loaded.config_path, loaded.config_loaded)
    logger.info(
        "paths app_dir=%s user_data_dir=%s config_dir=%s data_dir=%s logs_dir=%s",
        paths.app_dir,
        paths.user_data_dir,
        paths.config_dir,
        paths.data_dir,
        paths.logs_dir,
    )

    print(
        "OBS Duel Recorder Worker (v0.2 scaffold)\n"
        "\n"
        f"- version: {__version__}\n"
        f"- bind: http://{host}:{port}\n"
        f"- user_data: {runtime_dirs.user_data_dir}\n"
        f"- log: {log_path}\n"
        "\n"
        "Endpoints:\n"
        "- GET /health\n"
    )

    try:
        import uvicorn  # type: ignore
        from .http_api import HealthState, create_app
    except Exception:  # pragma: no cover
        print(
            "Worker HTTP server dependencies are missing.\n"
            "Install dependencies: python -m pip install -r app/worker/requirements.txt\n",
            file=sys.stderr,
        )
        return 3

    health_state = HealthState(
        started_at_monotonic=time.monotonic(),
        version=__version__,
        config_loaded=loaded.config_loaded,
        runtime_dirs_ok=True,
        paths=paths,
    )
    app = create_app(health_state=health_state)

    uvicorn.run(app, host=host, port=port, log_level="info")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
