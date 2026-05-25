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

Validation responses include stable `code` values such as
`runtime_path_ready`, `runtime_path_action_required`,
`worker_api_compatible`, `oauth_action_required`, and
`templates_action_required`. The Dock can use these codes without parsing
diagnostic text.

---

## Runtime Path Setup

The Worker validates that `user_data/` and its runtime directories exist and are writable.

Existing runtime data is preserved. Setup validation must not delete, move, overwrite, or migrate existing DBs, videos, screenshots, exports, logs, or config.

In OBS, open the `OBS Duel Recorder` Dock and check the `Setup` row. Saving
Settings starts the minimum first-run validation path: the Plugin creates
`user_data_dir` when possible, checks that it is writable, starts the Worker,
and reports endpoint/API compatibility in the Dock. Manual Start/Stop stays
disabled until the Worker state is `running`.

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

You can register local templates without hand-editing TOML by calling the setup
registration endpoint after placing your own files under `user_data/templates/`:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/setup/templates/register -Method Post -ContentType "application/json" -Body '{"kind":"start","path":"duel-start.tpl","threshold":1.0,"confirmations":2}' | Select-Object -ExpandProperty Content
Invoke-WebRequest http://127.0.0.1:8787/setup/templates/register -Method Post -ContentType "application/json" -Body '{"kind":"end","path":"duel-end.tpl","threshold":1.0,"confirmations":2}' | Select-Object -ExpandProperty Content
```

The endpoint validates that the file exists, is not empty, the threshold is
between `0.0` and `1.0`, and confirmations is a positive integer. Successful
registration writes `user_data/config/templates.toml` in the same format used by
the detection API.

Before enabling automatic recording, test your own local sample input:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/detection/test -Method Post -ContentType "application/json" -Body '{"kind":"start","frame_text":"sample frame bytes or fixture text"}' | Select-Object -ExpandProperty Content
```

Use samples captured from your own environment and keep them under runtime
storage such as `user_data/templates/`. Do not commit local template files,
screenshots, or game assets to the repository. The test result includes
template name, detection kind, score, match status, and diagnostics for missing
or low-confidence matches.

---

## OBS Integration Setup

The setup wizard reports Worker API and Worker version compatibility. The OBS Plugin still owns actual OBS process integration and Worker launch.

---

## Cancel And Reset

- `POST /setup/cancel` records cancellation and preserves completed steps.
- `POST /setup/reset` clears completed steps and preserves runtime data.
- After reset, setup can be run again from the first step.
