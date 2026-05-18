# OBS Duel Recorder

OBS Duel Recorder is an OBS Studio based duel recording assistant for Yu-Gi-Oh! Master Duel.

This project automatically records duel segments, manages match history, and uploads archived videos to YouTube.

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
- `v0.3.0` - SQLite Foundation

Current development target:
- `v0.4` - OBS Plugin Skeleton
- Tracking issue: [#110](https://github.com/Tao-pyth/obs-duel-recorder/issues/110)

Next roadmap target:
- `v0.5` - Overlay Integration

---

## Disclaimer

This project is an unofficial fan-made tool.

This project is not affiliated with KONAMI.

Yu-Gi-Oh! Master Duel assets are not distributed with this software.

---

## Features

- Automatic duel recording
- Match queue management
- YouTube archive upload
- OBS overlay integration
- Match memo support
- SQLite based history management
- Upload retry / recovery support
- GitHub Actions based packaging

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

- Version: `v0.3.0`
- Scope: SQLite Foundation
- Status: released
- Tag target: `5a31a55b76f5cd2d3e2a64cbf1d4cfed04642dd0`
- Tracking issue: [#88](https://github.com/Tao-pyth/obs-duel-recorder/issues/88)
- Release record: [docs/release-history.md](docs/release-history.md)

### Completed in v0.3

- SQLite initialization under runtime data (`user_data/`)
- Schema version management
- Minimal migration framework
- Initial tables (`matches`, `upload_queue`)
- Restart-safe, idempotent startup behavior
- Worker runtime and package metadata version aligned to `0.3.0`

### Planned After v0.3

- `v0.4` - OBS Plugin skeleton and Worker lifecycle management
- Later roadmap items include overlay integration, queue recovery, template matching, upload automation, packaging, and GitHub Pages documentation.

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
