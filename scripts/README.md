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
- `validate_release_package.ps1`

`build_docs_site.py` creates the GitHub Pages artifact under `build/docs-site/` without copying runtime data, secrets, screenshots, videos, databases, local template images, or game assets.

`build_release_package.ps1` creates the release ZIP under `build/release/` from an existing `obs-duel-recorder.dll`, Worker package files, `update.bat`, and the minimum documentation set. It writes `SHA256SUMS.txt` beside the ZIP and calls `validate_release_package.ps1` to reject runtime data, secrets, logs, databases, screenshots, videos, and local game assets.

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
