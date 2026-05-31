# OBS Sources Used By The Plugin

Status contract: This page describes the current OBS Text Source behavior used by the Plugin and the v1.1.1 documentation target.

The OBS Duel Recorder Plugin can create or reuse a small set of OBS Text Sources for overlay display. These are optional visual helpers; they are not the video files, local detection templates, screenshots, OAuth files, or game assets.

## Source List

| Field | Default OBS source name | Default text | Meaning |
|---|---|---|---|
| deck name | `ODR Deck Name` | `Deck: -` | User-facing deck label. |
| sequence number | `ODR Sequence` | `#---` | Per-own-deck usage sequence for the current or next recording. |
| result | `ODR Result` | `Result: unknown` | Duel result display. This may stay unknown until user review or recognition suggestions are applied. |
| opponent deck | `ODR Opponent Deck` | `Opponent: unknown` | Opponent deck label. |
| recording state | `ODR Recording State` | `Idle` | Display-only recording state text. |

The names above are OBS source names. They are intentionally stable so documentation, smoke tests, and troubleshooting can find them.

## Creation And Reuse Behavior

When overlay support is enabled:

1. The Plugin checks each configured source name.
2. If a supported OBS Text Source already exists with that name, the Plugin reuses it.
3. If the source is missing and `auto_create_sources` is enabled, the Plugin creates the missing Text Source in the current OBS scene.
4. If `auto_create_sources` is disabled, the Plugin reports the missing source and skips that field.
5. If more than one source has the same configured name, the Plugin reports a duplicate-source diagnostic and skips that field.
6. If a source exists with the configured name but is not a supported text source, the Plugin reports an unsupported-source diagnostic and does not replace or delete it.

Supported text source kinds are OBS text sources such as `text_gdiplus` or `text_ft2_source`, depending on the OBS build.

## Update Behavior

The Plugin writes text to the configured sources from Worker overlay state:

| Source | Updated from |
|---|---|
| `ODR Deck Name` | deck name payload or configured default |
| `ODR Sequence` | sequence number payload or configured default |
| `ODR Result` | result payload or configured default |
| `ODR Opponent Deck` | opponent deck payload or configured default |
| `ODR Recording State` | recording state mapping: `idle`, `recording`, `paused`, `unknown`, or configured default |

The Worker does not call OBS APIs directly. The Worker stores overlay state and the Plugin applies it to OBS Text Sources.

The update trigger is explicit:

- At Plugin startup, the Plugin creates or reuses the configured OBS Text Sources.
- The Plugin checks Worker overlay state during its normal refresh loop and writes to OBS only when the payload changed.
- When recording starts from the Dock, the Plugin sends the current own-deck name and the next per-deck sequence number to Worker overlay state, then applies the same values to OBS Text Sources.
- When match metadata is saved, Worker persists the metadata and updates overlay state with deck name, opponent deck, and sequence number. The Plugin then applies the saved values to OBS Text Sources.
- If recording is active before a match row exists, the Dock Save action updates the OBS Text Sources from the current metadata controls. Save again after the match target appears to persist the DB metadata.
- Recording state is updated by the Worker recording state machine when manual or automatic recording commands change state.
- Result may remain `unknown` until the user enters or confirms the duel result.

Deck sequence numbers are not upload queue ids and are not match ids. They are assigned per own-deck name when metadata is saved. If a saved match is moved to another deck name, it receives the next number for the new deck name; older numbers are not compacted so existing recordings remain stable.

## Safe Customization

Safe:
- Move the Text Sources in the OBS preview.
- Resize, crop, color, font-style, or otherwise style the Text Sources in OBS.
- Hide a source if you do not want it visible.
- Put the sources in a dedicated overlay scene or group after creation.

Use care:
- Renaming a source means the Plugin cannot find it by the default name unless settings are updated to the new name.
- Duplicating a source with the same name can make updates ambiguous.
- Changing the source type from text to another OBS source type prevents updates.
- Deleting a source is allowed, but the Plugin may recreate it later when auto-create is enabled and the current scene is active.

## What The Plugin Does Not Create

The Plugin does not create or distribute:
- Yu-Gi-Oh! Master Duel images or game assets,
- local start/end detection template images,
- screenshots or recorded videos,
- browser sources for YouTube,
- OAuth client files, tokens, or secrets,
- OBS scenes, scene collections, or transitions.

## Troubleshooting

| Symptom | Likely cause | Action |
|---|---|---|
| Source is not visible | Source is hidden, behind another source, outside the canvas, or in a different scene | Check the current scene, source visibility, and transform. |
| Source text does not update | Worker is not running, metadata was not saved, source name changed, source type is unsupported, or duplicate names exist | Save metadata, check Dock diagnostics, and restore the expected source names. |
| Source was not created | Auto-create is disabled or OBS has no supported text source kind | Enable source creation or create supported text sources manually. |
| Duplicate diagnostic appears | More than one source has the same configured name | Rename or remove duplicates so only one source uses each expected name. |

Related pages:
- [Operation flow and system overview](operation-flow.md)
- [First setup](setup.md)
- [Troubleshooting](troubleshooting.md)
