from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class RuntimeDirsTests(unittest.TestCase):
    def test_ensure_runtime_dirs_creates_expected_layout(self) -> None:
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dirs = ensure_runtime_dirs(user_data_dir=root)

            self.assertTrue(dirs.user_data_dir.is_dir())
            self.assertTrue(dirs.config_dir.is_dir())
            self.assertTrue(dirs.data_dir.is_dir())
            self.assertTrue(dirs.logs_dir.is_dir())

            self.assertTrue(dirs.db_dir.is_dir())
            self.assertTrue(dirs.videos_dir.is_dir())
            self.assertTrue(dirs.screenshots_dir.is_dir())
            self.assertTrue(dirs.exports_dir.is_dir())


class WorkerConfigTests(unittest.TestCase):
    def test_worker_config_loads_and_saves_upload_privacy_status(self) -> None:
        from odr_worker.config import WorkerConfig, load_worker_config, save_worker_config
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dirs = ensure_runtime_dirs(user_data_dir=root)

            saved_path = save_worker_config(
                user_data_dir=root,
                config=WorkerConfig(host="127.0.0.1", port=8787, upload_privacy_status="unlisted"),
            )
            loaded = load_worker_config(user_data_dir=root)

            self.assertEqual(saved_path.name, "worker.toml")
            self.assertTrue(saved_path.exists())
            self.assertTrue(loaded.config_loaded)
            self.assertEqual(loaded.config.upload_privacy_status, "unlisted")

    def test_invalid_upload_privacy_status_returns_clear_error(self) -> None:
        from odr_worker.config import WorkerConfigError, load_worker_config
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dirs = ensure_runtime_dirs(user_data_dir=root)
            config_path = dirs.config_dir / "worker.toml"
            config_path.write_text('[upload]\nprivacy_status = "public"\n', encoding="utf-8")

            with self.assertRaisesRegex(WorkerConfigError, "privacy_status"):
                load_worker_config(user_data_dir=root)

    def test_invalid_config_returns_clear_error(self) -> None:
        from odr_worker.config import WorkerConfigError, load_worker_config
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dirs = ensure_runtime_dirs(user_data_dir=root)
            config_path = dirs.config_dir / "worker.toml"
            config_path.write_text("[worker]\nport = \"not-a-port\"\n", encoding="utf-8")

            with self.assertRaisesRegex(WorkerConfigError, "Failed to load Worker config"):
                load_worker_config(user_data_dir=root)


if __name__ == "__main__":
    unittest.main()
