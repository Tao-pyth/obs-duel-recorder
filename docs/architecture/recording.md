# Recording State Machine

Recording lifecycle coordination is split between the OBS Plugin and the Python Worker.

The OBS Plugin owns OBS recording control and lifecycle events.

The Python Worker owns queue creation, recovery decisions, and persistent runtime state.

```mermaid
stateDiagram-v2
    [*] --> idle

    idle --> recording_pending: duel start detected or manual start
    recording_pending --> recording: OBS recording started
    recording_pending --> idle: start rejected or cancelled

    recording --> stopping_pending: duel end detected or manual stop
    stopping_pending --> recorded: OBS recording stopped and file finalized
    stopping_pending --> recording: stop rejected or cancelled

    recorded --> queued: Worker creates upload queue item
    queued --> idle: queue item persisted

    recording --> interrupted: OBS or Worker shutdown
    recording_pending --> interrupted: startup interrupted
    stopping_pending --> interrupted: shutdown during stop
    interrupted --> idle: discard interrupted recording during recovery
```

## Notes

- OCR must not be the primary recording trigger mechanism.
- Interrupted recording sessions should be discarded during recovery.
- Queue persistence is handled by the Python Worker, not the OBS Plugin.
