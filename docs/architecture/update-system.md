# Update System

v1.4 introduces a conservative update boundary for preserving runtime data while application files change.

## Ownership

- `update.bat` is the canonical Windows update entrypoint.
- The Worker owns update preflight, DB backup, migration execution, and update-state diagnostics.
- The OBS Plugin does not manipulate SQLite or `user_data` during update.

## Runtime Files

Update state is runtime data:

```text
user_data/data/update-state.json
user_data/data/installed-version.json
user_data/data/db/backups/
```

Normal update flow must preserve:
- `user_data/config/`
- `user_data/data/db/`
- `user_data/data/videos/`
- `user_data/data/screenshots/`
- `user_data/data/exports/`
- `user_data/logs/`

## Flow

1. Stop OBS before updating application files.
2. Run `update.bat validate` to check version/API compatibility.
3. Run `update.bat apply`.
4. The updater writes `update-state.json` with `in_progress`.
5. If `user_data/data/db/odr.sqlite3` exists, the updater writes a SQLite backup under `user_data/data/db/backups/`.
6. The updater runs deterministic Worker migrations through the existing SQLite migration boundary.
7. On success, the updater writes `completed` state and updates `installed-version.json`.
8. On failure, the updater writes `failed` state with backup path and recovery guidance.

## Packaged Worker Entrypoint

Packaged releases must prefer the bundled Worker executable:

```text
app/worker/odr-worker/odr-worker.exe
```

`update.bat` runs the bundled executable with the `update` subcommand when it is
present:

```powershell
app\worker\odr-worker\odr-worker.exe update validate
app\worker\odr-worker\odr-worker.exe update apply
```

The source-based `python -m odr_worker.update_system` path is a developer
fallback for repository checkouts. If neither the bundled executable nor
`python` is available, `update.bat` must fail with an actionable diagnostic that
identifies the expected bundled Worker path.

## Version Compatibility

- Downgrades are rejected when both current and target versions are SemVer values.
- API mismatches are rejected before mutations.
- Unknown current version is allowed so first-time adoption can be recorded without destructive behavior.

## Rollback Boundary

v1.4 does not roll back application binaries automatically. The safe rollback boundary is:
- keep `user_data/` in place,
- restore `user_data/data/db/backups/<backup>.sqlite3` to `user_data/data/db/odr.sqlite3`,
- reinstall or restore the earlier application/plugin files,
- rerun validation before starting OBS.

Diagnostics must not include secrets, OAuth tokens, local media contents, or full log contents.
