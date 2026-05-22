from __future__ import annotations

import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


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
            self.assertGreaterEqual(db_info.schema_version, 3)
            self.assertEqual(db_info.applied_migrations, ("0001_init", "0002_tables", "0003_queue_recovery"))

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
                conn.execute(
                    "INSERT INTO upload_queue(match_id, state, video_path) VALUES(?, ?, ?);",
                    (match_id, "ready_upload", "duel.mp4"),
                )
                conn.commit()

                row = conn.execute(
                    "SELECT state, retry_count, manual_review_evidence_json FROM upload_queue WHERE match_id = ?;",
                    (match_id,),
                ).fetchone()
                self.assertEqual(row[0], "ready_upload")
                self.assertEqual(row[1], 0)
                self.assertEqual(row[2], "{}")
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

    def test_init_db_wraps_sqlite_open_failures(self) -> None:
        from odr_worker.db import DbInitError, init_db
        from odr_worker.runtime_dirs import RuntimeDirs

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            not_a_dir = root / "not-a-dir"
            not_a_dir.write_text("x", encoding="utf-8")

            runtime_dirs = RuntimeDirs(
                user_data_dir=root,
                config_dir=root,
                data_dir=root,
                logs_dir=root,
                db_dir=not_a_dir,
                videos_dir=root,
                screenshots_dir=root,
                exports_dir=root,
            )

            with self.assertRaises(DbInitError):
                init_db(runtime_dirs=runtime_dirs)

    def test_init_db_preserves_migration_failure_diagnostics(self) -> None:
        from odr_worker.db import DbInitError, init_db
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        def read_sql(migration_id: str) -> str:
            if migration_id == "0001_init":
                return (WORKER_ROOT / "odr_worker" / "migrations" / "0001_init.sql").read_text(
                    encoding="utf-8"
                )
            if migration_id == "0002_tables":
                return "THIS IS INVALID SQL;"
            if migration_id == "0003_queue_recovery":
                return ""
            raise AssertionError("unexpected migration id")

        with tempfile.TemporaryDirectory() as tmp:
            user_data_dir = Path(tmp)
            runtime_dirs = ensure_runtime_dirs(user_data_dir=user_data_dir)

            with mock.patch("odr_worker.db._read_migration_sql", side_effect=read_sql):
                with self.assertRaises(DbInitError) as ctx:
                    init_db(runtime_dirs=runtime_dirs)

        self.assertIn("SQLite migration failed: 0002_tables", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
