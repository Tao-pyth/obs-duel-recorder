# Setup Wizard

v1.3 introduces a Worker-owned setup wizard state and validation boundary for first-time setup and recovery from partial setup.

Setup state is runtime data and is stored at:

```text
user_data/data/setup-wizard.json
```

The setup wizard does not move, delete, migrate, or overwrite existing runtime data.

---

## Steps

The canonical setup steps are:

- `runtime_path`
- `obs_integration`
- `oauth`
- `templates`

Each step can be marked complete independently. The state model distinguishes:

- `first_run`: no completed setup steps.
- `partial`: at least one step is complete, but not all steps are complete.
- `complete`: all setup steps are complete.

`cancel` records a cancellation timestamp without removing completed steps. `reset` clears completed steps and increments a reset counter while preserving runtime data.

---

## API Surface

### `GET /setup/status`

Returns the persisted setup state, current step, completed steps, and validation status for each setup step.

### `POST /setup/validate`

Returns validation results without mutating setup state.

Each validation result includes:

- `status`: `ready` or `action_required`
- `code`: stable machine-readable result code
- `diagnostics`: stable diagnostic objects, each with a `code`
- `existing_runtime_data`: whether existing runtime data was detected where
  relevant

Current stable result codes:

- `runtime_path_ready`
- `runtime_path_action_required`
- `worker_api_compatible`
- `oauth_ready`
- `oauth_action_required`
- `templates_ready`
- `templates_action_required`

### `POST /setup/steps/{step_id}/complete`

Marks one step complete by default.

Request:

```json
{"completed": true}
```

Passing `{"completed": false}` marks the step incomplete.

### `POST /setup/templates/register`

Registers or updates one local detection template and writes a compatible
`user_data/config/templates.toml`.

Request:

```json
{
  "kind": "start",
  "path": "duel-start.tpl",
  "threshold": 1.0,
  "confirmations": 2
}
```

Rules:

- `kind` accepts `start`, `end`, `duel_start`, or `duel_end`.
- `path` must point to an existing, non-empty local template file. Relative
  paths resolve through the same config/templates directories as detection.
- `threshold` must be between `0.0` and `1.0`.
- `confirmations` must be a positive integer and updates the matching
  start/end confirmation count.
- Template files are referenced from runtime storage; the endpoint does not
  copy game assets into the repository or release package.

Invalid registration requests return HTTP 400 with
`code=setup_template_registration_invalid`. Successful registration reloads the
Worker detection runtime so `GET /detection/templates` immediately reflects the
generated config.

### `POST /setup/cancel`

Records cancellation and returns the current setup state.

### `POST /setup/reset`

Clears completed setup steps, increments `reset_count`, and returns the current setup state.

---

## Validation

### Runtime Path

Validates that runtime directories exist and are writable:

- `user_data/`
- `config/`
- `data/`
- `logs/`
- `data/db/`
- `data/videos/`
- `data/screenshots/`
- `data/exports/`

Existing runtime data is detected and reported so setup can preserve it.

### OBS Integration

Reports Worker API and Worker version compatibility information. The Plugin still owns actual OBS process integration and Worker launch.

For the v1.1 OBS Dock setup path, the Plugin performs the minimum preflight
needed before Worker launch:

- create `user_data_dir` when possible,
- verify that `user_data_dir` is writable,
- probe Worker health on the configured endpoint,
- surface Worker launch failure and API incompatibility as distinct Dock setup
  actions.

This Dock setup path is intentionally smaller than the full Worker setup wizard.
OAuth and template readiness remain separate setup steps unless they are needed
for minimum manual recording.

### OAuth

Checks whether the YouTube client secret and OAuth token files exist under:

```text
user_data/config/secrets/
```

The validation result reports only file presence and safe paths. It never returns secret contents.

### Templates

Checks `user_data/config/templates.toml` and local template files through the existing detection configuration loader. Missing, invalid, unreadable, or empty templates are reported as actionable diagnostics.

The assisted registration endpoint can create this config without requiring a
user to hand-edit TOML, while preserving the same detection loader and
diagnostic behavior.

---

## Recovery Rules

- Setup can be rerun at any time through `POST /setup/reset`.
- Cancelled setup can be resumed because completed steps are preserved.
- Existing DBs, screenshots, videos, exports, config, and logs are not deleted by setup actions.
- Invalid setup state files return `setup_state_invalid` and require manual repair or reset.
