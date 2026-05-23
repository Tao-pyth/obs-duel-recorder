# Requirements

## Overview

OBS Duel Recorder is an OBS Studio based duel recording assistant for Yu-Gi-Oh! Master Duel.

The project automatically:
- records duel segments
- manages match history
- uploads videos to YouTube
- stores duel metadata
- manages overlays and queue states

---

## Architecture

The system separates responsibilities into:

| Component | Responsibility |
|---|---|
| OBS Plugin | OBS integration, overlay control, Dock UI, Worker lifecycle |
| Python Worker | Queue processing, SQLite, OCR, template matching, screenshot archive, upload processing |

---

## Platform

- Windows only
- OBS latest stable version
- SQLite based persistence

---

## Queue States

- ready_upload
- uploading
- uploaded
- upload_failed
- quota_waiting
- need_manual_review

---

## Upload Rules

- Upload success uses videos.insert response
- youtube_video_id must be stored
- youtube_url must be stored
- quota exceeded waits for YouTube quota reset
- OAuth scope must be limited to `https://www.googleapis.com/auth/youtube.upload`
- Default upload privacy must be `private`
- Tokens, client secrets, authorization codes, and bearer strings must be redacted from diagnostics
- Missing local videos are discarded with evidence
- Ambiguous upload outcomes require manual review instead of blind retry

---

## Match Data

- Match metadata is Worker-owned and persisted in SQLite.
- Editable match metadata includes deck name, opponent deck, result, memo, started time, ended time, and title template.
- Metadata validation failures must be actionable and must not change the existing record.
- Upload title, description, and notes generation must be deterministic.
- Post-upload metadata edits update future generated metadata but do not rewrite an already uploaded YouTube record automatically.

---

## Overlay Rules

Overlay supports:
- deck name
- sequence number
- result
- opponent deck

---

## Detection Rules

Primary:
- state transitions
- template matching

Secondary:
- OCR

OCR must not be the primary trigger mechanism.

---

## Screenshot Rules

- Screenshot files are runtime data under `user_data/data/screenshots/`.
- Screenshot metadata is Worker-owned and persisted in SQLite.
- Screenshots can link to match and upload queue records.
- Upload cleanup must preserve screenshots needed for failure diagnosis or manual review.
- The repository must not distribute runtime screenshots or game assets.

---

## Runtime Rules

Runtime data must be separated from application binaries.

user_data must persist across updates.
