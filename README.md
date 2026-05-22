# OBS Duel Recorder

OBS Duel Recorder is an OBS Studio based duel recording assistant for Yu-Gi-Oh! Master Duel.

This project is being built to record duel segments, manage match history, and upload archived videos to YouTube.

The project is designed around an OBS Plugin + Python Worker architecture.

---

## Versioning Policy

| Type | Meaning |
|---|---|
| Major | architecture/runtime breaking changes |
| Minor | planned roadmap features |
| Patch | bug fixes and non-roadmap changes |

Major and minor release completions are tagged as `vMajor.Minor.Patch`. See [Release and tag policy](docs/release.md).

---

## Current Development

Latest released version:
- `v0.7.0` - Queue Recovery System

Current development target:
- `v0.8` - Template Matching MVP
- Tracking issue: [#249](https://github.com/Tao-pyth/obs-duel-recorder/issues/249)

Next roadmap target:
- `v0.9` - Screenshot System

---

## Disclaimer

This project is an unofficial fan-made tool.

This project is not affiliated with KONAMI.

Yu-Gi-Oh! Master Duel assets are not distributed with this software.

---

## Features (Roadmap Overview)

Status note: This list includes current foundations and planned roadmap capabilities. For release availability, use [Current Development](#current-development), [Current Status](#current-status), and the [Roadmap](docs/roadmap.md) as the source of truth.

Current released foundation:
- SQLite runtime storage foundation
- Worker runtime, health, logging, and migration foundations
- OBS Plugin skeleton and Worker lifecycle management
- OBS overlay Text Source integration

Planned roadmap capabilities:
- Automatic duel recording (`v0.8`)
- Screenshot capture and linkage (`v0.9`)
- YouTube archive upload (`v1.0`)
- Match memo support (`v1.1`)
- GitHub Actions based packaging (`v1.4+`)

---

## Architecture

This project separates responsibilities into two components:

| Component | Responsibility |
|---|---|
| OBS Plugin | OBS integration, Dock UI, overlay control, Worker management |
| Python Worker | Queue processing, SQLite, image analysis, YouTube upload |

---

## Project Structure

```text
obs-duel-recorder/
|-- app/
|   |-- plugin/
|   |-- worker/
|   `-- ui/
|
|-- docs/
|   |-- requirements/
|   |-- architecture/
|   `-- user/
|
|-- scripts/
|
|-- user_data/
|   |-- config/
|   |-- data/
|   |   |-- db/
|   |   |-- videos/
|   |   |-- screenshots/
|   |   `-- exports/
|   `-- logs/
|
`-- .github/
```

---

## Current Status

### Latest Release

- Version: `v0.7.0`
- Scope: Queue Recovery System
- Status: released
- Tag target: `b0a262ac9500a6ec0ee43f9091ad0fee0e79f0dd`
- Tracking issue: [#248](https://github.com/Tao-pyth/obs-duel-recorder/issues/248)
- Release record: [docs/release-history.md](docs/release-history.md)

### Completed in v0.7

- Recovery-safe upload queue schema migration
- Worker queue API at `/queue/items` and `/queue/items/{id}/command`
- Startup reconciliation for interrupted `uploading` items
- Retry, retry-limit, and `quota_waiting` state handling
- `need_manual_review` runtime state and manual adjudication evidence
- Queue recovery acceptance and release readiness documentation

### Planned After v0.7

- `v0.8` - Template Matching MVP
- Later roadmap items include template matching, screenshot capture, upload automation, packaging, and GitHub Pages documentation.

---

## Documentation

Documentation starts at [docs/README.md](docs/README.md).

Key documents:
- [Roadmap](docs/roadmap.md)
- [Requirements](docs/requirements/requirements.md)
- [Release and tag policy](docs/release.md)
- [v0.1 acceptance checklist](docs/requirements/v0.1-acceptance.md)
- [Architecture docs](docs/architecture/)
- [User docs (language selector / 日本語)](docs/user/index.md)
- [Japanese user docs](docs/user/ja/index.md)

Planned GitHub Pages support is also included in the roadmap.

---

## Setup Policy

This repository does not distribute Yu-Gi-Oh! Master Duel assets or template images.

Detection templates are intended to be generated locally by users.

---

## License

MIT License
