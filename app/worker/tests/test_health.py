from __future__ import annotations

import sys
import tempfile
import time
import unittest
from pathlib import Path


# Ensure `app/worker` is on sys.path so `import odr_worker` works without install.
WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class HealthEndpointTests(unittest.TestCase):
    def test_health_payload_has_required_fields(self) -> None:
        from fastapi.testclient import TestClient

        from odr_worker.health import RuntimePaths
        from odr_worker.http_api import HealthState, create_app
        from odr_worker.runtime_dirs import ensure_runtime_dirs
        from odr_worker.version import __version__

        with tempfile.TemporaryDirectory() as tmp:
            user_data_dir = Path(tmp)
            runtime_dirs = ensure_runtime_dirs(user_data_dir=user_data_dir)

            paths = RuntimePaths(
                app_dir=(WORKER_ROOT.parent / "app").resolve(),
                user_data_dir=runtime_dirs.user_data_dir,
                config_dir=runtime_dirs.config_dir,
                data_dir=runtime_dirs.data_dir,
                logs_dir=runtime_dirs.logs_dir,
            )

            state = HealthState(
                started_at_monotonic=time.monotonic(),
                version=__version__,
                config_loaded=False,
                runtime_dirs_ok=True,
                paths=paths,
            )
            app = create_app(health_state=state)

            client = TestClient(app)
            resp = client.get("/health")
            self.assertEqual(resp.status_code, 200)

            data = resp.json()
            for key in (
                "status",
                "version",
                "uptime_seconds",
                "config_loaded",
                "runtime_dirs_ok",
                "paths",
            ):
                self.assertIn(key, data)

            for path_key in (
                "app_dir",
                "user_data_dir",
                "config_dir",
                "data_dir",
                "logs_dir",
            ):
                self.assertIn(path_key, data["paths"])


if __name__ == "__main__":
    unittest.main()
