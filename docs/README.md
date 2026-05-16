# Documentation

This directory is the documentation entry point for OBS Duel Recorder.

## Canonical Documents

- [Roadmap](roadmap.md)
- [Requirements](requirements/requirements.md)
- [Release and tag policy](release.md)
- [v0.1 Repository Foundation acceptance checklist](requirements/v0.1-acceptance.md)

## Architecture

Architecture documents describe technical behavior and responsibility boundaries.

- [Plugin and Worker architecture](architecture/plugin-worker.md)
- [Database design](architecture/db.md)
- [Queue design](architecture/queue.md)
- [Upload flow](architecture/upload.md)

## User Documentation

User-facing setup and usage documents live under `docs/user/`.

- [Installation](user/install.md)
- [First setup](user/setup.md)
- [Usage](user/usage.md)

## Documentation Rules

- Keep important design decisions under `docs/`.
- Keep user-facing documentation under `docs/user/`.
- Keep architecture and runtime behavior under `docs/architecture/`.
- Do not use documentation pages to store runtime data, credentials, screenshots, or game assets.

## Validation

Validate internal Markdown links (files within the repository):

```powershell
python scripts/validate_markdown_links.py
```

Notes:
- Checks `README.md` and `docs/**/*.md`.
- Ignores `http(s)` and `mailto:` targets.
