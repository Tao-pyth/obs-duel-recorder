-- v2.3 migration 0008_runtime_optimization_indexes
-- Purpose: support queue/upload polling and recovery without full queue scans.

CREATE INDEX IF NOT EXISTS idx_upload_queue_state_id ON upload_queue(state, id);
CREATE INDEX IF NOT EXISTS idx_upload_queue_state_updated_at ON upload_queue(state, updated_at);
