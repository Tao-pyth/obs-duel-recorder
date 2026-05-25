# Worker Diagnostic State Model (v0.2)

This document defines a minimal, **implementation-agnostic** diagnostic state set for the OBS Plugin Dock UI.

Goals:
- Distinguish common Worker failure modes beyond simple “connected / disconnected”.
- Enable actionable user guidance (config fix vs runtime permissions vs version mismatch).
- Keep the Worker/Plugin responsibility boundary intact.

This is **not** a UI spec. It is a shared vocabulary for Plugin→Worker lifecycle diagnostics.

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

This section is guidance only; the concrete field names live in the version-scoped `/health` contract and related docs.

For the minimal compatibility contract and gating rules, see:
- `docs/architecture/compatibility.md`

The Plugin can map to a diagnostic state using (in priority order):

1. **Process lifecycle**
   - Not running → `not_started` (or `crashed` if an unexpected exit was observed)
   - Running but API not reachable yet → `starting`

2. **API reachability**
   - `GET /health` fails consistently while process is running → `starting` (early) or `unhealthy` (after timeout)

   Notes:
   - If `GET /version` is implemented, the Plugin may use it as an earlier/lighter reachability signal.
   - `GET /version` success with `GET /health` failure should still be treated as startup/health diagnostics (not an automatic mismatch).

3. **/health (and /version) semantics** (when available)
   - Health OK → `running`
   - Health not OK with a config-related reason → `config_error`
   - Health not OK with a runtime dir/path reason → `runtime_dir_error`
   - Compatibility/version fields indicate mismatch → `version_mismatch` or `api_incompatible`

Compatibility guidance:
- Prefer `api_incompatible` when `api_version` does not match expected.
- Reserve `version_mismatch` for cases where the Plugin explicitly defines a version range/matrix beyond API version gating.

---

## Failure Evidence (minimum)

When transitioning into a failure-adjacent state (`starting` timeout, `unhealthy`, `config_error`, `runtime_dir_error`, `api_incompatible`, `crashed`), record a **minimal evidence bundle** so v0.4 smoke/release-readiness can be judged consistently.

Principles:
- Do not embed large logs into the Dock UI; **surface locations and identifiers**.
- Keep “startup” (`starting`) evidence distinct from “mismatch” evidence (see `docs/architecture/compatibility.md`).

Recommended minimum evidence items (per transition):
- **When**: observation timestamp (wall-clock time).
- **Where**: `host:port` probe target, resolved `ODR_USER_DATA_DIR`, and `<ODR_USER_DATA_DIR>/logs/`.
- **Who**: `instance_id` (if available), `api_version`, and Worker version string.
- **What**: last probe results (success/failure of `/version` and `/health`, plus status code / error kind).
- **If crashed**: last known exit code (if available) and a note pointing to the Worker log directory.
- **If launch failed before health was reachable**: checked command, expected packaged Worker path, whether that path exists, whether the known wrong nested path exists, and a stable launch category such as `missing`, `not_executable_or_access_denied`, `not_executable_or_wrong_architecture`, or `failed_to_start`.
- **If the Worker exits before readiness**: exit code and the sanitized launch summary (`command_path`, fixed `--host`/`--port` args, `working_dir`, and `user_data_dir`) without environment dumps or secrets.

This evidence is expected to be present across:
- OBS Plugin logs (primary for transitions and probe outcomes),
- Worker logs under `user_data/logs/` (primary for internal exceptions),
- v0.4 smoke notes (summary-level confirmation; see `docs/architecture/v0.4-smoke.md`).

For the v1.1 Dock UI, the status-first diagnostic rows should expose selectable
text for the current state, endpoint, `user_data_dir`, expected Worker path,
ownership, latest detail/error, and recommended action. Manual Start/Stop
controls remain enabled only when the diagnostic state is `running`. The Dock
should include a first-run `Setup` row that summarizes whether the minimum
runtime path, Worker launch, endpoint health, and API compatibility checks are
ready or need action. The Dock
should also show the last manual recording command result, including recording
state, `session_id`, source, last action, reason, and update time when
available; failed commands should leave a compact diagnostic reason visible.
The same Dock surface should include the last OBS recording output path or,
when OBS does not return one, explicit fallback evidence from the current output
path captured at recording start plus a note to check OBS Output settings and
OBS logs.

---

## Notes

- Keep “compatibility mismatch” distinct from generic “unhealthy” so users get the correct action.
- Avoid putting heavy logic in the OBS Plugin; the Worker should surface enough information via `/health` to make mapping reliable.
