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
- [v0.3 release readiness checklist](release/v0.3-release-readiness.md)
- [v0.4 release readiness checklist](release/v0.4-release-readiness.md)
- [v0.5 release readiness checklist](release/v0.5-release-readiness.md)
- [v0.6 release readiness checklist](release/v0.6-release-readiness.md)
- [v0.4 release summary](release/v0.4-release-summary.md)
- [v0.1 Repository Foundation acceptance checklist](requirements/v0.1-acceptance.md)
- [v0.3 SQLite Foundation acceptance checklist](requirements/v0.3-sqlite-foundation-acceptance.md)
- [v0.4 OBS Plugin Skeleton acceptance checklist](requirements/v0.4-obs-plugin-skeleton-acceptance.md)
- [v0.5 Overlay Integration acceptance checklist](requirements/v0.5-overlay-integration-acceptance.md)
- [v0.6 Recording State Management acceptance checklist](requirements/v0.6-recording-state-management-acceptance.md)

## Architecture

Architecture documents describe technical behavior and responsibility boundaries.

- [Plugin and Worker architecture](architecture/plugin-worker.md)
- [Overlay architecture](architecture/overlay.md)
- [Database design](architecture/db.md)
- [Queue design](architecture/queue.md)
- [Recording state machine](architecture/recording.md)
- [Upload flow](architecture/upload.md)
- [v0.5 OBS overlay smoke procedure](architecture/v0.5-overlay-smoke.md)
- [v0.6 recording-state smoke procedure](architecture/v0.6-recording-state-smoke.md)

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

Quick validation commands (run from the repository root):

```powershell
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File docs/tools/validate_markdown_links.ps1
python scripts/validate_jp_user_docs_coverage.py --root .
```

Notes:
- Markdown links: checks `README*.md` and `docs/**/*.md`.
- JP user docs coverage: checks English ↔ Japanese topic coverage.

## Documentation Rules

- Keep important design decisions under `docs/`.
- Keep user-facing documentation under `docs/user/`.
- Keep architecture and runtime behavior under `docs/architecture/`.
- Treat English documents as canonical; translations should not contradict the English source.
- Keep completed release point information in `docs/release-history.md` or a version-specific release summary.
- Do not use documentation pages to store runtime data, credentials, screenshots, or game assets.
