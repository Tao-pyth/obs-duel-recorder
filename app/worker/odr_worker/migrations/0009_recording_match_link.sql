-- v1.1 migration 0009_recording_match_link
-- Purpose: link completed recording sessions to durable match metadata rows.

ALTER TABLE matches ADD COLUMN recording_session_id TEXT NOT NULL DEFAULT '';

CREATE UNIQUE INDEX IF NOT EXISTS idx_matches_recording_session_id
ON matches(recording_session_id)
WHERE recording_session_id != '';
