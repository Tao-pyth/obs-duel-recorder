from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class SetupWizardApiTests(unittest.TestCase):
    def _client(self):
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig
        from odr_worker.db import init_db
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        runtime_dirs = ensure_runtime_dirs(user_data_dir=Path(tmp.name))
        db_info = init_db(runtime_dirs=runtime_dirs)
        loaded_config = LoadedWorkerConfig(
            config=WorkerConfig(),
            config_path=runtime_dirs.config_dir / "worker.toml",
            config_loaded=False,
        )
        client = TestClient(create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config, db_info=db_info))
        self.addCleanup(client.close)
        return client, runtime_dirs

    def test_first_run_partial_complete_and_restart_persistence(self) -> None:
        client, runtime_dirs = self._client()

        first = client.get("/setup/status")
        self.assertEqual(first.status_code, 200)
        self.assertEqual(first.json()["status"], "first_run")
        self.assertTrue(first.json()["first_run"])
        self.assertEqual(first.json()["current_step"], "runtime_path")

        partial = client.post("/setup/steps/runtime_path/complete", json={})
        self.assertEqual(partial.status_code, 200)
        self.assertEqual(partial.json()["status"], "partial")
        self.assertEqual(partial.json()["current_step"], "obs_integration")

        for step in ("obs_integration", "oauth", "templates"):
            completed = client.post(f"/setup/steps/{step}/complete", json={})
            self.assertEqual(completed.status_code, 200)
        self.assertEqual(completed.json()["status"], "complete")
        self.assertEqual(completed.json()["current_step"], "")

        second_client = self._client_for_runtime(runtime_dirs)
        persisted = second_client.get("/setup/status")
        self.assertEqual(persisted.status_code, 200)
        self.assertEqual(persisted.json()["status"], "complete")
        self.assertFalse(persisted.json()["first_run"])

    def test_cancel_and_reset_preserve_runtime_data(self) -> None:
        client, runtime_dirs = self._client()
        marker = runtime_dirs.db_dir / "preserve.db"
        marker.write_text("keep", encoding="utf-8")
        client.post("/setup/steps/runtime_path/complete", json={})

        cancelled = client.post("/setup/cancel")
        self.assertEqual(cancelled.status_code, 200)
        self.assertEqual(cancelled.json()["status"], "partial")
        self.assertTrue(cancelled.json()["cancelled_at"])
        self.assertTrue(marker.exists())

        reset = client.post("/setup/reset")
        self.assertEqual(reset.status_code, 200)
        self.assertEqual(reset.json()["status"], "first_run")
        self.assertEqual(reset.json()["reset_count"], 1)
        self.assertTrue(marker.exists())

    def test_validate_reports_runtime_oauth_template_and_obs_steps(self) -> None:
        client, runtime_dirs = self._client()
        validations = client.post("/setup/validate")

        self.assertEqual(validations.status_code, 200)
        body = validations.json()["validations"]
        self.assertEqual(body["runtime_path"]["status"], "ready")
        self.assertEqual(body["runtime_path"]["code"], "runtime_path_ready")
        self.assertTrue(body["runtime_path"]["existing_runtime_data"])
        self.assertEqual(body["obs_integration"]["status"], "ready")
        self.assertEqual(body["obs_integration"]["code"], "worker_api_compatible")
        self.assertEqual(body["oauth"]["status"], "action_required")
        self.assertEqual(body["oauth"]["code"], "oauth_action_required")
        self.assertEqual(body["templates"]["status"], "action_required")
        self.assertEqual(body["templates"]["code"], "templates_action_required")

        secrets_dir = runtime_dirs.config_dir / "secrets"
        secrets_dir.mkdir(parents=True, exist_ok=True)
        (secrets_dir / "youtube-client-secret.json").write_text("{}", encoding="utf-8")
        (secrets_dir / "youtube-token.json").write_text("{}", encoding="utf-8")
        templates_dir = runtime_dirs.user_data_dir / "templates"
        templates_dir.mkdir(parents=True, exist_ok=True)
        (templates_dir / "start.bin").write_text("start", encoding="utf-8")
        (runtime_dirs.config_dir / "templates.toml").write_text(
            """
[[templates]]
name = "start"
kind = "duel_start"
path = "start.bin"
""",
            encoding="utf-8",
        )

        ready_client = self._client_for_runtime(runtime_dirs)
        ready = ready_client.post("/setup/validate").json()["validations"]
        self.assertEqual(ready["oauth"]["status"], "ready")
        self.assertEqual(ready["oauth"]["code"], "oauth_ready")
        self.assertEqual(ready["templates"]["status"], "ready")
        self.assertEqual(ready["templates"]["code"], "templates_ready")

    def test_validate_reports_runtime_path_failure_with_stable_code(self) -> None:
        client, runtime_dirs = self._client()
        runtime_dirs.videos_dir.rmdir()

        validations = client.post("/setup/validate")
        self.assertEqual(validations.status_code, 200)
        runtime_path = validations.json()["validations"]["runtime_path"]
        self.assertEqual(runtime_path["status"], "action_required")
        self.assertEqual(runtime_path["code"], "runtime_path_action_required")
        self.assertIn(
            {"code": "directory_missing", "label": "videos", "path": runtime_dirs.videos_dir.as_posix()},
            runtime_path["diagnostics"],
        )

    def test_invalid_step_and_invalid_state_are_actionable(self) -> None:
        client, runtime_dirs = self._client()

        invalid_step = client.post("/setup/steps/not-a-step/complete", json={})
        self.assertEqual(invalid_step.status_code, 400)
        self.assertEqual(invalid_step.json()["code"], "setup_step_unknown")

        state_path = runtime_dirs.data_dir / "setup-wizard.json"
        state_path.write_text(json.dumps({"completed_steps": ["bad-step"]}), encoding="utf-8")
        invalid_state = client.get("/setup/status")
        self.assertEqual(invalid_state.status_code, 400)
        self.assertEqual(invalid_state.json()["code"], "setup_state_invalid")

    def _client_for_runtime(self, runtime_dirs):
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig
        from odr_worker.db import init_db

        db_info = init_db(runtime_dirs=runtime_dirs)
        loaded_config = LoadedWorkerConfig(
            config=WorkerConfig(),
            config_path=runtime_dirs.config_dir / "worker.toml",
            config_loaded=False,
        )
        client = TestClient(create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config, db_info=db_info))
        self.addCleanup(client.close)
        return client


if __name__ == "__main__":
    unittest.main()
