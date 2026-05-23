# Match Metadata

v1.1 introduces Worker-owned match metadata. Metadata is stored on the existing `matches` table and is exposed through the Worker API.

The metadata boundary supports:

- opponent deck input
- match memo input
- result and timestamp editing
- deterministic title generation
- deterministic description and notes generation
- upload metadata handoff to the v1.0 upload flow

---

## Fields

Editable fields:

- `deck_name`
- `opponent_deck`
- `result`
- `memo`
- `started_at`
- `ended_at`
- `title_template`

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

---

## Upload Metadata

`GET /matches/{match_id}/upload-metadata` returns deterministic upload metadata:

- `title`
- `description`
- `notes`
- supported template `variables`

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

---

## Upload Integration

When a queue item has `match_id`, `POST /upload/process-next` includes generated `upload_metadata` in the response. The v1.0 mocked upload boundary does not perform real YouTube metadata writes, but v1.1 makes the title and description handoff explicit for the future real upload client.

