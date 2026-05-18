# Worker/Plugin Compatibility Contract

This document defines a minimal, stable compatibility contract between the **OBS Plugin** and the **Python Worker**.

Goal:
- Make compatibility checks explicit before implementation spreads across Plugin and Worker.
- Ensure the Dock UI can distinguish **mismatch** vs **generic unhealthy** states.

Non-goals:
- UI implementation details.
- Adding new endpoints beyond documenting the contract.

---

## Terms

- **Worker version**: the Worker binary/app version (SemVer).
- **API version**: the localhost HTTP API contract version used for compatibility gating.

For v0.x, the API version is treated as a **minor-level contract**: patch releases must remain compatible.

---

## Canonical fields

The Worker SHOULD surface these fields via `GET /health` (canonical):

- `version: string`
  - Worker version (SemVer recommended), e.g. `0.2.0`.
- `api_version: string`
  - API contract version (SemVer-like string), e.g. `0.2` for the v0.2 contract.

Optional (recommended for foreign-process / port-collision detection):

- `instance_id: string`
  - A per-process unique identifier generated at Worker startup.
  - The Plugin can use this to confirm the reachable localhost API is the intended Worker instance.

The Plugin SHOULD know its own:

- `plugin_version: string`
- `expected_api_version: string`

---

## Where compatibility is surfaced

### `GET /health` (canonical)

The Worker MUST expose a compatibility signal in `GET /health`.

Recommended minimal shape (illustrative):

```json
{
  "status": "ok",
  "version": "0.2.0",
  "api_version": "0.2",
  "instance_id": "c4dbf6b1b2a34f59a39f0bbaf43ad8a2"
}
```

The exact `/health` payload is version-scoped by acceptance criteria, but the compatibility fields above should be considered stable.

### `GET /version` (optional)

If the Worker exposes `GET /version`, it SHOULD include at least:

```json
{
  "version": "0.2.0",
  "api_version": "0.2",
  "instance_id": "c4dbf6b1b2a34f59a39f0bbaf43ad8a2"
}
```

Notes:
- `GET /version` is intended as a lightweight compatibility preflight.
- `GET /version` must not be treated as a readiness/health signal by itself.

---

## Plugin behavior (high level)

### Compatibility gating

When the Plugin can reach the Worker API and can read compatibility fields (via `/health` and/or `/version`):

- If `api_version == expected_api_version`:
  - treat as compatible and continue.

- If `api_version != expected_api_version`:
  - map to `api_incompatible` diagnostic state.
  - avoid performing further API calls (do not attempt to continue with a mismatched contract).
  - show a clear action: update Plugin and/or Worker to a compatible pair.

`version_mismatch` should be reserved for cases where a version range/matrix is explicitly defined. If no explicit rule exists yet, prefer using `api_incompatible` as the compatibility gate.

### Port-collision / foreign process preflight

Before spawning a Worker, the Plugin may do a preflight on the target `host:port`:

- Preferred: call `GET /version` first (if available), then call `GET /health` if needed.
- Fallback: call `GET /health` directly when `/version` is not implemented.

If a response is reachable but is not compatible (or the Plugin cannot confirm it is the intended Worker instance), treat it as a **launch failure** (port collision / foreign process).

### Startup vs mismatch

When the Worker process is running but `GET /health` is unreachable:
- treat as `starting` (early) or `unhealthy` (after timeout), not as a compatibility mismatch.

Even if `GET /version` succeeds, a failing `GET /health` during startup should still map to startup/health diagnostics (not mismatch) unless an explicit compatibility mismatch is observed.

---

## Related docs

- `docs/architecture/worker-diagnostics.md`
- `docs/architecture/plugin-worker.md`
- `docs/requirements/v0.2-worker-core-api-acceptance.md`

Refs: #54
