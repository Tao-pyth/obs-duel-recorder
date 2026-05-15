# Runtime Directories (v0.2)

The Worker must keep **application code** and **runtime data** separated:

- Application code: `app/`
- Runtime data: `user_data/`

This document describes the v0.2 runtime directory layout and the expected Worker startup behavior.

---

## Directory Layout

The Worker uses the following runtime directories under the runtime root:

- `user_data/config/`
- `user_data/data/`
  - `user_data/data/db/`
  - `user_data/data/videos/`
  - `user_data/data/screenshots/`
  - `user_data/data/exports/`
- `user_data/logs/`

---

## Startup Behavior

On startup, the Worker SHOULD:

- Create the required directories if they do not exist.
- Treat directory creation as **idempotent** (safe to run repeatedly).
- Fail fast with an actionable error if any required directory cannot be created.

The Worker MUST NOT:

- Delete or overwrite existing runtime data during directory creation.
- Commit any runtime data (DBs, logs, screenshots, videos, exports, tokens, secrets) to git.

---

## Runtime Root Override

By default, the runtime root is `<repo>/user_data/`.

If needed, the runtime root can be overridden using the environment variable:

- `ODR_USER_DATA_DIR` (absolute path recommended)

See `odr_worker.config.get_user_data_dir` for the current v0.2 scaffold behavior.
