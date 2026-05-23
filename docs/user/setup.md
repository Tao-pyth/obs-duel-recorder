# First Setup

Status note: v1.3 adds a Worker setup wizard API for first-run setup status, validation, cancel, and reset/rerun flows. Graphical setup UI remains later work.

## Initial Startup

Check setup state:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/setup/status | Select-Object -ExpandProperty Content
```

The setup status can be:

- `first_run`
- `partial`
- `complete`

The current step is reported as `runtime_path`, `obs_integration`, `oauth`, or `templates`.

---

## Runtime Path Setup

The Worker validates that `user_data/` and its runtime directories exist and are writable.

Existing runtime data is preserved. Setup validation must not delete, move, overwrite, or migrate existing DBs, videos, screenshots, exports, logs, or config.

---

## YouTube Setup

The setup wizard validates whether YouTube credential files exist under:

```text
user_data/config/secrets/
```

Required files:

- `youtube-client-secret.json`
- `youtube-token.json`

The setup wizard reports only file presence and safe paths. It does not display token or client-secret contents.

---

## Template Setup

The setup wizard validates:

- `user_data/config/templates.toml`
- local template files referenced by that config

The repository does not distribute game assets or local template images.

---

## OBS Integration Setup

The setup wizard reports Worker API and Worker version compatibility. The OBS Plugin still owns actual OBS process integration and Worker launch.

---

## Cancel And Reset

- `POST /setup/cancel` records cancellation and preserves completed steps.
- `POST /setup/reset` clears completed steps and preserves runtime data.
- After reset, setup can be run again from the first step.
