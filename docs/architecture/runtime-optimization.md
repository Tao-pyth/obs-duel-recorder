# Runtime Optimization

v2.3 improves long-session runtime stability without changing ownership boundaries.

## Baseline And Thresholds

The v2.3 baseline uses deterministic local fixtures instead of user media:

| Area | v2.2 baseline risk | v2.3 target threshold | Evidence |
|---|---|---|---|
| Upload status | `/upload/status` materializes all queue rows before counting states. | State counts use SQL aggregation and do not instantiate queue payloads. | `QueueStore.count_by_state()` and upload tests. |
| Upload processing | `/upload/process-next` lists every `ready_upload` row and picks the first item. | Next ready item uses `ORDER BY id LIMIT 1`. | `QueueStore.next_ready_item()` and queue tests. |
| Queue recovery | Startup recovery returns recovered rows but not scan timing/count evidence. | Recovery reports scanned count, recovered count, and duration. | `/queue/recovery` fixture tests. |
| DB access | Queue polling depends on state scans. | Runtime queue indexes cover `(state, id)` and `(state, updated_at)`. | Migration `0008_runtime_optimization_indexes`. |
| Long session | Sustained queue/status checks rely on behavior tests only. | Fixture smoke covers repeated queue/status/recovery paths without committing runtime data. | Worker unit tests and release evidence. |

Regression review should fail if a future change reintroduces full queue materialization for status counts or first-ready upload selection.

## Implemented v2.3 Optimizations

- `QueueStore.count_by_state()` counts upload queue states with SQL `GROUP BY`.
- `UploadStore.status()` uses `count_by_state()` instead of loading every queue item.
- `QueueStore.next_ready_item()` retrieves the next upload candidate with `ORDER BY id LIMIT 1`.
- `UploadStore.process_next()` uses `next_ready_item()` and preserves existing retry, quota, auth, ambiguous outcome, and manual review semantics.
- Queue SQLite connections set `PRAGMA busy_timeout = 5000` to reduce transient lock failures during local long-session operation.
- Migration `0008_runtime_optimization_indexes` adds queue state indexes for polling and recovery.
- Startup recovery diagnostics include `scanned_count`, `recovered_count`, and `duration_ms`.

## Measurement Environment

Local fixture measurements are run on Windows with repository-local temporary `user_data` directories. Fixtures create SQLite rows and small placeholder video files only; they do not use real videos, screenshots, OAuth files, logs, tokens, or game assets.

Recommended commands:

```powershell
python -m unittest app.worker.tests.test_queue app.worker.tests.test_upload app.worker.tests.test_db
python -m unittest discover -s app\worker\tests
```

## Long-Session Smoke

For a long-session smoke, repeat the following fixture sequence against a temporary runtime:

1. Create multiple queue items with mixed `ready_upload`, `uploading`, `upload_failed`, `quota_waiting`, `need_manual_review`, and `uploaded` states.
2. Call `/upload/status` repeatedly and confirm counts remain stable.
3. Call `/upload/process-next` and confirm the lowest-id ready item is selected.
4. Restart the Worker app fixture and call `/queue/recovery`.
5. Confirm interrupted uploads move conservatively to manual review unless a success marker or missing local file rule applies.

## Preservation Rules

Optimizations must preserve:
- Worker API compatibility
- Plugin launch, heartbeat, and diagnostic behavior
- queue state transitions
- upload failure classification
- SQLite migration safety
- `user_data/` persistence

## Non-Goals

- Removing diagnostics solely to reduce overhead.
- Deleting or compacting user data without explicit user action.
- Adding heavyweight runtime dependencies without documented need.
