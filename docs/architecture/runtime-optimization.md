# Runtime Optimization

v2.3 improves long-session runtime stability without changing ownership boundaries.

## Baseline First

Every optimization should start with baseline evidence:
- memory usage
- upload processing behavior
- queue recovery behavior
- DB query or migration behavior
- long-session smoke or fixture notes

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
