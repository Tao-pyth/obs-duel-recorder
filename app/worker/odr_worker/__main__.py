from __future__ import annotations

import argparse
import sys

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

    print(
        "OBS Duel Recorder Worker (v0.2 scaffold)\n"
        "\n"
        "This is a placeholder entrypoint added by the v0.2 skeleton issue.\n"
        "Implementation is tracked by issues #11-#16.\n"
        "\n"
        f"Runtime directories ensured under: {runtime_dirs.user_data_dir}\n"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
