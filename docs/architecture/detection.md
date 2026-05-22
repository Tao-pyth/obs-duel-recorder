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

End detection:
- while active, repeated `duel_end` matches increment `end_count`
- once `end_confirmations` is reached, lifecycle becomes `ended_duel`
- Worker sends recording command `stop` with source `automatic`

Skipped recording commands are returned as events, not raised as detection failures, so detection remains diagnosable even when recording state is not ready for a transition.
