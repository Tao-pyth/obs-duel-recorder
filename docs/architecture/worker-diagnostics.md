# Worker Diagnostic State Model (v0.2)

This document defines a minimal, **implementation-agnostic** diagnostic state set for the OBS Plugin Dock UI.

Goals:
- Distinguish common Worker failure modes beyond simple “connected / disconnected”.
- Enable actionable user guidance (config fix vs runtime permissions vs version mismatch).
- Keep the Worker/Plugin responsibility boundary intact.

This is **not** a UI spec. It is a shared vocabulary for Plugin↔Worker lifecycle diagnostics.

---

## Terms

- **Worker process state**: what the OS/process lifecycle is doing (starting/running/exited).
- **API state**: whether the localhost HTTP API responds.
- **Compatibility state**: whether the Worker and Plugin can safely talk using the same contract.

The Dock UI should primarily display a **single diagnostic state** plus a short recommended action.

---

## Diagnostic States

Recommended enum-like values (string IDs):

### `not_started`
Worker is not running (never started, or intentionally stopped).

Recommended user action:
- Start the Worker.

### `starting`
Worker process has been launched, but the API is not yet ready.

Recommended user action:
- Wait; if it exceeds a timeout, show logs and suggest restart.

### `running`
Worker API is responding and reports healthy.

Recommended user action:
- None.

### `unhealthy`
Worker API responds but reports unhealthy.

Recommended user action:
- Show the reported reason and point to logs.

### `config_error`
Worker cannot start or cannot become healthy due to invalid or missing configuration.

Recommended user action:
- Fix config file location/contents; restart.

### `runtime_dir_error`
Worker cannot access or create required runtime directories under `user_data/`.

Recommended user action:
- Fix directory permissions or choose a valid `user_data` path; restart.

### `version_mismatch`
Worker is running, but the Worker version is not compatible with the Plugin version.

Recommended user action:
- Update either Plugin or Worker so versions align.

### `api_incompatible`
Worker is running, but the API contract/version is incompatible (even if Worker version strings look close).

Recommended user action:
- Update to a compatible pair; avoid continuing.

### `crashed`
Worker process exited unexpectedly.

Recommended user action:
- Show last exit code and logs; allow restart.

---

## Mapping Guidance (high level)

This section is guidance only; the concrete field names live in the v0.2 `/health` contract and related docs.

The Plugin can map to a diagnostic state using (in priority order):

1. **Process lifecycle**
   - Not running → `not_started` (or `crashed` if an unexpected exit was observed)
   - Running but API not reachable yet → `starting`

2. **API reachability**
   - `GET /health` fails consistently while process is running → `starting` (early) or `unhealthy` (after timeout)

3. **/health semantics** (when available)
   - Health OK → `running`
   - Health not OK with a config-related reason → `config_error`
   - Health not OK with a runtime dir/path reason → `runtime_dir_error`
   - Compatibility/version fields indicate mismatch → `version_mismatch` or `api_incompatible`

---

## Notes

- Keep “compatibility mismatch” distinct from generic “unhealthy” so users get the correct action.
- Avoid putting heavy logic in the OBS Plugin; the Worker should surface enough information via `/health` to make mapping reliable.
