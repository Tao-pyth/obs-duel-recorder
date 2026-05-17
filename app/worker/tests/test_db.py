from __future__ import annotations

import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path


# Ensure `app/worker` is on sys.path so `import odr_worker` works without install.
WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class SqliteFoundationTests(unittest.TestCase):
    def test_init_db_creates_db_and_tables(self) -> None:
        from odr_worker.db import init_db
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            user_data_dir = Path(tmp)
            runtime_dirs = ensure_runtime_dirs(user_data_dir=user_data_dir)

            db_info = init_db(runtime_dirs=runtime_dirs)
            self.assertTrue(db_info.db_path.exists())
            self.assertGreaterEqual(db_info.schema_version, 2)
            self.assertEqual(db_info.applied_migrations, ("0001_init", "0002_tables"))

            conn = sqlite3.connect(db_info.db_path)
            try:
                tables = {
                    row[0]
                    for row in conn.execute("SELECT name FROM sqlite_master WHERE type='table';").fetchall()
                }
                self.assertIn("matches", tables)
                self.assertIn("upload_queue", tables)

                conn.execute("INSERT INTO matches DEFAULT VALUES;")
                match_id = conn.execute("SELECT id FROM matches ORDER BY id DESC LIMIT 1;").fetchone()[0]
                conn.execute("INSERT INTO upload_queue(match_id, state) VALUES(?, ?);", (match_id, "ready_upload"))
                conn.commit()

                row = conn.execute("SELECT state FROM upload_queue WHERE match_id = ?;", (match_id,)).fetchone()
                self.assertEqual(row[0], "ready_upload")
            finally:
                conn.close()

    def test_init_db_is_restart_safe(self) -> None:
        from odr_worker.db import init_db
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            user_data_dir = Path(tmp)
            runtime_dirs = ensure_runtime_dirs(user_data_dir=user_data_dir)

            first = init_db(runtime_dirs=runtime_dirs)
            second = init_db(runtime_dirs=runtime_dirs)

            self.assertEqual(first.db_path, second.db_path)
            self.assertEqual(first.schema_version, second.schema_version)
            self.assertEqual(first.applied_migrations, second.applied_migrations)


if __name__ == "__main__":
    unittest.main()
