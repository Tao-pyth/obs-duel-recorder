# OBS Plugin

This directory is reserved for OBS Plugin code.

The Plugin is responsible for:
- OBS Frontend API integration
- Dock UI integration
- overlay and Text Source updates
- Worker process launch
- Worker heartbeat monitoring
- OBS lifecycle detection

The Plugin must remain lightweight.

The Plugin must not:
- perform heavy image processing
- perform OCR
- directly manipulate SQLite
- directly upload to YouTube

Implementation code is intentionally not added in v0.1 Repository Foundation.
