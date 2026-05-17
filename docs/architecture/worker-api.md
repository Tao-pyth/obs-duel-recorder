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
