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
- `v1.1.4`, `v1.1.5`, `v1.1.6`, and `v1.1.7` are completed user-ready tags in the active sequence.
- The current active target is `v1.1.8`, tracked by #595 and its linked issues.
- Existing older `v1.x` and `v2.x` tags from before the usability policy are legacy non-product tags. They must not be moved or overwritten.

See [Release and tag policy](docs/release.md).

## Current Development

Latest release status:
- Latest tagged user-ready version: `v1.1.7` - stabilization, selected-target YouTube upload actions, and documentation synchronization.
- Latest feature release: `v1.1.4` - consolidated Dock, YouTube readiness, and guided automatic recording update.
- Safety hotfixes after `v1.1.4`: `v1.1.5` video-preview crash fix, `v1.1.6` installed-binary hash verification, and `v1.1.7` selected upload stabilization.

Current development target:
- `v1.1.8` - real OBS Dock usability correction for cramped buttons, duplicate preview/settings surfaces, frame selectors, and template editing visibility.
- Tracking issue: [#595](https://github.com/Tao-pyth/obs-duel-recorder/issues/595).
- Child issues: [#596](https://github.com/Tao-pyth/obs-duel-recorder/issues/596), [#597](https://github.com/Tao-pyth/obs-duel-recorder/issues/597), [#598](https://github.com/Tao-pyth/obs-duel-recorder/issues/598), [#599](https://github.com/Tao-pyth/obs-duel-recorder/issues/599), [#600](https://github.com/Tao-pyth/obs-duel-recorder/issues/600), and [#601](https://github.com/Tao-pyth/obs-duel-recorder/issues/601).

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
- Compact Record / Upload / Template / Manage Dock direction, localized UI foundations, and Material-inspired state/color documentation.

Current v1.1.8 issue set:
- P1: fix Record tab action button truncation (#596).
- P1: remove the duplicate upload text preview action from the Record tab (#597).
- P1: remove misleading arrow icons from representative-frame selectors (#598).
- P1: fix Upload tab action button truncation (#599).
- P1: move upload text template editing into a dedicated Template tab (#600).
- P1: remove the duplicate settings dialog button from the Manage tab (#601).

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
- `v1.1.7`: [GitHub tag](https://github.com/Tao-pyth/obs-duel-recorder/tree/v1.1.7)
- Release history: [docs/release-history.md](docs/release-history.md)

Active version gates:
- `v1.1.4` - completed consolidated Dock, YouTube readiness, and automatic recording update.
- `v1.1.5` - completed crash hotfix for video preview parsing.
- `v1.1.6` - completed installed OBS binary/package hash verification hotfix.
- `v1.1.7` - completed stabilization and YouTube upload UX completion target.
- `v1.1.8` - active real OBS Dock usability correction target.

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
