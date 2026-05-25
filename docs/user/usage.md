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
- metadata editing for the latest completed recording
- upload retry
- upload discard
- mark uploaded after confirming the YouTube video id
- queue recovery visibility

Retrying an item that needs manual review can create a duplicate YouTube upload.
Use Retry only after checking YouTube. Use Discard when the item should no
longer be uploaded, and Mark Uploaded only when the video already exists on
YouTube.

---

## Match Memo

Users can:
- edit deck name, opponent deck, result, rank, DP, and memo from the Dock
- input opponent deck
- add notes
- upload with metadata

Invalid metadata values are rejected by the Worker without replacing the saved
match record.
