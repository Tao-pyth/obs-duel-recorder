# Usage

Status note: This page may include planned (not-yet-released) usage flows, including automatic recording, manual controls, retry, and queue recovery. Confirm release availability in the [Roadmap](../roadmap.md) before treating a flow as current.

## Automatic Recording

Automatic recording is driven by template detection:

- repeated `duel_start` template matches request recording start
- repeated `duel_end` template matches request recording stop
- the OBS Plugin watches Worker recording state and calls OBS Start/Stop
- Worker diagnostics report skipped transitions if the current recording state
  rejects an automatic request

OCR and image-recognition candidates are metadata suggestions only. They do not
start or stop recording.

---

## Manual Control

The Dock UI supports:
- manual start
- manual stop
- retry
- queue recovery

---

## Match Memo

Users can:
- input opponent deck
- add notes
- upload with metadata
