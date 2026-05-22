-- v0.9 migration 0004_screenshots
-- Purpose: add screenshot archive metadata linked to matches and upload queue items.

CREATE TABLE IF NOT EXISTS screenshots (
  id INTEGER PRIMARY KEY,
  match_id INTEGER,
  queue_item_id INTEGER,
  kind TEXT NOT NULL,
  relative_path TEXT NOT NULL UNIQUE,
  content_type TEXT NOT NULL,
  size_bytes INTEGER NOT NULL,
  status TEXT NOT NULL DEFAULT 'available',
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  CHECK(status IN ('available', 'missing', 'deleted')),
  FOREIGN KEY(match_id) REFERENCES matches(id),
  FOREIGN KEY(queue_item_id) REFERENCES upload_queue(id)
);

CREATE INDEX IF NOT EXISTS idx_screenshots_match_id ON screenshots(match_id);
CREATE INDEX IF NOT EXISTS idx_screenshots_queue_item_id ON screenshots(queue_item_id);
CREATE INDEX IF NOT EXISTS idx_screenshots_status ON screenshots(status);
