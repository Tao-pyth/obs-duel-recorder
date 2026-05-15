# Worker Configuration (v0.2 scaffold)

This document defines where the Python Worker loads configuration from in **v0.2**.

This is intentionally minimal and designed to preserve:
- responsibility separation (Plugin vs Worker)
- restart safety
- runtime data separation (`user_data/`)
- secret-safety (no OAuth tokens or credentials in git)

## Runtime location

The Worker loads config from the runtime directory:

- `user_data/config/worker.toml`

The Worker MUST be able to start with **no config file present**.

## Secrets

Secrets / OAuth credentials MUST NOT be committed to git.

The following paths MUST remain ignored (see `AGENTS.md`):
- `config/secrets/`
- `user_data/config/secrets/`

## v0.2 config shape

The Worker reads a single TOML file with a top-level `[worker]` table.

Example:

```toml
[worker]
host = "127.0.0.1"
port = 8787
```

Notes:
- v0.2 keeps this minimal (host/port only).
- Additional settings (logging, feature flags, etc.) should be added by later issues.

## Environment override (optional)

For development, the Worker may support overriding the runtime directory via an environment variable:

- `ODR_USER_DATA_DIR` (absolute path)

This should remain an optional developer convenience; the default is still `<repo>/user_data/`.
