# Python Worker

This directory is reserved for Python Worker code.

The Worker is responsible for:
- SQLite management
- queue persistence
- match state management
- template matching
- OCR processing
- YouTube uploads
- export generation
- recovery processing

The Worker must support restart-safe execution.

## v0.2 scaffold

A minimal package scaffold exists under `app/worker/odr_worker/`.

- Print version: `python -m odr_worker --version`
- Placeholder run: `python -m odr_worker`

Implementation work is tracked by issues #11-#16.
