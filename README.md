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
- `v0.4.0` - OBS Plugin Skeleton

Current development target:
- `v0.5` - Overlay Integration
- Tracking issue: [#288](https://github.com/Tao-pyth/obs-duel-recorder/issues/288)

Next roadmap target:
- `v0.6` - Recording State Management

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

Planned roadmap capabilities:
- OBS overlay integration (`v0.5`)
- Recording state management (`v0.6`)
- Match queue management (`v0.7`)
- Automatic duel recording (`v0.8`)
- YouTube archive upload (`v1.0`)
- Match memo support (`v1.1`)
- Upload retry / recovery support (`v0.7`)
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

- Version: `v0.4.0`
- Scope: OBS Plugin Skeleton
- Status: released
- Intended tag target: `629f4f6d8d28d0d9dcca96381340a04077d2bb62`
- Tracking issue: [#110](https://github.com/Tao-pyth/obs-duel-recorder/issues/110)
- Release record: [docs/release-history.md](docs/release-history.md)

### Completed in v0.4

- Loadable OBS Plugin scaffold
- OBS Frontend API startup/shutdown integration
- Plugin-managed Worker launch and ownership tracking
- Worker heartbeat monitoring via localhost `/health`
- Status-first OBS Dock diagnostics
- Persisted Plugin settings at `%APPDATA%\obs-duel-recorder\plugin-settings.json`
- Localhost API wrapper diagnostics for runtime-root continuity and Worker identity
- Windows OBS smoke evidence recorded in #285

### Planned After v0.4

- `v0.5` - Overlay Integration
- Later roadmap items include recording state management, queue recovery, template matching, upload automation, packaging, and GitHub Pages documentation.

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
