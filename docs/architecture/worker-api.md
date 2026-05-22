# Worker API (v0.2)

This document defines the minimal localhost HTTP API for **v0.2 - Worker Core API**.

Scope (v0.2):
- `GET /health` only
- No queue / upload / OCR / template matching endpoints

The Worker must run on localhost by default.

---

## `GET /health`

Purpose:
- Allow the OBS Plugin to detect whether the Worker is reachable and healthy.
- Provide minimal diagnostic fields for the Dock UI.
- Provide compatibility gating fields (`api_version`).

### Response (v0.2)

Minimum fields:

- `status`: `ok` (v0.2 uses `ok` only; later versions may introduce `degraded`)
- `version`: Worker version string
- `api_version`: API contract version string (v0.2: `0.2`)
- `uptime_seconds`: integer seconds since Worker start
- `config_loaded`: whether `user_data/config/worker.toml` was loaded
- `runtime_dirs_ok`: whether runtime directories were initialized successfully
- `paths`: canonical runtime path fields
  - `app_dir`
  - `user_data_dir`
  - `config_dir`
  - `data_dir`
  - `logs_dir`

Compatibility rules for `api_version` live in:
- `docs/architecture/compatibility.md`

Canonical v0.2 acceptance criteria:
- `docs/requirements/v0.2-worker-core-api-acceptance.md`

---

## v0.6 Recording State Endpoints

v0.6 adds a recording lifecycle boundary while preserving `/health`, `/version`, and `/overlay/state` compatibility.

### `GET /recording/state`

Purpose:
- Return the Worker-owned authoritative recording lifecycle state.
- Provide enough state for Plugin/Dock display and smoke verification.
- Support restart recovery diagnostics.

Response fields:

- `state`: one of `idle`, `starting`, `recording`, `stopping`, `completed`, `interrupted`, or `error`
- `session_id`: active or most recent recording session identifier, or empty when none exists
- `command_source`: `manual`, `automatic`, or `recovery`
- `last_action`: last accepted lifecycle command
- `reason`: optional diagnostic reason
- `updated_at`: UTC ISO 8601 timestamp

### `POST /recording/command`

Purpose:
- Apply one lifecycle command through the same boundary for manual, automatic, and recovery sources.
- Reject invalid transitions without mutating state.

Request fields:

- `action`: one of `start`, `confirm_started`, `stop`, `confirm_stopped`, `mark_interrupted`, `discard_interrupted`, or `reset`
- `source`: optional; one of `manual`, `automatic`, or `recovery`; defaults to `manual`
- `reason`: optional diagnostic string

Error behavior:

- Invalid payloads return HTTP 400 with `code=recording_command_invalid`.
- Invalid transitions return HTTP 409 with `code=recording_transition_invalid`.
- Error details include at least the current state and requested action when transition validation fails.

The v0.6 recording API is defined by:
- `docs/architecture/recording.md`
- `docs/requirements/v0.6-recording-state-management-acceptance.md`

---

## v0.7 Queue Recovery Endpoints

v0.7 adds a recovery-safe upload queue boundary while preserving `/health`, `/version`, `/overlay/state`, and `/recording/*` compatibility.

### `GET /queue/recovery`

Purpose:
- Return the startup queue recovery decisions applied during Worker initialization.
- Provide smoke-verifiable evidence for interrupted `uploading` reconciliation.

Response:
- `recovered`: list of `{id, from, to, reason}` records.

### `GET /queue/items`

Purpose:
- List persisted queue items.
- Optionally filter by `state`.

Query:
- `state`: optional queue state filter.

### `POST /queue/items`

Purpose:
- Create a `ready_upload` queue item for recovery/retry processing.

Request fields:
- `match_id`: optional integer
- `video_path`: optional string
- `max_retries`: optional integer, default `3`

### `GET /queue/items/{item_id}`

Purpose:
- Return one queue item and its retry/manual-review evidence.

### `POST /queue/items/{item_id}/command`

Purpose:
- Apply a queue transition through the Worker-owned state machine.

Actions:
- `start_upload`
- `mark_uploaded`
- `mark_upload_failed`
- `mark_quota_waiting`
- `mark_need_manual_review`
- `retry`
- `discard`

Error behavior:
- Invalid payloads return HTTP 400 with `code=queue_payload_invalid`.
- Missing items return HTTP 404 with `code=queue_item_not_found`.
- Invalid transitions return HTTP 409 with `code=queue_transition_invalid`.

The v0.7 queue API is defined by:
- `docs/architecture/queue.md`
- `docs/requirements/v0.7-queue-recovery-system-acceptance.md`

---

## v0.8 Template Detection Endpoints

v0.8 adds local template detection and duel lifecycle state while preserving prior API compatibility.

### `GET /detection/templates`

Purpose:
- Return template configuration diagnostics.
- Confirm whether `user_data/config/templates.toml` was loaded.
- List locally configured templates without bundling any template assets.

### `GET /detection/state`

Purpose:
- Return the current duel lifecycle state.

Response fields:
- `lifecycle_state`: `no_duel`, `potential_duel`, `active_duel`, or `ended_duel`
- `start_count`: current start confirmation count
- `end_count`: current end confirmation count
- `last_event`: latest lifecycle event
- `updated_at`: UTC ISO 8601 timestamp

### `POST /detection/frame`

Purpose:
- Evaluate one frame fixture against configured local templates.
- Update duel lifecycle state.
- Trigger automatic recording commands through the v0.6 recording boundary when start/end is confirmed.

Request fields:
- `frame_text`: UTF-8 fixture text, or
- `frame_hex`: hex-encoded bytes

Response fields:
- lifecycle state fields
- `events`: lifecycle and recording integration events
- `matches`: template match details
- `recording_state`: current Worker recording state after detection integration

Error behavior:
- Invalid detection payloads return HTTP 400 with `code=detection_payload_invalid`.
- Invalid detection config makes runtime endpoints return HTTP 503 with `code=detection_unavailable`.

The v0.8 detection API is defined by:
- `docs/architecture/detection.md`
- `docs/requirements/v0.8-template-matching-mvp-acceptance.md`
