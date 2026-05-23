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

---

## v0.9 Screenshot Endpoints

v0.9 adds Worker-owned screenshot archive endpoints while preserving prior API compatibility.

### `GET /screenshots`

Purpose:
- List screenshot records.
- Optionally filter records by match or upload queue item.

Query:
- `match_id`: optional integer.
- `queue_item_id`: optional integer.

### `POST /screenshots/capture`

Purpose:
- Store screenshot bytes under `user_data/data/screenshots/`.
- Insert a SQLite metadata row linked to optional match and queue context.
- Return the created screenshot record.

Request fields:
- `match_id`: optional integer.
- `queue_item_id`: optional integer.
- `kind`: optional screenshot kind.
- `captured_at`: optional timestamp used in the file name.
- `content_type`: optional content type.
- `extension`: optional file extension.
- `content_base64`: screenshot bytes, or
- `content_text`: UTF-8 fixture content.

### `GET /screenshots/{screenshot_id}`

Purpose:
- Return one screenshot metadata record.

### `GET /screenshots/{screenshot_id}/preview`

Purpose:
- Return base64 preview content and content type when the file exists.
- Mark an available record `missing` when the DB row exists but the file is gone.

### `POST /screenshots/cleanup`

Purpose:
- Delete local screenshot files and mark DB rows `deleted` only when the linked queue item is cleanup-safe.
- Preserve screenshot evidence for upload failures, quota waits, active uploads, and manual review.

Request:
- `queue_item_id`: integer.

The v0.9 screenshot API is defined by:
- `docs/architecture/screenshots.md`
- `docs/requirements/v0.9-screenshot-system-acceptance.md`

---

## v1.0 Upload Endpoints

v1.0 adds Worker-owned upload status and execution endpoints while preserving prior API compatibility.

### `GET /upload/status`

Purpose:
- Report upload configuration and queue status without leaking secrets.
- Show whether OAuth client-secret and token files are configured.
- Provide queue counts by upload state.

Response fields:
- `settings.oauth_scope`: always `https://www.googleapis.com/auth/youtube.upload`.
- `settings.privacy_status`: default `private`.
- `settings.client_secret_configured`: boolean.
- `settings.token_configured`: boolean.
- `queue_counts`: counts by queue state.
- `manual_actions`: `retry`, `discard`, and `mark_uploaded`.

### `POST /upload/process-next`

Purpose:
- Select the lowest-id `ready_upload` queue item.
- Move it through upload execution.
- Persist success or failure outcome into the queue item.

v1.0 test/smoke request fields:
- `mock_result`: one of `success`, `network_error`, `quota_exceeded`, `ambiguous_error`, or `auth_error`.
- `youtube_video_id`: required for deterministic success when overriding the default mock id.
- `youtube_url`: optional; derived from `youtube_video_id` when omitted.
- `next_attempt_at`: optional for retry/quota outcomes.
- `manual_review_evidence`: optional object; redacted before persistence.

Outcome mapping:
- `success` -> `uploaded`
- `network_error` -> `upload_failed`
- `quota_exceeded` -> `quota_waiting`
- `ambiguous_error` -> `need_manual_review`
- `auth_error` -> `need_manual_review`
- missing local file -> `discarded`

Manual decisions continue to use `POST /queue/items/{item_id}/command`:
- `retry`
- `discard`
- `mark_uploaded`

The v1.0 upload API is defined by:
- `docs/architecture/upload.md`
- `docs/requirements/v1.0-youtube-upload-mvp-acceptance.md`

---

## v1.1 Match Metadata Endpoints

v1.1 adds Worker-owned match metadata endpoints while preserving prior API compatibility.

### `GET /matches`

Purpose:
- List match metadata records.
- Optionally search lightweight local metadata fields.

Query:
- `query`: optional text search across opponent deck, memo, deck name, and result.

### `POST /matches`

Purpose:
- Create a match metadata record.
- Accept editable metadata fields with deterministic defaults.

