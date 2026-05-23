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

Important versioning note:
- Project versions are assigned only by user-visible usability.
- The project is still **pre-v1.0** because first public release publication and final tag approval are not complete yet.
- The OBS Plugin DLL build, real OBS load smoke, packaging workflow, checksum gates, Worker EXE bundled distribution, and local download-to-first-run smoke are complete.
- The planned active v1.0 publication tag is `v1.0.1` because legacy `v1.0.0` already exists.
- Existing `v1.x` and `v2.x` tags are legacy non-product tags from before this rule. They are not part of the active version sequence and are not proof of a usable OBS plugin release.

See [Release and tag policy](docs/release.md).

---

## Current Development

Latest release status:
- Released user-ready version: none yet; the project remains pre-v1.0.
- Latest completed version gate: `v0.13` - Practical Distribution Readiness

Current development target:
- `v1.0` - First Usable OBS Plugin Release
- Tracking issue: [#401](https://github.com/Tao-pyth/obs-duel-recorder/issues/401)
- Planned active publication tag: `v1.0.1`

Next roadmap target:
- Public GitHub Release publication and final tag approval

---

## Disclaimer

This project is an unofficial fan-made tool.

This project is not affiliated with KONAMI.

Yu-Gi-Oh! Master Duel assets are not distributed with this software.

---

## Features (Roadmap Overview)

Status note: This list includes current foundations and planned roadmap capabilities. For release availability, use [Current Development](#current-development), [Current Status](#current-status), and the [Roadmap](docs/roadmap.md) as the source of truth.

Completed foundation:
- SQLite runtime storage foundation
- Worker runtime, health, logging, and migration foundations
- OBS Plugin skeleton and Worker lifecycle management
- OBS overlay Text Source integration
- Screenshot capture and linkage
- YouTube upload MVP boundary
- Match metadata, memo, search, and upload metadata generation
- Export archive generation with SQLite, metadata, screenshots, and video linkages
- Setup wizard state and validation for runtime path, OBS integration, OAuth, and templates
- Update entrypoint, DB backup-before-migration, update-state diagnostics, and runtime preservation
- Image-recognition-assisted result, rank, and DP metadata candidates with manual review/correction audit
- Read-only match, deck, opponent, upload, and memo statistics
- GitHub Pages documentation publication with static HTML artifact generation
- Runtime queue/upload optimization, DB indexes, and recovery diagnostics
- OBS Plugin DLL build and real OBS load smoke evidence
- Worker EXE bundled distribution and local download-to-first-run smoke evidence

Planned roadmap capabilities:
- Release publication and final tag naming (`v1.0.1` planned)

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

- Released user-ready version: none yet
- Status: pre-v1.0; OBS Plugin DLL build/load smoke, packaging automation, Worker EXE bundled distribution, and local download-to-first-run smoke are complete
- Latest completed version gate: [#404](https://github.com/Tao-pyth/obs-duel-recorder/issues/404)
- Current version tracking issue: [#401](https://github.com/Tao-pyth/obs-duel-recorder/issues/401)
- Next release publication gate: [#401](https://github.com/Tao-pyth/obs-duel-recorder/issues/401)
- Release record: [docs/release-history.md](docs/release-history.md)

### Completed Legacy Implementation Work

- SQL aggregation for upload status counts without queue item materialization
- Single-row next upload candidate selection with `ORDER BY id LIMIT 1`
- Runtime queue indexes for state-first polling and recovery paths
- SQLite busy timeout for queue connections
- Startup recovery diagnostics with scanned count, recovered count, and duration evidence
- Runtime baseline, regression threshold, and long-session fixture documentation

### Version Gates

- `v0.11` - OBS Plugin Real Load Smoke
- `v0.12` - Release Packaging Automation
- `v0.13` - Practical Distribution Readiness
- `v1.0` - First Usable OBS Plugin Release (current)

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

GitHub Pages publication is documented in [docs/pages.md](docs/pages.md).

---

## Setup Policy

This repository does not distribute Yu-Gi-Oh! Master Duel assets or template images.

Detection templates are intended to be generated locally by users.

---

## License

MIT License
