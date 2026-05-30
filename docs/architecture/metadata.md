# Match Metadata

v1.1 introduces Worker-owned match metadata. Metadata is stored on the existing `matches` table and is exposed through the Worker API.

The metadata boundary supports:

- opponent deck input
- match memo input
- rank and DP input
- result and timestamp editing
- deterministic title generation
- deterministic description and notes generation
- upload metadata handoff to the v1.0 upload flow
- durable linkage from a completed manual recording session to a pending match
  metadata row

---

## Fields

Editable fields:

- `deck_name`
- `opponent_deck`
- `result`
- `rank`
- `dp`
- `memo`
- `started_at`
- `ended_at`
- `title_template`

System-managed fields:

- `recording_session_id`

Rules:

- Missing fields default to an empty string.
- `null` clears a field.
- Field values are trimmed.
- Overlong values are rejected without changing the existing record.
- Post-upload edits are allowed; they update future generated metadata but do not rewrite an already uploaded YouTube record automatically.

Length limits:

- `deck_name`: 120
- `opponent_deck`: 120
- `result`: 40
- `rank`: 80
- `dp`: 40
- `memo`: 4000
- `started_at`: 64
- `ended_at`: 64
- `title_template`: 200

---

## Search

`GET /matches?query=<text>` searches:

- `opponent_deck`
- `memo`
- `deck_name`
- `result`

Search is intended as a lightweight local lookup boundary. Statistics and advanced search remain later work.

`GET /matches/latest` returns the newest match row for the OBS Dock metadata
editor. The Dock uses this only as a convenience entry point; all saved edits
still go through `PUT /matches/{match_id}/metadata`, so Worker validation
remains the single write boundary.

---

## Upload Metadata

`GET /matches/{match_id}/upload-metadata` returns deterministic upload metadata:

- `title`
- `description`
- `notes`
- supported template `variables`
- `missing_fields`
- `warning`

Default title template:

```text
Duel {match_id} vs {opponent_deck} - {result}
```

Supported variables:

- `match_id`
- `deck_name`
- `opponent_deck`
- `result`
- `started_at`
- `ended_at`
- `created_at`

Missing values use stable fallbacks such as `Unknown Opponent` or `unknown`. Unknown template variables render as `unknown`.

Title length is capped at 100 characters. Description length is capped at 5000 characters. Truncation is deterministic.

The OBS Dock Template tab calls this same endpoint for the selected match and
displays the generated title, description, and tags without starting an upload.
When required metadata is missing, the preview shows the Worker warning and
directs the user back to metadata editing.

---

## Upload Integration

When a queue item has `match_id`, `POST /upload/process-next` includes generated `upload_metadata` in the response. The v1.0 mocked upload boundary does not perform real YouTube metadata writes, but v1.1 makes the title and description handoff explicit for the future real upload client.

---

## Recording Completion Handoff

When a manual recording reaches `completed`, the Worker creates or reuses one
match metadata row for that recording `session_id`. The row is intentionally
pending: unknown deck, result, and opponent fields remain blank until later
metadata editing or recognition assistance fills them.

If the Plugin supplies the completed recording `video_path`, the same handoff
creates or reuses one `ready_upload` queue item linked to the pending match. If
the path is unavailable, the response reports `pending_output_path` and leaves
queue creation pending instead of inventing a media path.

Repeated `confirm_stopped` calls for the same recording session return the same
`match_id` and queue item, and must not create duplicate match or queue rows.
Interrupted or failed recordings do not create completed match rows.

---

## Dock Editing

The OBS Dock provides an `Edit Metadata` action for the latest completed match
record. Users can edit deck name, opponent deck, result, rank, DP, and memo
without calling localhost APIs manually.

The Dock displays Worker validation failures and leaves the existing metadata
record unchanged when a value is rejected. OCR and image-recognition candidates
remain suggestions; they are never applied by opening or saving the Dock editor
unless the user explicitly submits metadata through the Worker update API.
