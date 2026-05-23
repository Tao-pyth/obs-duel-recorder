from __future__ import annotations

import datetime as _dt
import logging
import sqlite3
from dataclasses import dataclass
from pathlib import Path

from .runtime_dirs import RuntimeDirs


class DbInitError(RuntimeError):
    pass


@dataclass(frozen=True)
class DbInfo:
    db_path: Path
    schema_version: int
    applied_migrations: tuple[str, ...]


_MIGRATIONS: tuple[str, ...] = (
    "0001_init",
    "0002_tables",
    "0003_queue_recovery",
    "0004_screenshots",
    "0005_match_metadata",
    "0006_image_recognition_metadata",
)


def get_db_path(*, runtime_dirs: RuntimeDirs) -> Path:
    return (runtime_dirs.db_dir / "odr.sqlite3").resolve()


def _read_migration_sql(migration_id: str) -> str:
    migrations_dir = Path(__file__).resolve().parent / "migrations"
    path = migrations_dir / f"{migration_id}.sql"
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise DbInitError(f"Failed to read migration file: {path}") from exc


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")


def _ensure_meta_tables(conn: sqlite3.Connection) -> None:
    conn.execute(
        "CREATE TABLE IF NOT EXISTS odr_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);"
    )
    conn.execute(
        "CREATE TABLE IF NOT EXISTS schema_migrations (id TEXT PRIMARY KEY, applied_at TEXT NOT NULL);"
    )


def _get_schema_version(conn: sqlite3.Connection) -> int:
    row = conn.execute("SELECT value FROM odr_meta WHERE key = 'schema_version';").fetchone()
    if row is None:
        return 0
    try:
        return int(row[0])
    except (TypeError, ValueError):
        return 0


def _set_schema_version(conn: sqlite3.Connection, schema_version: int) -> None:
    conn.execute(
        "INSERT INTO odr_meta(key, value) VALUES('schema_version', ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
        (str(schema_version),),
    )


def _migration_version(migration_id: str) -> int:
    try:
        return int(migration_id.split("_", 1)[0])
    except (IndexError, ValueError):
        return 0


def _get_applied_migration_ids(conn: sqlite3.Connection) -> set[str]:
    rows = conn.execute("SELECT id FROM schema_migrations;").fetchall()
    return {str(r[0]) for r in rows}


def init_db(*, runtime_dirs: RuntimeDirs, logger: logging.Logger | None = None) -> DbInfo:
    """Initialize SQLite and apply v0.3 migrations.

    Goals:
    - DB lives under runtime data (`user_data/`)
    - startup is idempotent and restart-safe
    - migrations apply deterministically and at most once
    """

    log = logger or logging.getLogger(__name__)
    db_path = get_db_path(runtime_dirs=runtime_dirs)

    try:
        conn = sqlite3.connect(db_path)
    except (sqlite3.Error, OSError) as exc:
        raise DbInitError(f"Failed to open SQLite DB: {db_path}: {exc}") from exc

    try:
        conn.execute("PRAGMA foreign_keys = ON;")
        _ensure_meta_tables(conn)
        applied = _get_applied_migration_ids(conn)

        applied_in_order: list[str] = []
        current_version = _get_schema_version(conn)
        log.info("sqlite db=%s schema_version=%s", db_path, current_version)

        for migration_id in _MIGRATIONS:
            if migration_id in applied:
                applied_in_order.append(migration_id)
                continue

            sql = _read_migration_sql(migration_id)
            try:
                conn.execute("BEGIN;")
                conn.executescript(sql)
                conn.execute(
                    "INSERT INTO schema_migrations(id, applied_at) VALUES(?, ?);",
                    (migration_id, _utc_now_iso()),
                )
                new_version = max(current_version, _migration_version(migration_id))
                _set_schema_version(conn, new_version)
                conn.execute("COMMIT;")
                current_version = new_version
            except sqlite3.DatabaseError as exc:
                if conn.in_transaction:
                    try:
                        conn.execute("ROLLBACK;")
                    except sqlite3.DatabaseError:
                        pass
                raise DbInitError(f"SQLite migration failed: {migration_id}: {exc}") from exc

            applied_in_order.append(migration_id)

        return DbInfo(db_path=db_path, schema_version=current_version, applied_migrations=tuple(applied_in_order))
    finally:
        conn.close()
