-- v0.3 migration 0001_init
-- Purpose: create metadata tables required for deterministic, restart-safe migrations.

CREATE TABLE IF NOT EXISTS odr_meta (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS schema_migrations (
  id TEXT PRIMARY KEY,
  applied_at TEXT NOT NULL
);
