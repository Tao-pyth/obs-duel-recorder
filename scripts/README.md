# Scripts

This directory is reserved for repository-level helper scripts.

Allowed responsibilities:
- setup helpers
- packaging helpers
- maintenance tooling
- local validation helpers
- future update-flow helpers

Current helpers:
- `validate_markdown_links.py`
- `validate_jp_user_docs_coverage.py`
- `build_docs_site.py`
- `build_release_package.ps1`
- `build_worker_exe.ps1`
- `validate_release_package.ps1`
- `verify_obs_install_layout.ps1`

`build_docs_site.py` creates the GitHub Pages artifact under `build/docs-site/` without copying runtime data, secrets, screenshots, videos, databases, local template images, or game assets.

`build_worker_exe.ps1` creates the bundled Windows Worker executable under `build/worker/odr-worker/` using PyInstaller. Install the Worker bundle dependency first:

```powershell
python -m pip install "./app/worker[bundle]"
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build_worker_exe.ps1 -OutputDir build/worker -Clean
```

Use `-PythonExe <path-to-python.exe>` when building from an isolated virtual environment.

`build_release_package.ps1` creates the release ZIP under `build/release/` from an existing `obs-duel-recorder.dll`, the bundled Worker executable, Worker source fallback files, `update.bat`, and the minimum documentation set. It writes `SHA256SUMS.txt` beside the ZIP and calls `validate_release_package.ps1` to reject runtime data, secrets, logs, databases, screenshots, videos, and local game assets.

`verify_obs_install_layout.ps1` validates a packaged install against an OBS root directory. Packaged users normally run `verify-install.bat "<OBS install>"` from the ZIP root; the batch wrapper requires only Windows PowerShell and does not require Python or development tools.

Scripts must not store:
- runtime data
- secrets
- OAuth tokens
- generated videos
- generated screenshots
- generated exports
- SQLite databases
- Yu-Gi-Oh! Master Duel assets or extracted template images

Implementation scripts are intentionally not added in v0.1 Repository Foundation.
