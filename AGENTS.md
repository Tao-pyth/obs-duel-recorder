# AGENTS.md

# OBS Duel Recorder - Agent Instructions

This repository is developed with AI coding agents in mind.

Agents must prioritize:
- long-term maintainability
- responsibility separation
- recovery safety
- reproducible runtime behavior
- documentation-first development

---

# Project Overview

OBS Duel Recorder is an OBS Studio based duel recording assistant for Yu-Gi-Oh! Master Duel.

The system automatically:
- detects duel segments
- records duel videos
- manages upload queues
- uploads videos to YouTube
- stores match history in SQLite
- manages overlays and metadata

This repository follows an OBS Plugin + Python Worker architecture.

---

# Core Architecture Rules

## Responsibility Separation

The project MUST maintain strict separation between:

| Component | Responsibility |
|---|---|
| OBS Plugin | OBS integration, Dock UI, overlay control, Worker lifecycle |
| Python Worker | Queue processing, SQLite, image analysis, OCR, upload processing |

Heavy processing MUST NOT be implemented inside the OBS Plugin unless explicitly required.

---

# Plugin Rules

The OBS Plugin is responsible for:

- OBS Frontend API integration
- Dock UI integration
- Overlay / Text Source updates
- Worker process launch
- Worker heartbeat monitoring
- OBS lifecycle detection

The Plugin SHOULD remain lightweight.

The Plugin MUST NOT:
- perform heavy image processing
- perform OCR
- directly manipulate SQLite
- directly upload to YouTube

---

# Worker Rules

The Python Worker is responsible for:

- SQLite management
- Queue persistence
- Match state management
- Template matching
- OCR processing
- YouTube uploads
- Export generation
- Recovery processing

The Worker MUST support restart-safe execution.

---

# Runtime Directory Rules

The repository separates runtime data from application binaries.

Application files:
```text
app/
```

User runtime data:
```text
user_data/
```

Agents MUST preserve:
- user_data/config
- user_data/data
- user_data/logs

during updates or packaging operations.

---

# Database Rules

SQLite is the primary storage engine.

The project MUST:
- support schema migration
- maintain schema version information
- support recovery-safe queue persistence

The database file MUST NOT be committed to git.

---

# Queue Rules

Queue state persistence is required.

Expected queue states include:
- ready_upload
- uploading
- uploaded
- upload_failed
- quota_waiting
- need_manual_review

Interrupted recording sessions SHOULD be discarded during recovery.

Upload retry behavior:
- network failures -> retry
- quota exceeded -> wait until quota reset
- missing file -> discard queue item

---

# Upload Rules

YouTube uploads are performed using YouTube Data API.

Upload success MUST be determined using the response from:
```text
videos.insert
```

The following fields MUST be stored:
- youtube_video_id
- youtube_url

Additional verification API calls SHOULD be avoided to reduce quota usage.

---

# Overlay Rules

The project uses fixed OBS Text Sources.

The overlay system SHOULD support:
- deck name
- deck sequence number
- result
- opponent deck

Overlay templates MUST be configurable from settings.

---

# Detection Rules

The project uses:
- state transitions
- template matching

as the primary detection method.

OCR is supplemental only.

OCR MUST NOT be the primary recording trigger mechanism.

---

# Asset Distribution Rules

This repository MUST NOT distribute:
- Yu-Gi-Oh! Master Duel assets
- screenshots from the game
- template images extracted from the game

Template generation is intended to be performed locally by the user.

---

# Logging Rules

Logs MUST be:
- generated per startup date
- stored under user_data/logs
- preserved unless manually removed

---

# Update Rules

Updates MUST preserve:
- SQLite databases
- OAuth tokens
- queue state
- settings

The update flow is expected to:
1. backup database
2. replace application files
3. run migrations
4. resume runtime

---

# OAuth Rules

OAuth tokens are stored locally.

OAuth credentials MUST NOT be committed to git.

The following paths MUST remain ignored:
```text
config/secrets/
user_data/config/secrets/
```

---

# Documentation Rules

This repository follows documentation-first development.

Important design decisions MUST be documented under:
```text
docs/
```

README.md:
- high level overview
- setup policy
- project summary

docs/architecture:
- technical design
- queue behavior
- DB structure
- upload flow

docs/user:
- user-facing setup and usage guides

---

# GitHub Pages Rules

The repository is expected to support GitHub Pages documentation in the future.

User-facing documentation SHOULD remain under:
```text
docs/user/
```

---

# Code Style Rules

Python code SHOULD:
- prioritize readability
- contain beginner-friendly comments
- avoid unnecessary abstraction
- separate DB logic from API logic

Avoid overengineering.

---

# Recovery Rules

The system MUST prioritize recovery safety over automation aggressiveness.

If runtime state becomes inconsistent:
- preserve data
- stop unsafe operations
- move entries to recovery/manual review states

---

# Version Rules

The project follows:
```text
Major.Minor.Patch
```

versioning.

Plugin and Worker versions MUST be compatible.

Version mismatch SHOULD trigger update handling.

---

# License

This repository uses the MIT License.

---

# Disclaimer

This project is an unofficial fan-made tool.

This project is not affiliated with KONAMI.

Yu-Gi-Oh! Master Duel assets are not distributed with this software.

# Versioning Rules

Minor versions are roadmap-driven planned features.

Patch versions are intended for:
- bug fixes
- migration fixes
- runtime fixes
- non-roadmap adjustments

---

# Runtime Rules

User runtime data must persist across updates.

Application binaries and runtime data must remain separated.
