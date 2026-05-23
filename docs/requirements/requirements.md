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
| Python Worker | Queue processing, SQLite, image recognition, template matching, screenshot archive, upload processing |

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
- Upload execution must use a provider boundary so tests can use a deterministic mock uploader and production can use an optional Google uploader.
- The Google uploader should use the official Google client libraries as optional dependencies.
- Google API failures must be classified by HTTP status and API reason into stable outcomes such as quota, auth, network, and ambiguous failure.
- Setup wizard and `/upload/status` must detect missing OAuth prerequisites before a real upload attempt.

---

## Match Data

- Match metadata is Worker-owned and persisted in SQLite.
- Editable match metadata includes deck name, opponent deck, result, memo, started time, ended time, and title template.
- Metadata validation failures must be actionable and must not change the existing record.
- Upload title, description, and notes generation must be deterministic.
- Post-upload metadata edits update future generated metadata but do not rewrite an already uploaded YouTube record automatically.

---

## Statistics Rules

- Statistics are Worker-owned and derived from SQLite runtime data.
- Statistics must not rewrite source records merely to improve aggregates.
- Empty datasets must return stable zero-count responses.
- Statistics filters must be documented and deterministic.
- Statistics diagnostics must not expose OAuth secrets, logs, local media contents, or unnecessary absolute paths.

---

## Export Rules

- Exports are Worker-owned and written under `user_data/data/exports/`.
- Export archives must include a manifest that records app version, API version, schema version, included artifacts, and missing files.
- Export creation must not mutate live runtime state.
- Failed exports must not leave misleading completed archives.
- OAuth tokens, client secrets, logs, temporary files, and config secrets must be excluded by default.
- Videos are linkage-only by default; explicit video inclusion must be opt-in.

---

## Setup Rules

- Setup wizard state is runtime data and must live under `user_data/data/`.
- Setup actions must not delete, move, overwrite, or migrate existing runtime data.
- First-run, partial, complete, cancel, reset, and rerun states must be distinguishable.
- Setup validation must report actionable diagnostics for runtime path, OBS integration, OAuth, and templates.
- OAuth validation must never return token or client-secret contents.
- Local template setup must not distribute or commit game assets.

---

## Update Rules

- `update.bat` is the canonical Windows update entrypoint.
- Update validation must check target Worker/API compatibility before mutating runtime state.
- Downgrades must be rejected unless a future release explicitly documents a supported path.
- SQLite DB backup must be created before update-triggered migration execution when a DB exists.
- Update state, installed version records, and DB backups are runtime data under `user_data/data/`.
- Normal update must preserve OAuth tokens, DB, videos, screenshots, exports, logs, and runtime root overrides.
- Partial update and migration failure diagnostics must point to backup/recovery guidance.
- Update diagnostics must not expose OAuth secrets, tokens, local media contents, or full logs.

---

## Packaging Rules

- Release packaging should be produced as a GitHub Actions ZIP artifact.
- The ZIP layout must include the Plugin DLL, Worker package, `update.bat`, and minimal user/developer docs needed for install/update.
- `user_data/`, logs, databases, screenshots, videos, OAuth files, and generated runtime exports must never be included in release ZIPs.
- Release assets should include a SHA256 checksum.
- GitHub Actions may generate release assets automatically, but final release attachment/publishing should remain manually approved unless a later release defines a fully automated policy.

---

## Image Recognition Rules

- v2.0 image analysis is image-recognition-assisted metadata extraction, not mandatory OCR text recognition.
- Fixture-based recognition must be implemented first through an abstract Worker provider boundary.
- Pillow is allowed for lightweight image preprocessing; heavyweight OCR or ML runtime dependencies are not required for v2.0.
- Recognition results must be treated as candidates until verified by rules, fixtures, or user correction.
- Fixture-based minimum success criteria must be documented before release.
- Failed or low-confidence recognition must fall back to manual correction and existing match metadata editing.
- Image recognition must not become the primary duel trigger mechanism; template matching and state transitions remain the primary detection path.
- The repository must not distribute game assets, user screenshots, or proprietary training data.

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
- image-recognition-assisted metadata extraction

Image recognition must not be the primary trigger mechanism.

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
