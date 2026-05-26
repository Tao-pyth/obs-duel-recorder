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

As of `v1.1.2`, the Dock uses tabs to keep normal OBS operation compact:

- `Record`: recording, upload review, and metadata actions
- `Setup`: Worker readiness and next action
- `Settings`: runtime path, theme, and language settings
- `Auto`: automatic recording template registration and detection tests
- `Help`: task-focused help and language recovery text
- `Diagnostics`: endpoint, user data, Worker path, logs, ownership, and detail

Retrying an item that needs manual review can create a duplicate YouTube upload.
Use Retry only after checking YouTube. Use Discard when the item should no
longer be uploaded, and Mark Uploaded only when the video already exists on
YouTube.

---

## Language

Open the `Settings` tab, then `Open Settings`, to switch the Dock UI language
between English and Japanese. The selected language is saved in the Plugin
settings file and is restored after OBS restarts.

The Help message includes a language-change note in the opposite language so a
user can recover if the UI is changed to an unfamiliar language.

---

## Match Memo

Users can:
- edit deck name, opponent deck, result, rank, DP, and memo from the Dock
- input opponent deck
- add notes
- upload with metadata

Invalid metadata values are rejected by the Worker without replacing the saved
match record.

Use Preview Upload Metadata before upload to inspect the generated YouTube title
and description. If the preview reports missing fields, use Edit Metadata and
preview again before retrying upload.
