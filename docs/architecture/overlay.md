# Overlay Architecture

This document defines the OBS overlay integration contract for v0.5.

Tracking issue: #288
Contract issue: #291

## Scope

v0.5 establishes controlled OBS Text Source updates for a fixed set of overlay fields.

In scope:

- fixed Text Source ownership and naming
- overlay settings defaults
- deterministic source creation/reuse behavior
- display-only overlay state updates
- diagnostics for missing, duplicated, unsupported, or failed updates

Out of scope:

- the v0.6 recording lifecycle state machine
- automatic duel detection
- upload metadata generation
- a full visual overlay designer
- deleting unrelated user-created OBS sources

## Overlay Fields

The v0.5 overlay field set is:

| Field | Default source name | Default empty text | Notes |
|---|---|---|---|
| deck name | `ODR Deck Name` | `Deck: -` | User-facing deck label. |
| sequence number | `ODR Sequence` | `#---` | Display sequence for the current/next recording. |
| result | `ODR Result` | `Result: unknown` | May remain `unknown` until later detection or manual input. |
| opponent deck | `ODR Opponent Deck` | `Opponent: unknown` | Stable overlay requirement from `docs/requirements/requirements.md`. |
| recording state | `ODR Recording State` | `Idle` | Display-only in v0.5; authoritative state machine remains v0.6. |

These names are source names, not localized labels. They should remain stable so smoke tests and user setup guides can identify them.

## Source Ownership

The Plugin owns only sources it creates with the default names above or sources explicitly configured in overlay settings.

Rules:

1. If the configured source exists and is a supported OBS Text Source, reuse it.
2. If the configured source is missing and `auto_create_sources` is true, create it in the current scene.
3. If the configured source is missing and `auto_create_sources` is false, report a diagnostic and skip that field update.
4. If multiple sources match a configured name, report `overlay_source_duplicate` and do not update that field until the ambiguity is resolved.
5. If the source exists but is not a supported text source, report `overlay_source_unsupported` and do not replace or delete it.
6. The Plugin must not delete user-created sources as part of v0.5 overlay management.

## Supported Source Types

The initial supported OBS source kind is a text source available in the running OBS build.

On Windows, OBS commonly exposes `text_gdiplus_v3` or a compatible text source kind depending on version. The Plugin should detect the available text source kind rather than hard-coding behavior that only works for one OBS minor release.

If no supported text source kind is available, source creation must fail with `overlay_text_source_unavailable`.

## Overlay Settings

Overlay settings are part of the Plugin settings JSON persisted under:

```text
%APPDATA%\obs-duel-recorder\plugin-settings.json
```

The initial v0.5 shape should be additive and backward-compatible with the v0.4 settings file:

```json
{
  "host": "127.0.0.1",
  "port": 8787,
  "user_data_dir": "C:\\path\\to\\user_data",
  "restart_worker_on_change": true,
  "overlay": {
    "enabled": true,
    "auto_create_sources": true,
    "sources": {
      "deck_name": "ODR Deck Name",
      "sequence_number": "ODR Sequence",
      "result": "ODR Result",
      "opponent_deck": "ODR Opponent Deck",
      "recording_state": "ODR Recording State"
    },
    "defaults": {
      "deck_name": "Deck: -",
      "sequence_number": "#---",
      "result": "Result: unknown",
      "opponent_deck": "Opponent: unknown",
      "recording_state": "Idle"
    }
  }
}
```

Missing `overlay` settings must load with these defaults. Existing v0.4 settings must remain valid.

## Update Payload

The v0.5 overlay state payload should be small and explicit:

```json
{
  "deck_name": "Sample Deck",
  "sequence_number": "001",
  "result": "unknown",
  "opponent_deck": "Unknown Opponent",
  "recording_state": "idle"
}
```

Field behavior:

- missing field: keep the previous displayed value or configured default, according to the implementation path documented in the PR
- empty string: use the configured default for that field
- overlong value: truncate or reject with a diagnostic; do not crash OBS
- unknown enum-like value: display `unknown` or a configured default and log a diagnostic

## Worker API Boundary

The Worker stores the current overlay state in memory and exposes it over the existing localhost API.

Routes:

- `GET /overlay/state`: returns the current overlay state payload.
- `PUT /overlay/state`: validates and applies a partial overlay state update, then returns the full current state.

`PUT /overlay/state` accepts any subset of the overlay fields. Missing fields preserve the previous value. Unknown fields, non-string values, overlong values, and unknown `recording_state` values return `400` with `code: "overlay_payload_invalid"`.

The Plugin polls `GET /overlay/state` through its Worker API client. Actual Text Source writes are handled by the Plugin; the Worker does not call OBS APIs and does not own recording lifecycle decisions.

## Recording-State Boundary

v0.5 may display recording-state text, but it must not become the owner of recording lifecycle decisions.

Allowed v0.5 states:

- `idle`
- `recording`
- `paused`
- `unknown`

The v0.6 state machine may later replace or refine these values. v0.5 should treat them as display inputs only.

The v0.5 Plugin display mapping is intentionally narrow:

- `idle` -> `Idle`
- `recording` -> `Recording`
- `paused` -> `Paused`
- `unknown` -> `Unknown`

The Plugin writes this display text only to the configured recording-state Text Source. It does not start, stop, pause, resume, infer, or persist recording lifecycle state.

## Diagnostics

Overlay diagnostics should use stable names in logs and Dock/status evidence where practical:

- `overlay_source_missing`
- `overlay_source_duplicate`
- `overlay_source_unsupported`
- `overlay_text_source_unavailable`
- `overlay_update_failed`
- `overlay_payload_invalid`
- `overlay_settings_invalid`

Diagnostics should include:

- field name
- configured source name
- action attempted (`create`, `reuse`, `update`, `skip`)
- short reason

Diagnostics must not include secrets or runtime media content.

## Smoke Evidence

The v0.5 smoke procedure lives in:

- `docs/architecture/v0.5-overlay-smoke.md`

Accepted smoke evidence must be posted to #296 before closing source management and field-update implementation issues.
