# Worker Logging (v0.2)

This document defines the minimal logging behavior for the Python Worker during v0.2.

Goals:
- Write logs to a predictable location under `user_data/`.
- Use date-based log files for easy troubleshooting.
- Keep logging initialization lightweight and restart-safe.

---

## Log Location

Worker logs MUST be written under:

- `user_data/logs/`

The Worker MUST NOT commit logs to git.

---

## Log File Naming

v0.2 uses per-startup-date log files:

- `worker-YYYY-MM-DD.log`

Multiple runs on the same date append to the same file.

---

## Startup Behavior

On startup, the Worker SHOULD:

- Ensure `user_data/logs/` exists.
- Initialize logging early so startup failures can be captured.
- Avoid adding duplicate handlers when initialized multiple times.

---

## Runtime-root Evidence (v0.4)

When launched by the OBS Plugin, the Worker MUST log the resolved `ODR_USER_DATA_DIR` during startup.

Minimum startup log evidence:

- resolved `ODR_USER_DATA_DIR`
- resolved log directory
- whether the runtime root is the default `<repo>/user_data/` or an override
- any manual-action diagnostic caused by a runtime-root continuity mismatch

This evidence allows support and smoke verification to distinguish wrong runtime root, wrong persisted state, and wrong process / wrong port cases.

---

## Scope Guardrails (v0.2)

This document covers only minimal startup logging.

It does NOT define:
- structured logging
- log rotation
- per-module log levels
- upload/queue/database logging policies

Those can be expanded in later milestones.
