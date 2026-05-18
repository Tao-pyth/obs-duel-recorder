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
- **Runtime root**: the resolved `ODR_USER_DATA_DIR` used by the Worker for config, data, and logs.
- **Singleton Worker**: the only Worker process that should serve one runtime root.

For v0.x, the API version is treated as a **minor-level contract**: patch releases must remain compatible.

---

## Canonical fields

The Worker SHOULD surface these fields via `GET /health` (canonical):

- `version: string`
  - Worker version (SemVer recommended), e.g. `0.2.0`.
- `api_version: string`
  - API contract version (SemVer-like string), e.g. `0.2` for the v0.2 contract.

Optional diagnostic fields:

- `instance_id: string`
  - A per-process unique identifier generated at Worker startup.
  - The Plugin can use this to diagnose stale-process, wrong-port, or singleton invariant violations.
  - `instance_id` is not the primary ownership gate in v0.4; the primary ownership scope is the singleton Worker for the resolved `ODR_USER_DATA_DIR`.
- `pid: number`
  - The Worker process id at the time the response is generated.
  - Diagnostic evidence only (e.g. log correlation); it MUST NOT be treated as an ownership gate.
- `started_at: string`
  - Worker start timestamp in ISO 8601 format.
  - Diagnostic evidence only (e.g. log correlation); it MUST NOT be treated as an ownership gate.

The Plugin SHOULD know its own:

- `plugin_version: string`
- `expected_api_version: string`
- resolved `ODR_USER_DATA_DIR`

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

### Singleton and port-collision preflight

Before spawning a Worker, the Plugin should do a preflight on the target `host:port`:

- Preferred: call `GET /version` first (if available), then call `GET /health` if needed.
- Fallback: call `GET /health` directly when `/version` is not implemented.

If a response is reachable, compatible, and belongs to the same runtime-root singleton scope, the Plugin should reuse that Worker instead of spawning another one.

If a response is reachable but is not compatible, or the Plugin cannot confirm the same runtime-root singleton scope, treat it as a **launch failure** (port collision / foreign process).

`instance_id` handling:
- Missing `instance_id` should not block v0.4 release readiness by itself.
- If `instance_id` changes unexpectedly during the same runtime-root session, surface a diagnostic warning or error for stale-process / wrong-port investigation.
- `instance_id` should support singleton diagnostics, not replace runtime-root singleton ownership.

### Startup vs mismatch

When the Worker process is running but `GET /health` is unreachable:
- treat as `starting` (early) or `unhealthy` (after timeout), not as a compatibility mismatch.

Even if `GET /version` succeeds, a failing `GET /health` during startup should still map to startup/health diagnostics (not mismatch) unless an explicit compatibility mismatch is observed.

---

## Related docs

- `docs/architecture/worker-diagnostics.md`
- `docs/architecture/plugin-worker.md`
- `docs/requirements/v0.2-worker-core-api-acceptance.md`

Refs: #54 #144
Note: persisted DB / queue reuse boundary is tracked separately in #149.