Request fields:
- `deck_name`: optional string.
- `opponent_deck`: optional string.
- `result`: optional string.
- `memo`: optional string.
- `started_at`: optional string.
- `ended_at`: optional string.
- `title_template`: optional string.

### `GET /matches/{match_id}`

Purpose:
- Return one match metadata record.

### `PUT /matches/{match_id}/metadata`

Purpose:
- Update editable match metadata fields.
- Reject invalid values without changing the existing record.
- Allow post-upload edits for future generated metadata without rewriting an already uploaded YouTube record automatically.

### `GET /matches/{match_id}/upload-metadata`

Purpose:
- Generate deterministic upload title, description, notes, and supported title-template variables.
- Apply stable fallbacks for missing metadata and unknown template variables.

### Upload integration

`POST /upload/process-next` includes `upload_metadata` in the response when the processed queue item is linked to a `match_id` and metadata is available.

The v1.1 match metadata API is defined by:
- `docs/architecture/metadata.md`
- `docs/requirements/v1.1-match-metadata-acceptance.md`

---

## v1.2 Export Endpoints

v1.2 adds Worker-owned export endpoints while preserving prior API compatibility.

### `GET /exports`

Purpose:
- List completed ZIP exports under `user_data/data/exports/`.
- Return each export file name, absolute path, size, and updated timestamp.

### `POST /exports`

Purpose:
- Create a ZIP archive backup from runtime state.
- Return the completed output path and manifest.
- Avoid mutating live runtime state.

Request fields:
- `created_at`: optional timestamp used for deterministic default naming.
- `name`: optional ZIP file name or stem.
- `include_videos`: optional boolean, default `false`.

ZIP contents:
- `manifest.json`
- `database/odr.sqlite3`
- `metadata/matches.json`
- `metadata/upload_queue.json`
- `metadata/screenshots.json`
- `metadata/video_linkages.json`
- existing referenced screenshot files under `screenshots/`
- optional video files under `videos/` only when `include_videos` is `true`

Error behavior:
- Invalid payloads return HTTP 400 with `code=export_payload_invalid`.
- Existing target archives return HTTP 400 with `code=export_path_conflict`.
- Failed exports remove temporary ZIP files and do not replace completed archives.

The v1.2 export API is defined by:
- `docs/architecture/export.md`
- `docs/requirements/v1.2-export-system-acceptance.md`

---

## v1.3 Setup Wizard Endpoints

v1.3 adds Worker-owned setup state and validation endpoints while preserving prior API compatibility.

### `GET /setup/status`

Purpose:
- Return first-run, partial, complete, cancel, and reset state.
- Return current setup step and each setup step validation status.
- Surface the runtime setup state file path.

### `POST /setup/validate`

Purpose:
- Validate setup prerequisites without mutating setup state.
- Return runtime path, OBS integration, OAuth, and template validation diagnostics.

### `POST /setup/steps/{step_id}/complete`

Purpose:
- Mark one setup step complete or incomplete.
- Persist setup progress under runtime data.

Request:
- `completed`: optional boolean, default `true`.

### `POST /setup/cancel`

Purpose:
- Record setup cancellation without deleting completed steps or runtime data.

### `POST /setup/reset`

Purpose:
- Clear completed setup steps and increment the reset counter.
- Preserve existing runtime data.

Error behavior:
- Unknown setup steps return HTTP 400 with `code=setup_step_unknown`.
- Invalid payloads return HTTP 400 with `code=setup_payload_invalid`.
- Invalid persisted setup state returns HTTP 400 with `code=setup_state_invalid`.

The v1.3 setup wizard API is defined by:
- `docs/architecture/setup-wizard.md`
- `docs/requirements/v1.3-setup-wizard-acceptance.md`

---

## v1.4 Update Endpoints

v1.4 adds Worker-owned update validation and update-state diagnostics while preserving prior API compatibility.

### `GET /update/status`

Purpose:
- Return the last update state.
- Detect partial or failed update state.
- Surface the target Worker/API version and DB backup directory.

