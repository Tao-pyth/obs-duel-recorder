from __future__ import annotations

import sys
import tempfile
import unittest
from datetime import datetime
from pathlib import Path


# Ensure `app/worker` is on sys.path so `import odr_worker` works without install.
WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


def _parse_started_at(value: object) -> None:
    if not isinstance(value, str):
        raise AssertionError("started_at must be a string")
    if not value.endswith("Z"):
        raise AssertionError("started_at must be in UTC (Z suffix)")
    datetime.fromisoformat(value.replace("Z", "+00:00"))


class HealthEndpointTests(unittest.TestCase):
    def test_health_payload_has_required_fields(self) -> None:
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            user_data_dir = Path(tmp)
            runtime_dirs = ensure_runtime_dirs(user_data_dir=user_data_dir)
            loaded_config = LoadedWorkerConfig(
                config=WorkerConfig(),
                config_path=runtime_dirs.config_dir / "worker.toml",
                config_loaded=False,
            )

            app = create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config)

            client = TestClient(app)
            resp = client.get("/health")
            self.assertEqual(resp.status_code, 200)

            data = resp.json()
            for key in (
                "status",
                "version",
                "api_version",
                "instance_id",
                "pid",
                "started_at",
                "uptime_seconds",
                "config_loaded",
                "runtime_dirs_ok",
                "paths",
            ):
                self.assertIn(key, data)

            self.assertIsInstance(data["pid"], int)
            _parse_started_at(data["started_at"])

            for path_key in (
                "app_dir",
                "user_data_dir",
                "config_dir",
                "data_dir",
                "logs_dir",
            ):
                self.assertIn(path_key, data["paths"])

    def test_version_endpoint_has_required_fields(self) -> None:
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            user_data_dir = Path(tmp)
            runtime_dirs = ensure_runtime_dirs(user_data_dir=user_data_dir)
            loaded_config = LoadedWorkerConfig(
                config=WorkerConfig(),
                config_path=runtime_dirs.config_dir / "worker.toml",
                config_loaded=False,
            )

            app = create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config)

            client = TestClient(app)
            resp = client.get("/version")
            self.assertEqual(resp.status_code, 200)

            data = resp.json()
            for key in ("version", "api_version", "instance_id", "pid", "started_at"):
                self.assertIn(key, data)

            self.assertIsInstance(data["pid"], int)
            _parse_started_at(data["started_at"])


if __name__ == "__main__":
    unittest.main()
