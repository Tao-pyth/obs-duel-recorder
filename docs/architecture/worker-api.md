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
