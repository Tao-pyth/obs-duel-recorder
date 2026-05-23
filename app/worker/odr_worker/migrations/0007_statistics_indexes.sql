-- v2.1 migration 0007_statistics_indexes
-- Purpose: add read-side indexes for statistics queries.

CREATE INDEX IF NOT EXISTS idx_matches_deck_name ON matches(deck_name);
CREATE INDEX IF NOT EXISTS idx_matches_result_started_at ON matches(result, started_at);
CREATE INDEX IF NOT EXISTS idx_upload_queue_match_id_state ON upload_queue(match_id, state);
