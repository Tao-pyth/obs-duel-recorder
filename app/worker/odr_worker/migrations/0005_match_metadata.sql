-- v1.1 migration 0005_match_metadata
-- Purpose: add editable match metadata and upload metadata template fields.

ALTER TABLE matches ADD COLUMN deck_name TEXT NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN opponent_deck TEXT NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN result TEXT NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN memo TEXT NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN started_at TEXT NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN ended_at TEXT NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN title_template TEXT NOT NULL DEFAULT '';
ALTER TABLE matches ADD COLUMN updated_at TEXT NOT NULL DEFAULT '';

UPDATE matches SET updated_at = created_at WHERE updated_at = '';

CREATE INDEX IF NOT EXISTS idx_matches_opponent_deck ON matches(opponent_deck);
CREATE INDEX IF NOT EXISTS idx_matches_result ON matches(result);
CREATE INDEX IF NOT EXISTS idx_matches_started_at ON matches(started_at);
