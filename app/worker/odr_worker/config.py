from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import os
import tomllib


class WorkerConfigError(RuntimeError):
    """Raised when the Worker config file cannot be parsed or validated."""


@dataclass(frozen=True)
class WorkerConfig:
    """Worker configuration scaffold (v0.2).

    v0.2 intentionally keeps configuration minimal. Later issues can extend this
    structure once the Worker runtime behavior is implemented.
    """

    host: str = "127.0.0.1"
    port: int = 8787


@dataclass(frozen=True)
class LoadedWorkerConfig:
    config: WorkerConfig
    config_path: Path
    config_loaded: bool


def get_repo_root() -> Path:
    """Return the repository root directory.

    Assumption (v0.2 scaffold): this file lives at:
    `<repo>/app/worker/odr_worker/config.py`.

    This is a scaffold helper; if the layout changes later, update this logic.
    """

    return Path(__file__).resolve().parents[3]


def get_user_data_dir() -> Path:
    """Return the runtime user_data directory.

    Default: `<repo>/user_data/`

    Override (optional): set `ODR_USER_DATA_DIR` to an absolute path.
    """

    override = os.getenv("ODR_USER_DATA_DIR")
    if override:
        return Path(override).expanduser().resolve()

    return get_repo_root() / "user_data"


def get_default_config_path(user_data_dir: Path | None = None) -> Path:
    base_dir = user_data_dir or get_user_data_dir()
    return base_dir / "config" / "worker.toml"


def load_worker_config(user_data_dir: Path | None = None) -> LoadedWorkerConfig:
    """Load Worker configuration from `user_data/config/worker.toml`.

    - Missing config file is allowed and returns defaults.
    - Secrets MUST NOT be stored in git; see `AGENTS.md`.
    """

    config_path = get_default_config_path(user_data_dir)

    if not config_path.exists():
        return LoadedWorkerConfig(config=WorkerConfig(), config_path=config_path, config_loaded=False)

    try:
        raw = tomllib.loads(config_path.read_text(encoding="utf-8"))
        config_table = raw.get("worker", {})
        if not isinstance(config_table, dict):
            raise ValueError("[worker] must be a TOML table")

        host = str(config_table.get("host", WorkerConfig.host))
        port = int(config_table.get("port", WorkerConfig.port))
    except (OSError, tomllib.TOMLDecodeError, TypeError, ValueError) as exc:
        raise WorkerConfigError(f"Failed to load Worker config from {config_path}: {exc}") from exc

    return LoadedWorkerConfig(
        config=WorkerConfig(host=host, port=port),
        config_path=config_path,
        config_loaded=True,
    )
