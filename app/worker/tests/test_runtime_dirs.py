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


if __name__ == "__main__":
    unittest.main()
