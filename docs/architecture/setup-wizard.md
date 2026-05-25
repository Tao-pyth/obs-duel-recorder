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

### `POST /setup/steps/{step_id}/complete`

Marks one step complete by default.

Request:

```json
{"completed": true}
```

Passing `{"completed": false}` marks the step incomplete.

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

---

## Recovery Rules

- Setup can be rerun at any time through `POST /setup/reset`.
- Cancelled setup can be resumed because completed steps are preserved.
- Existing DBs, screenshots, videos, exports, config, and logs are not deleted by setup actions.
- Invalid setup state files return `setup_state_invalid` and require manual repair or reset.
