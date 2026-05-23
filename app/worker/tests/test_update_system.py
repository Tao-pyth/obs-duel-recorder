from __future__ import annotations

import json
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class UpdateSystemTests(unittest.TestCase):
    def _runtime(self):
        from odr_worker.db import init_db
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        runtime_dirs = ensure_runtime_dirs(user_data_dir=Path(tmp.name))
        init_db(runtime_dirs=runtime_dirs)
        return runtime_dirs

    def test_validate_detects_versions_and_rejects_unsupported_paths(self) -> None:
        from odr_worker.update_system import UpdateManager

        runtime_dirs = self._runtime()
        manager = UpdateManager(runtime_dirs=runtime_dirs)

        downgrade = manager.validate({"current_version": "2.1.0", "target_version": "2.0.0"})
        self.assertFalse(downgrade["valid"])
        self.assertEqual(downgrade["errors"][0]["code"], "downgrade_unsupported")

        api_mismatch = manager.validate({"current_version": "2.0.0", "expected_api_version": "2.0"})
        self.assertFalse(api_mismatch["valid"])
        self.assertEqual(api_mismatch["errors"][0]["code"], "api_version_mismatch")

    def test_apply_creates_db_backup_records_completion_and_preserves_runtime(self) -> None:
        from odr_worker.update_system import UpdateManager

        runtime_dirs = self._runtime()
        preserved = [
            runtime_dirs.config_dir / "secrets" / "youtube-token.json",
            runtime_dirs.videos_dir / "duel.mp4",
            runtime_dirs.screenshots_dir / "shot.txt",
            runtime_dirs.exports_dir / "export.zip",
            runtime_dirs.logs_dir / "worker.log",
        ]
        for path in preserved:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"keep")

        manager = UpdateManager(runtime_dirs=runtime_dirs)
        result = manager.apply_update(
            {"current_version": "1.3.0", "created_at": "2026-05-23T13:00:00Z"}
        )

        self.assertEqual(result["status"], "completed")
        backup_path = Path(str(result["backup_path"]))
        self.assertTrue(backup_path.exists())
        self.assertIn("odr-20260523T130000Z-2.1.0.sqlite3", backup_path.name)
        for path in preserved:
            self.assertTrue(path.exists(), path)

        state = json.loads((runtime_dirs.data_dir / "update-state.json").read_text(encoding="utf-8"))
        installed = json.loads((runtime_dirs.data_dir / "installed-version.json").read_text(encoding="utf-8"))
        self.assertEqual(state["status"], "completed")
        self.assertEqual(installed["version"], "2.1.0")

    def test_apply_records_failure_after_backup_when_migration_fails(self) -> None:
        from odr_worker.db import DbInitError
        from odr_worker.update_system import UpdateError, UpdateManager

        runtime_dirs = self._runtime()
        marker = runtime_dirs.videos_dir / "preserved.mp4"
        marker.write_bytes(b"keep")
        manager = UpdateManager(runtime_dirs=runtime_dirs)

        with mock.patch("odr_worker.update_system.init_db", side_effect=DbInitError("boom")):
            with self.assertRaises(UpdateError) as ctx:
                manager.apply_update({"current_version": "1.3.0", "created_at": "2026-05-23T13:01:00Z"})

        self.assertEqual(ctx.exception.code, "update_migration_failed")
        state = json.loads((runtime_dirs.data_dir / "update-state.json").read_text(encoding="utf-8"))
        self.assertEqual(state["status"], "failed")
        self.assertTrue(Path(state["backup_path"]).exists())
        self.assertTrue(marker.exists())

    def test_status_detects_partial_update_state(self) -> None:
        from odr_worker.update_system import UpdateManager

        runtime_dirs = self._runtime()
        state_path = runtime_dirs.data_dir / "update-state.json"
        state_path.write_text('{"status":"in_progress"}\n', encoding="utf-8")

        status = UpdateManager(runtime_dirs=runtime_dirs).status()

        self.assertEqual(status["status"], "in_progress")
        self.assertTrue(status["partial_update_detected"])

    def test_api_exposes_update_status_validate_and_apply(self) -> None:
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig
        from odr_worker.db import init_db
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            runtime_dirs = ensure_runtime_dirs(user_data_dir=Path(tmp))
            db_info = init_db(runtime_dirs=runtime_dirs)
            loaded_config = LoadedWorkerConfig(
                config=WorkerConfig(),
                config_path=runtime_dirs.config_dir / "worker.toml",
                config_loaded=False,
            )
            client = TestClient(create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config, db_info=db_info))
            self.addCleanup(client.close)

            status = client.get("/update/status")
            self.assertEqual(status.status_code, 200)
            self.assertEqual(status.json()["target_version"], "2.1.0")

            invalid = client.post("/update/validate", json={"expected_api_version": "2.0"})
            self.assertEqual(invalid.status_code, 200)
            self.assertFalse(invalid.json()["valid"])

            applied = client.post(
                "/update/apply",
                json={"current_version": "1.3.0", "created_at": "2026-05-23T13:02:00Z"},
            )
            self.assertEqual(applied.status_code, 200)
            self.assertEqual(applied.json()["status"], "completed")

    def test_backup_is_valid_sqlite_copy(self) -> None:
        from odr_worker.update_system import UpdateManager

        runtime_dirs = self._runtime()
        manager = UpdateManager(runtime_dirs=runtime_dirs)
        result = manager.apply_update(
            {"current_version": "1.3.0", "created_at": "2026-05-23T13:03:00Z"}
        )

        conn = sqlite3.connect(Path(str(result["backup_path"])))
        try:
            row = conn.execute("SELECT value FROM odr_meta WHERE key = 'schema_version';").fetchone()
        finally:
            conn.close()
        self.assertIsNotNone(row)


if __name__ == "__main__":
    unittest.main()
