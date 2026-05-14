# Plugin Worker Architecture

## Responsibility Separation

| Component | Responsibility |
|---|---|
| OBS Plugin | OBS integration, UI, overlay updates |
| Python Worker | DB, queue, OCR, uploads |

---

## Plugin Responsibilities

- OBS Frontend API
- Dock UI
- Text Source updates
- Worker process management
- Worker heartbeat monitoring

---

## Worker Responsibilities

- SQLite
- Queue processing
- Recovery
- Template matching
- OCR
- YouTube upload
- Export generation

---

## Communication

Plugin and Worker communicate using localhost HTTP APIs.

Example:
- GET /health
- POST /events/recording-started
- POST /events/recording-stopped
