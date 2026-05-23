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
- `v1.3.0` - Setup Wizard

Current development target:
- `v1.4` - Update System
- Tracking issue: [#323](https://github.com/Tao-pyth/obs-duel-recorder/issues/323)

Next roadmap target:
- `v2.0` - Advanced Runtime Phase

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
- Screenshot capture and linkage
- YouTube upload MVP boundary
- Match metadata, memo, search, and upload metadata generation
- Export archive generation with SQLite, metadata, screenshots, and video linkages
- Setup wizard state and validation for runtime path, OBS integration, OAuth, and templates

Planned roadmap capabilities:
- Update system (`v1.4`)
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

- Version: `v1.3.0`
- Scope: Setup Wizard
- Status: released
- Tag target: `c0579c1f9f3c892059d4bf27dc43e90754453730`
- Tracking issue: [#322](https://github.com/Tao-pyth/obs-duel-recorder/issues/322)
- Release record: [docs/release-history.md](docs/release-history.md)

### Completed in v1.3

- Worker-owned setup wizard API
- First-run, partial, complete, cancel, and reset state handling
- Runtime path, OBS integration, OAuth, and template validation
- Secret-safe setup diagnostics
- Runtime-data-preserving reset/rerun behavior
- v1.3 acceptance and release readiness documentation

### Planned After v1.3

- `v1.4` - Update System
- Later roadmap items include packaging and GitHub Pages documentation.

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