Response fields:
- `status`: `idle`, `in_progress`, `completed`, `failed`, or `invalid`
- `partial_update_detected`: boolean
- `current_version`: last installed version, or `unknown`
- `target_version`: Worker version from the running code
- `api_version`: Worker API version
- `state_path`: update-state file path
- `backup_dir`: DB backup directory
- `last_update`: persisted update state object

### `POST /update/validate`

Purpose:
- Validate version/API compatibility before mutation.
- Reject unsupported downgrade paths.
- Report whether the SQLite DB exists for backup.

Request fields:
- `current_version`: optional SemVer string for the installed version before update
- `target_version`: optional SemVer string; defaults to the running Worker version
- `expected_api_version`: optional API version; defaults to the running Worker API version

### `POST /update/apply`

Purpose:
- Record update `in_progress` state.
- Back up SQLite before migration execution when a DB exists.
- Run deterministic Worker migrations.
- Record `completed` or `failed` state with recovery guidance.

Error behavior:
- Invalid payloads return HTTP 400 with `code=update_payload_invalid`.
- Validation failures return HTTP 409 with `code=update_validation_failed`.
- Migration failures return HTTP 409 with `code=update_migration_failed`.

The v1.4 update API is defined by:
- `docs/architecture/update-system.md`
- `docs/requirements/v1.4-update-system-acceptance.md`

---

## v2.0 Image Recognition Endpoints

v2.0 adds recognition-assisted metadata extraction while preserving prior API compatibility.

### `POST /recognition/analyze`

Purpose:
- Evaluate deterministic fixture input through the Worker-owned recognition provider boundary.
- Return result, rank, and DP candidates with evidence and confidence.
- Avoid mutating match metadata automatically.
- Provide the manual correction endpoint and patch body when a `match_id` is supplied.

Request fields:
- `provider`: optional; `fixture` is the only v2.0 provider.
- `match_id`: optional integer; validates that the target match exists.
- `content_text`: UTF-8 fixture text containing lines such as `result: win`, `rank: Diamond I`, and `dp: 12000`, or
- `fixture`: object containing optional `result`, `rank`, `dp`, and `confidence` values.

Response fields:
- `provider`: `fixture`.
- `status`: `candidates_available` or `manual_review_required`.
- `minimum_confidence`: required confidence for `metadata_patch`.
- `candidates`: list of `{field, value, confidence, evidence}`.
- `metadata_patch`: values safe to apply through `PUT /matches/{match_id}/metadata`.
- `manual_correction`: correction method, endpoint, and body when `match_id` is provided.
- `mutated`: always `false` for v2.0 analysis.
- `candidate_records`: persisted audit records when SQLite is available.

Error behavior:
- Unsupported providers return HTTP 400 with `code=recognition_provider_unsupported`.
- Invalid recognition payloads return HTTP 400 with `code=recognition_payload_invalid`.
- Missing target matches return HTTP 404 with `code=match_not_found`.

### `GET /recognition/candidates`

Purpose:
- List persisted recognition candidates for audit and manual review.

Query:
- `match_id`: optional integer filter.
- `status`: optional filter: `candidate`, `confirmed`, `corrected`, or `rejected`.

### `POST /recognition/candidates/{candidate_id}/command`

Purpose:
- Resolve a candidate through explicit manual action.
- Preserve an audit trail while allowing confirmed or corrected values to update match metadata.

Actions:
- `confirm`: apply the candidate value to the linked match metadata.
- `correct`: apply the provided `value` to the linked match metadata and store the corrected value.
- `reject`: mark the candidate rejected without changing match metadata.

Error behavior:
- Invalid commands return HTTP 400 with `code=recognition_command_invalid`.
- Missing candidates return HTTP 404 with `code=recognition_candidate_not_found`.
- Already resolved candidates return HTTP 400 with `code=recognition_candidate_already_resolved`.

The v2.0 image recognition API is defined by:
- `docs/architecture/image-recognition.md`
- `docs/requirements/v2.0-ocr-integration-acceptance.md`
