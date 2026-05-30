# OBS Duel Recorder

OBS Duel Recorder is an OBS Studio based duel recording assistant for Yu-Gi-Oh! Master Duel.

The project records duel segments, manages local match history, prepares upload queues, and supports YouTube upload through an OBS Plugin + Python Worker architecture.

## Versioning Policy

| Type | Meaning |
|---|---|
| Major | architecture/runtime breaking changes |
| Minor | planned roadmap features |
| Patch | bug fixes, runtime fixes, migration fixes, and non-roadmap usability corrections |

Important versioning notes:
- Project versions are assigned by user-visible usability, not by the old prototype tag sequence.
- The first user-ready OBS Plugin release is `v1.0.1`.
- `v1.1.4`, `v1.1.5`, and `v1.1.6` are completed user-ready tags in the active sequence.
- The current active target is `v1.1.7`, tracked by #584 and its linked issues.
- Existing older `v1.x` and `v2.x` tags from before the usability policy are legacy non-product tags. They must not be moved or overwritten.

See [Release and tag policy](docs/release.md).

## Current Development

Latest release status:
- Latest tagged user-ready version: `v1.1.6` - install layout verification hotfix.
- Latest feature release: `v1.1.4` - consolidated Dock, YouTube readiness, and guided automatic recording update.
- Safety hotfixes after `v1.1.4`: `v1.1.5` video-preview crash fix and `v1.1.6` installed-binary hash verification.

Current development target:
- `v1.1.7` - current stabilization, documentation synchronization, install safety, automatic reporting boundary, and post-recording YouTube upload UX completion.
- Tracking issue: [#584](https://github.com/Tao-pyth/obs-duel-recorder/issues/584).
- Detailed YouTube upload UX parent: [#508](https://github.com/Tao-pyth/obs-duel-recorder/issues/508).

Automatic GitHub Issue error reporting status:
- The repository has an architecture contract for relay-owned error reporting.
- The distributed Plugin and Worker do not currently create GitHub Issues directly.
- Shipping automatic Issue creation requires a relay or controlled GitHub Actions path, explicit opt-in, and redaction checks. This is tracked by [#581](https://github.com/Tao-pyth/obs-duel-recorder/issues/581).

## Disclaimer

This project is an unofficial fan-made tool.

This project is not affiliated with KONAMI.

Yu-Gi-Oh! Master Duel assets are not distributed with this software.

## Features

Completed foundations:
- SQLite runtime storage, migrations, and recovery-safe queue persistence.
- Worker runtime, health, logging, and bundled Windows executable distribution.
- OBS Plugin skeleton, Dock integration, Worker lifecycle management, and OBS overlay Text Source integration.
- Screenshot capture, local metadata editing, upload metadata generation, and export archive generation.
- Setup wizard state and validation for runtime path, OBS integration, OAuth, and templates.
- ZIP release package generation, install/update assistant, package validation, and install layout verification.
- YouTube upload provider boundary with deterministic mock behavior and optional Google provider support.
- Compact Record / Upload / Manage Dock direction, localized UI foundations, and Material-inspired state/color documentation.

Current v1.1.7 issue set:
- P0: synchronize README, traceability, release records, and user docs with the actual current state (#579).
- P0: make OBS install/update verification recovery-safe, including package-vs-installed hash checks (#580).
- P1: complete the recording -> metadata confirmation -> upload text preview -> selected YouTube upload -> recovery experience (#508, #509-#578).
- P1: define whether automatic GitHub Issue error reporting is shipped or remains a documented relay contract (#581).
- P1: prioritize and order the current open issue set for multi-agent implementation (#582).
- P2: reconcile stale tracking issues and duplicate release-state records (#583).

## Architecture

This project separates responsibilities into two components:

| Component | Responsibility |
|---|---|
| OBS Plugin | OBS integration, Dock UI, overlay control, Worker lifecycle |
| Python Worker | Queue processing, SQLite, image analysis, OCR support, YouTube upload |

Heavy processing belongs in the Worker. The OBS Plugin should remain lightweight and focused on OBS integration and UI.

## Project Structure

```text
obs-duel-recorder/
|-- app/
|   |-- plugin/
|   |-- worker/
|   `-- ui/
|-- docs/
|   |-- requirements/
|   |-- architecture/
|   `-- user/
|-- scripts/
|-- user_data/
|   |-- config/
|   |-- data/
|   `-- logs/
`-- .github/
```

Runtime data under `user_data/` must be preserved across updates.

## Current Status

Latest release:
- `v1.1.6`: [GitHub tag](https://github.com/Tao-pyth/obs-duel-recorder/tree/v1.1.6)
- Release history: [docs/release-history.md](docs/release-history.md)

Active version gates:
- `v1.1.4` - completed consolidated Dock, YouTube readiness, and automatic recording update.
- `v1.1.5` - completed crash hotfix for video preview parsing.
- `v1.1.6` - completed installed OBS binary/package hash verification hotfix.
- `v1.1.7` - active stabilization and YouTube upload UX completion target.

## Documentation

Documentation starts at [docs/README.md](docs/README.md).

Key documents:
- [Roadmap](docs/roadmap.md)
- [Traceability](docs/traceability.md)
- [Requirements](docs/requirements/requirements.md)
- [Release and tag policy](docs/release.md)
- [Release history](docs/release-history.md)
- [Packaging](docs/architecture/packaging.md)
- [Automatic error reporting contract](docs/architecture/automatic-error-reporting.md)
- [Dock workflow and UI state architecture](docs/architecture/dock-workflow-ui-state.md)
- [Installation](docs/user/install.md)
- [User docs](docs/user/index.md)
- [Japanese user docs](docs/user/ja/index.md)

GitHub Pages publication is documented in [docs/pages.md](docs/pages.md).

## Setup Policy

This repository does not distribute Yu-Gi-Oh! Master Duel assets or template images.

Detection templates are intended to be generated locally by users.

## License

MIT License
