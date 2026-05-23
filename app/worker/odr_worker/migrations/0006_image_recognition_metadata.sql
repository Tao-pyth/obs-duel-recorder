-- v2.0 migration 0006_image_recognition_metadata
-- Purpose: add editable recognition-assisted metadata fields.

ALTER TABLE matches ADD COLUMN rank TEXT NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN dp TEXT NOT NULL DEFAULT '';

CREATE INDEX IF NOT EXISTS idx_matches_rank ON matches(rank);
CREATE INDEX IF NOT EXISTS idx_matches_dp ON matches(dp);

CREATE TABLE IF NOT EXISTS recognition_candidates (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  match_id INTEGER,
  provider TEXT NOT NULL,
  field TEXT NOT NULL,
  value TEXT NOT NULL,
  confidence REAL NOT NULL,
  evidence TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'candidate',
  corrected_value TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  FOREIGN KEY(match_id) REFERENCES matches(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_recognition_candidates_match_id ON recognition_candidates(match_id);
CREATE INDEX IF NOT EXISTS idx_recognition_candidates_status ON recognition_candidates(status);
