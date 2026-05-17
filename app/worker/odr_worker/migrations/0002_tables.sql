-- v0.3 migration 0002_tables
-- Purpose: create initial tables required by v0.3 acceptance checklist.

CREATE TABLE IF NOT EXISTS matches (
  id INTEGER PRIMARY KEY,
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE TABLE IF NOT EXISTS upload_queue (
  id INTEGER PRIMARY KEY,
  match_id INTEGER,
  state TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  FOREIGN KEY(match_id) REFERENCES matches(id)
);

CREATE INDEX IF NOT EXISTS idx_upload_queue_state ON upload_queue(state);
