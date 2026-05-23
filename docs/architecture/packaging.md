# Packaging

This document defines the release packaging direction for `v0.12 - Release Packaging Automation` on the active usability-based version sequence.

## Release ZIP

The preferred distribution format is a GitHub Actions generated ZIP.

Expected ZIP layout:

```text
obs-duel-recorder-vX.Y.Z/
|-- app/
|   |-- plugin/
|   |   `-- obs-duel-recorder.dll
|   `-- worker/
|       `-- <worker package files>
|-- docs/
|   `-- <minimal install/update/user docs>
|-- update.bat
|-- README.md
|-- LICENSE
`-- RELEASE-MANIFEST.json
```

The ZIP checksum is published beside the ZIP:

```text
obs-duel-recorder-vX.Y.Z.zip
SHA256SUMS.txt
```

`SHA256SUMS.txt` contains the SHA256 hash for the ZIP file.

## Exclusions

Release ZIPs must not include:
- `user_data/`
- logs
- SQLite databases
- screenshots
- videos
- OAuth tokens or client secrets
- local template images or game assets
- generated runtime exports

## Release Asset Policy

- GitHub Actions may build and upload draft artifacts automatically.
- Release publication or attaching final assets should require maintainer approval unless a later policy explicitly enables full automation.
- A SHA256 checksum must be generated for each release ZIP.

## Automation

Packaging is implemented by:

- `scripts/build_release_package.ps1`
- `scripts/validate_release_package.ps1`
- `.github/workflows/release-package.yml`

The packaging script expects an existing `obs-duel-recorder.dll` from the v0.11
OBS Plugin build/smoke boundary. It does not install OBS, Qt, or rebuild the
plugin by itself.

The release workflow can either read `build/plugin/Release/obs-duel-recorder.dll`
from the workspace or download an artifact named by `plugin_dll_artifact_name`.
It always generates:

- `obs-duel-recorder-vX.Y.Z.zip`
- `SHA256SUMS.txt`

The workflow uploads those files as a run artifact. When `publish_release` is
enabled, a separate `release` environment job attaches them to an existing
GitHub Release after maintainer-controlled approval.

## Relationship To GitHub Pages

GitHub Pages is documentation publication only and is defined by `docs/pages.md`.

The Pages artifact must not include plugin DLLs, Worker packages, release ZIP files, checksums, runtime data, credentials, screenshots, videos, logs, DBs, local template images, or game assets.
