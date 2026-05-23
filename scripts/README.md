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

`build_docs_site.py` creates the GitHub Pages artifact under `build/docs-site/` without copying runtime data, secrets, screenshots, videos, databases, local template images, or game assets.

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
