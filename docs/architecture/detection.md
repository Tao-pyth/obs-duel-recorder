# Template Detection Design

## Scope

v0.8 introduces the Template Matching MVP for duel start/end detection.

The Python Worker owns:
- local template configuration loading
- local template file loading
- bounded template matching
- duel lifecycle state transitions
- automatic recording trigger requests through the v0.6 recording boundary

The repository must not include game assets, generated screenshots, local user templates, or other runtime media.

---

## Template Configuration

Default config path:

```text
user_data/config/templates.toml
```

Default template directory:

```text
user_data/templates/
```

Example:

```toml
[detection]
start_confirmations = 2
end_confirmations = 2

[[templates]]
name = "duel_start"
kind = "duel_start"
path = "duel-start.tpl"
threshold = 1.0

[[templates]]
name = "duel_end"
kind = "duel_end"
path = "duel-end.tpl"
threshold = 1.0
```

Rules:
- Missing config is allowed and produces an empty detection template set.
- Relative template paths resolve from `user_data/config/` first, then `user_data/templates/`.
- `kind` must be `duel_start` or `duel_end`.
- `threshold` is a number from `0.0` to `1.0`.
- Missing or empty template files are reported as diagnostics and are not matched.

The setup API also provides an assisted registration workflow:

```text
POST /setup/templates/register
```

It accepts a start/end kind, local template path, threshold, and confirmation
count, then writes the same `templates.toml` format shown above. This keeps the
manual and guided paths on one compatible detection configuration contract.

Template matching can be tested without changing lifecycle state or sending a
recording command:

```text
POST /detection/test
```

The test endpoint accepts the same `frame_text` or `frame_hex` fixture payload
as `/detection/frame`, plus optional `kind` (`start`, `end`, `duel_start`, or
`duel_end`). It returns per-template `name`, `kind`, `score`, and `matched`
fields with diagnostics such as `template_match_missing` or
`templates_not_configured`.

---

## MVP Matching

The v0.8 MVP uses deterministic local byte matching:

- a template matches when its bytes are present in the submitted frame bytes
- score is `1.0` for a byte match and `0.0` otherwise
- submitted frames are test/smoke inputs through `frame_text` or `frame_hex`

This intentionally avoids adding image-processing dependencies or committing real game assets. Later versions can replace the matcher behind the same Worker API boundary.

---

## Duel Lifecycle

Lifecycle states:

- `no_duel`
- `potential_duel`
- `active_duel`
- `ended_duel`

Start detection:
- repeated `duel_start` matches increment `start_count`
- once `start_confirmations` is reached, lifecycle becomes `active_duel`
- Worker sends recording command `start` with source `automatic`
- OBS Plugin heartbeat reads `/recording/state`; when it sees
  `state=starting` and `command_source=automatic`, the Dock/UI thread requests
  OBS recording start and confirms the Worker when OBS reports recording started

End detection:
- while active, repeated `duel_end` matches increment `end_count`
- once `end_confirmations` is reached, lifecycle becomes `ended_duel`
- Worker sends recording command `stop` with source `automatic`
- OBS Plugin heartbeat reads `/recording/state`; when it sees
  `state=stopping` and `command_source=automatic`, the Dock/UI thread requests
  OBS recording stop and confirms the Worker when OBS reports recording stopped

Skipped recording commands are returned as events, not raised as detection failures, so detection remains diagnosable even when recording state is not ready for a transition.

OCR and image-recognition candidate APIs are not recording triggers. They may
suggest metadata after or around a match, but start/stop ownership remains with
template detection and the recording state boundary.

---

## Manual Smoke

Use only local user-provided template/sample data.

1. Start OBS with the Plugin loaded and Worker `running`.
2. Register local start/end templates through `POST /setup/templates/register`.
3. Verify samples through `POST /detection/test`; do not continue until the
   intended start and end samples return `matched=true`.
4. Submit repeated start frames to `POST /detection/frame` until
   `duel_started` is returned. OBS should start recording and
   `/recording/state` should move through `starting` to `recording` after the
   OBS frontend started event confirms the Worker.
5. Submit repeated end frames to `POST /detection/frame` until `duel_ended` is
   returned. OBS should stop recording and `/recording/state` should move
   through `stopping` to `completed` after the OBS frontend stopped event
   confirms the Worker.
6. If the Worker rejects a start/stop transition, inspect the returned
   `recording_*_skipped:*` event, the Dock `Recording` row, and OBS logs.
