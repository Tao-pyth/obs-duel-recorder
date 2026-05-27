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
- The first user-ready OBS Plugin release is `v1.0.1`.
- The `v1.1.3` target is Dock Workflow, Metadata, Upload, and UI State Update, coordinated through #470 after `v1.1.2`.
- Existing `v1.x` and `v2.x` tags are legacy non-product tags from before this rule. They are not part of the active version sequence and are not proof of a usable OBS plugin release.
- Existing legacy tags must not be moved or overwritten. Because a legacy `v1.1.0` tag exists, v1.1.x publication decisions must be recorded before release.

See [Release and tag policy](docs/release.md).

---

## Current Development

Latest release status:
- Released user-ready version: `v1.1.3` - Dock Workflow, Metadata, Upload, and UI State Update
- Latest completed version gate: `v1.1.3` - Dock Workflow, Metadata, Upload, and UI State Update

Current development target:
- pending planning after `v1.1.3`

Next roadmap target:
- pending planning after `v1.1.3`

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
- First user-ready OBS Plugin release package (`v1.0.1`)

Completed v1.1.0 issue set:
- clearer install guidance and install layout verification
- actionable Worker launch diagnostics in the Dock and OBS logs
- recording completion and output evidence in the Dock
- first-run setup validation UI
- recording-to-match/queue handoff
- practical automatic recording setup and template testing
- production YouTube upload provider, OAuth setup, and manual-review controls
- match metadata editing and upload metadata preview UI
- ZIP install/update assistant and installer/MSI deferral decision

Completed v1.1.1 issue set:
- full release verification matrix and final validation
- user operation flow and system overview documentation with diagrams
- UI images in user documentation
- colorful user-focused OBS Dock redesign
- selectable Dock color themes from Settings
- detailed OBS source documentation
- simple Help message from the Dock
- smoother automatic recording setup information registration
- v1.1.1 release records and publication handoff

Active v1.1.2 issue set:
- automatic error report GitHub Issue operation documentation
- release package validation that rejects CI fixture or non-PE Worker executables
- compact OBS Dock tabs for recording, setup, settings, automatic setup, help, and diagnostics
- Japanese/English language selection from Settings
- Help recovery note that explains language changes in the opposite language

Completed v1.1.3 issue set:
- Record -> Metadata -> Upload -> Manage Dock workflow
- direct Dock metadata editing with deck/opponent dropdown candidates
- carry-over for deck, opponent deck, rank, and DP
- editable upload title, description, and tag templates with preview
- Worker-rendered upload metadata shared by preview and upload processing
- localized metadata and automatic setup fallback dialogs
- short UI state labels with raw diagnostics hidden behind detail controls
- Material-inspired UI state and color role documentation

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

- Released user-ready version: [`v1.0.1`](https://github.com/Tao-pyth/obs-duel-recorder/releases/tag/v1.0.1)
- Status: first usable OBS Plugin release published with ZIP and SHA256 assets
- Latest completed version gate: [#401](https://github.com/Tao-pyth/obs-duel-recorder/issues/401)
- Latest completed hardening issue set: [#440](https://github.com/Tao-pyth/obs-duel-recorder/issues/440)
- Latest completed patch target: [#470](https://github.com/Tao-pyth/obs-duel-recorder/issues/470) / `v1.1.3`
- Release record: [docs/release-history.md](docs/release-history.md)

### Completed Legacy Implementation Work

- SQL aggregation for upload status counts without queue item materialization
- Single-row next upload candidate selection with `ORDER BY id LIMIT 1`
- Runtime queue indexes for state-first polling and recovery paths
- SQLite busy timeout for queue connections
- Startup recovery diagnostics with scanned count, recovered count, and duration evidence
- Runtime baseline, regression threshold, and long-session fixture documentation

### Active Version Gates

- `v0.11` - OBS Plugin Real Load Smoke
- `v0.12` - Release Packaging Automation
- `v0.13` - Practical Distribution Readiness
- `v1.0` - First Usable OBS Plugin Release
- `v1.1` - First Trial Usability Hardening
- `v1.1.1` - Usability UI and Documentation Hardening
- `v1.1.2` - Recovery Reporting, Distribution, UI, and i18n Update
- `v1.1.3` - Dock Workflow, Metadata, Upload, and UI State Update

---

## Documentation

Documentation starts at [docs/README.md](docs/README.md).

Key documents:
- [Roadmap](docs/roadmap.md)
- [Requirements](docs/requirements/requirements.md)
- [Release and tag policy](docs/release.md)
- [v1.1.0 acceptance checklist](docs/requirements/v1.1-first-trial-usability-hardening-acceptance.md)
- [v1.1.0 release readiness checklist](docs/release/v1.1-first-trial-usability-hardening-readiness.md)
- [v1.1.1 acceptance checklist](docs/requirements/v1.1.1-usability-ui-documentation-hardening-acceptance.md)
- [v1.1.1 release readiness checklist](docs/release/v1.1.1-usability-ui-documentation-hardening-readiness.md)
- [v1.1.2 acceptance checklist](docs/requirements/v1.1.2-recovery-reporting-distribution-ui-i18n-acceptance.md)
- [v1.1.2 release readiness checklist](docs/release/v1.1.2-recovery-reporting-distribution-ui-i18n-readiness.md)
- [v1.1.3 acceptance checklist](docs/requirements/v1.1.3-dock-workflow-metadata-upload-ui-acceptance.md)
- [v1.1.3 release readiness checklist](docs/release/v1.1.3-dock-workflow-metadata-upload-ui-readiness.md)
- [Dock workflow and UI state architecture](docs/architecture/dock-workflow-ui-state.md)
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
