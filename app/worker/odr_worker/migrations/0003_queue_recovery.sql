-- v0.7 migration 0003_queue_recovery
-- Purpose: add queue recovery, retry, and manual-review metadata.

ALTER TABLE upload_queue ADD COLUMN video_path TEXT NOT NULL DEFAULT '';
ALTER TABLE upload_queue ADD COLUMN youtube_video_id TEXT NOT NULL DEFAULT '';
ALTER TABLE upload_queue ADD COLUMN youtube_url TEXT NOT NULL DEFAULT '';
ALTER TABLE upload_queue ADD COLUMN retry_count INTEGER NOT NULL DEFAULT 0;
ALTER TABLE upload_queue ADD COLUMN max_retries INTEGER NOT NULL DEFAULT 3;
ALTER TABLE upload_queue ADD COLUMN next_attempt_at TEXT NOT NULL DEFAULT '';
ALTER TABLE upload_queue ADD COLUMN last_error_code TEXT NOT NULL DEFAULT '';
ALTER TABLE upload_queue ADD COLUMN last_error_message TEXT NOT NULL DEFAULT '';
ALTER TABLE upload_queue ADD COLUMN manual_review_reason TEXT NOT NULL DEFAULT '';
ALTER TABLE upload_queue ADD COLUMN manual_review_evidence_json TEXT NOT NULL DEFAULT '{}';
ALTER TABLE upload_queue ADD COLUMN updated_at TEXT NOT NULL DEFAULT '';

UPDATE upload_queue SET updated_at = created_at WHERE updated_at = '';

CREATE INDEX IF NOT EXISTS idx_upload_queue_next_attempt ON upload_queue(state, next_attempt_at);
CREATE INDEX IF NOT EXISTS idx_upload_queue_updated_at ON upload_queue(updated_at);
