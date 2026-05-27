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
- upload title and description preview for the latest completed recording
- upload retry
- upload discard
- mark uploaded after confirming the YouTube video id
- queue recovery visibility

As of `v1.1.3`, the Dock follows the normal operation order:

- `Record`: recording state, output identity, and manual start/stop
- `Metadata`: latest match fields, deck/opponent dropdowns, memo, and save status
- `Upload`: upload queue actions plus title, description, and tag template preview
- `Manage`: setup readiness, inline settings, Help, automatic setup, and diagnostics details

Retrying an item that needs manual review can create a duplicate YouTube upload.
Use Retry only after checking YouTube. Use Discard when the item should no
longer be uploaded, and Mark Uploaded only when the video already exists on
YouTube.

---

## Language

Open the `Manage` tab to switch the Dock UI language between English and
Japanese. The selected language is saved in the Plugin settings file and is
restored after OBS restarts.

The Help message includes a language-change note in the opposite language so a
user can recover if the UI is changed to an unfamiliar language.

---

## Match Memo

Users can:
- edit deck name, opponent deck, result, rank, DP, and memo directly from the Dock
- select or type deck and opponent deck names
- reuse previous deck, opponent deck, rank, and DP values when the next match starts empty
- add notes
- edit upload title, description, and tag templates before upload

Invalid metadata values are rejected by the Worker without replacing the saved
match record.

Use the Upload preview before upload to inspect the generated YouTube title,
description, and tags. If the preview reports missing fields, save Metadata and
preview again before retrying upload.
