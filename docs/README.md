# Documentation

This directory is the documentation entry point for OBS Duel Recorder.

## Canonical Documents

- [Roadmap](roadmap.md)
- [Requirements](requirements/requirements.md)
- [Traceability](traceability.md)
- [Release and tag policy](release.md)
- [Release history](release-history.md)
- [Version tracking issue template](release/version-tracking-issue-template.md)
- [v0.2 release readiness checklist](release/v0.2-release-readiness.md)
- [v0.1 Repository Foundation acceptance checklist](requirements/v0.1-acceptance.md)

## Architecture

Architecture documents describe technical behavior and responsibility boundaries.

- [Plugin and Worker architecture](architecture/plugin-worker.md)
- [Database design](architecture/db.md)
- [Queue design](architecture/queue.md)
- [Upload flow](architecture/upload.md)

## Worker (Developer)

- [v0.2 Worker quickstart (smoke)](worker/v0.2-quickstart.md)

## User Documentation

User-facing setup and usage documents live under `docs/user/`.

- [Installation](user/install.md)
- [First setup](user/setup.md)
- [Usage](user/usage.md)

## Validation

Documentation validation policy lives at:
- [Documentation validation policy](validation.md)

## Documentation Rules

- Keep important design decisions under `docs/`.
- Keep user-facing documentation under `docs/user/`.
- Keep architecture and runtime behavior under `docs/architecture/`.
- Treat English documents as canonical; translations should not contradict the English source.
- Keep completed release point information in `docs/release-history.md` or a version-specific release summary.
- Do not use documentation pages to store runtime data, credentials, screenshots, or game assets.
