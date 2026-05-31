# Export System

v1.2 introduces a Worker-owned export boundary for runtime archive backups.

Exports are created under:

```text
user_data/data/exports/
```

The export operation does not mutate live runtime state. SQLite is copied through a snapshot before packaging, and the final ZIP is written through a temporary file before being renamed into place.

---

## API Surface

### `POST /exports`

Creates one ZIP export and returns the completed output location.

Request fields:

- `created_at`: optional ISO-like timestamp used for deterministic naming.
- `name`: optional ZIP file name or stem. Invalid path characters are normalized.
- `include_videos`: optional boolean. Defaults to `false`.

Default file name:

```text
odr-export-<created_at>.zip
```

If the target ZIP already exists, the request fails with `export_path_conflict` and does not replace the existing archive.

### `GET /exports`

Lists ZIP exports currently present in `user_data/data/exports/`.

### `POST /exports/registration-csv`

Creates a CSV report of current match registration and upload queue status.

Request fields:

- `save_dir`: target directory for the CSV file. The Worker creates the
  directory when it is missing.

Default file name:

```text
odr-registration-YYYYMMDD-HHMMSS.csv
```

The CSV is written with UTF-8 BOM so spreadsheet tools can open Japanese text
more reliably. The export is read-only against SQLite and does not mutate match
records or queue state.

CSV fields include match ID, recording session ID, deck names, result, rank,
DP, memo, queue item ID, queue status, video path, YouTube video ID, YouTube
URL, and queue error/manual-review fields. OAuth tokens, client secrets, logs,
and local media bytes are never included.

Beginner note: this CSV export is for checking registration status in a
spreadsheet. It is separate from the ZIP backup export and does not replace the
database.

---

## ZIP Structure

Required entries:

```text
manifest.json
database/odr.sqlite3
metadata/matches.json
metadata/upload_queue.json
metadata/screenshots.json
metadata/video_linkages.json
```

Screenshot entries:

```text
screenshots/<relative_path from screenshots table>
```

Video entries are included only when `include_videos` is `true`:

```text
videos/queue-<queue_item_id>-<file_name>
```

The default video behavior is linkage-only. The export records `video_path`, `youtube_video_id`, and `youtube_url` in `metadata/video_linkages.json` and the manifest.

---

## Manifest Contract

`manifest.json` contains:

- `export_version`: v1.2 export format version.
- `created_at`: timestamp used for this export.
- `file_name`: ZIP file name.
- `app_version`: Worker version.
- `api_version`: Worker API version.
- `schema_version`: SQLite schema version.
- `include_videos`: whether video files were included.
- `included_artifacts`: database, metadata, screenshot, and optional video entries.
- `missing_files`: referenced screenshot/video files that were not present on disk.
- `counts`: match, queue, screenshot, video linkage, and included-file counts.
- `exclusions`: config, logs, OAuth tokens, client secrets, temporary files, and default video behavior.

---

## Exclusion Rules

The export excludes:

- `user_data/config/`
- OAuth token files
- YouTube client secret files
- `user_data/logs/`
- temporary files
- generated export work files

Videos are excluded by default and represented as linkages. This keeps the default export small while preserving enough information to audit uploaded videos.

---

## Failure Behavior

- Failed exports do not leave a new completed ZIP.
- Existing completed ZIPs are never overwritten.
- Temporary ZIP files are removed on export failure.
- Missing screenshots or videos are recorded in `manifest.json` instead of failing the whole export.
