# Screenshot System

v0.9 introduces a Worker-owned screenshot archive boundary.

The screenshot system is intentionally local-first:

- Screenshot files live under `user_data/data/screenshots/`.
- Screenshot metadata lives in SQLite.
- The OBS Plugin and future upload workers use the Worker API rather than writing screenshot files or DB rows directly.
- Runtime screenshots are never committed to git.

---

## Capture

Screenshots are captured through:

- `POST /screenshots/capture`

Request fields:

- `match_id`: optional integer. When provided, it must reference an existing `matches.id`.
- `queue_item_id`: optional integer. When provided, it must reference an existing `upload_queue.id`.
- `kind`: optional string, default `duel`.
- `captured_at`: optional UTC timestamp string. The timestamp is used in the deterministic file name.
- `content_type`: optional string, default `application/octet-stream`.
- `extension`: optional file extension. If omitted, `image/png` maps to `png`, `image/jpeg` maps to `jpg`, and other content maps to `bin`.
- `content_base64`: base64 encoded screenshot bytes, or
- `content_text`: UTF-8 fixture text for tests and deterministic smoke checks.

Exactly one content source should be supplied. Empty screenshot content is rejected.

---

## Naming Rules

The Worker generates relative paths in this shape:

```text
match-<match_id-or-none>/<timestamp>-<kind>-queue-<queue_item_id-or-none>.<extension>
```

Examples:

```text
match-42/20260523T040000Z-duel-start-queue-7.png
match-none/20260523T040100Z-preview-queue-none.bin
```

Rules:

- `kind` is normalized to lowercase ASCII letters, numbers, `_`, and `-`.
- timestamps are stripped to a path-safe alphanumeric prefix.
- extensions are limited to lowercase ASCII letters and numbers.
- generated paths are resolved under `user_data/data/screenshots/`; path traversal is rejected.
- existing screenshot paths are not overwritten.

---

## DB Linkage

The `screenshots` table links each archive entry to optional match and queue context:

- `match_id` references `matches.id`
- `queue_item_id` references `upload_queue.id`
- `relative_path` is unique
- `status` is one of `available`, `missing`, or `deleted`

If a DB foreign-key or uniqueness check fails, the Worker returns a stable screenshot error and removes the newly written file so the archive does not keep orphaned bytes.

---

## Preview

Screenshot previews are read through:

- `GET /screenshots/{screenshot_id}/preview`

When the file exists and the DB row is `available`, the response includes:

- `available: true`
- `record`
- `content_base64`
- `content_type`

If the DB row exists but the file is missing, the Worker marks the record `missing` and returns:

- `available: false`
- `record`

---

## Upload Cleanup

Upload-related cleanup is explicit and queue-state aware:

- `POST /screenshots/cleanup`

Request:

```json
{"queue_item_id": 7}
```

Cleanup deletes available screenshot files and marks rows `deleted` only when the queue item is in a terminal cleanup-safe state:

- `uploaded`
- `discarded`

Cleanup preserves screenshots for diagnosable states:

- `ready_upload`
- `uploading`
- `upload_failed`
- `quota_waiting`
- `need_manual_review`

This keeps failure evidence available for manual review and retry decisions.

