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
|       |-- odr-worker/
|       |   `-- odr-worker.exe
|       `-- <worker package files>
|-- docs/
|   `-- <minimal install/update/user docs>
|-- scripts/
|   |-- install_release_package.ps1
|   `-- verify_obs_install_layout.ps1
|-- install.bat
|-- update.bat
|-- verify-install.bat
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
- `scripts/install_release_package.ps1`
- `scripts/validate_release_package.ps1`
- `scripts/verify_obs_install_layout.ps1`
- `.github/workflows/release-package.yml`

The packaging script expects an existing `obs-duel-recorder.dll` from the v0.11
OBS Plugin build/smoke boundary. It does not install OBS, Qt, or rebuild the
plugin by itself.

The packaging script also expects a bundled Worker executable directory,
normally created by `scripts/build_worker_exe.ps1` under
`build/worker/odr-worker/`. The bundled executable is the normal user path for
packaged releases; source-based Worker execution is kept as a developer
fallback.

The release ZIP includes `install.bat`,
`scripts/install_release_package.ps1`, `verify-install.bat`, and
`scripts/verify_obs_install_layout.ps1` so packaged users can install/update
and validate the OBS layout without Python or development tools:

```powershell
.\install.bat "C:\Program Files\obs-studio"
```

The install assistant validates the OBS root, copies
`obs-duel-recorder.dll` to `obs-plugins\64bit\`, copies the full Worker bundle
to `obs-plugins\worker\odr-worker\`, preserves runtime `user_data`, and then
runs the verifier.

```powershell
.\verify-install.bat "C:\Program Files\obs-studio"
```

The verifier checks that `obs-duel-recorder.dll` is under
`obs-plugins\64bit\`, that the full Worker bundle is under
`obs-plugins\worker\odr-worker\`, and that the known wrong
`obs-plugins\64bit\worker\` placement is absent.

## Worker Executable Bundle Decision

The v0.13 distribution gate uses PyInstaller to create the Windows Worker
executable bundle.

Decision reasons:
- PyInstaller has a stable Windows onedir output model that fits the current ZIP
  layout without requiring an installer.
- The Worker already exposes a small CLI entrypoint, so PyInstaller can bundle
  the FastAPI/Uvicorn runtime with limited packaging glue.
- The generated directory can be validated by the existing PowerShell package
  builder and checksum flow.
- PyInstaller can be kept as an optional `bundle` dependency, so normal Worker
  development and tests do not need the bundling toolchain.

Constraints:
- The executable bundle is Windows-only for the active release target.
- The release workflow must build the Worker executable before packaging.
- Runtime data, logs, DBs, screenshots, videos, OAuth files, secrets, and game
  assets must remain outside the executable bundle and release ZIP.
- Source-based Worker execution remains a developer fallback, not the normal
  user install path.

Rejected alternatives for v0.13:
- Nuitka: a stronger optimization/compiler path, but it adds more build
  complexity than this release gate needs.
- cx_Freeze: workable for Python app freezing, but PyInstaller has simpler
  workflow integration for the current PowerShell ZIP builder.
- Requiring user-installed Python: rejected for v1.0 practical distribution
  because normal users should not need to install Python, pip, or a virtual
  environment manually.

The release workflow can either read `build/plugin/Release/obs-duel-recorder.dll`
from the workspace or download an artifact named by `plugin_dll_artifact_name`.
It builds the bundled Worker executable before creating the release package and
always generates:

- `obs-duel-recorder-vX.Y.Z.zip`
- `SHA256SUMS.txt`

The workflow uploads those files as a run artifact. When `publish_release` is
enabled, a separate `release` environment job attaches them to an existing
GitHub Release after maintainer-controlled approval.

## Relationship To GitHub Pages

GitHub Pages is documentation publication only and is defined by `docs/pages.md`.

The Pages artifact must not include plugin DLLs, Worker packages, release ZIP files, checksums, runtime data, credentials, screenshots, videos, logs, DBs, local template images, or game assets.
