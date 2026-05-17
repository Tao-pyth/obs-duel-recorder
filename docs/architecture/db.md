# Database Design

## Database Engine

SQLite

---

## Location (runtime contract)

The SQLite DB file is runtime data and must live under `user_data/`.

Default location (v0.3):
- `user_data/data/db/odr.sqlite3`

---

## Migration

Database schema migration is required.

- Schema version information is stored in the DB (`odr_meta.schema_version`).
- Applied migrations are tracked in `schema_migrations`.
- Migrations live under `app/worker/odr_worker/migrations/*.sql` and are applied in deterministic order.

---

## Main Tables

v0.3 creates a minimal schema so future versions can evolve it via migrations.

### matches

Initial (v0.3):
- `id`
- `created_at`

Planned (future; evolves via migrations):
- deck_name
- deck_seq
- opponent_deck
- result
- memo
- started_at
- ended_at
- video_path
- screenshot_path
- upload_status
- youtube_video_id
- youtube_url

### upload_queue

Initial (v0.3):
- `id`
- `match_id`
- `state`
- `created_at`
