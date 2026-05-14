# User Runtime Data

This directory is reserved for user runtime data.

Runtime data must remain separated from application code and must persist across updates.

## Layout

- `config/`: user configuration and local settings.
- `data/db/`: SQLite databases.
- `data/videos/`: recorded videos.
- `data/screenshots/`: generated screenshots.
- `data/exports/`: generated exports.
- `logs/`: startup and runtime logs.

## Commit Policy

Only safe placeholders and documentation should be committed from this directory.

Do not commit:
- SQLite database files
- OAuth tokens
- secrets
- logs
- videos
- screenshots
- exports
- generated runtime files
- Yu-Gi-Oh! Master Duel assets or extracted template images
