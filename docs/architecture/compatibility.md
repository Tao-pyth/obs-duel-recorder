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

The Worker SHOULD surface these fields via `GET /health`:

- `version: string`
  - Worker version (SemVer recommended), e.g. `0.2.0`.
- `api_version: string`
  - API contract version (SemVer-like string), e.g. `0.2` for the v0.2 contract.

The Plugin SHOULD know its own:

- `plugin_version: string`
- `expected_api_version: string`

---

## Where compatibility is surfaced

### `GET /health`

The Worker MUST expose a compatibility signal in `GET /health`.

Recommended minimal shape (illustrative):

```json
{
  "status": "ok",
  "version": "0.2.0",
  "api_version": "0.2"
}
```

The exact v0.2 `/health` payload is defined by v0.2 docs and acceptance criteria, but the compatibility fields above should be considered stable.

---

## Plugin behavior (high level)

When the Plugin can reach the Worker API:

- If `api_version == expected_api_version`:
  - treat as compatible and continue.

- If `api_version != expected_api_version`:
  - map to `api_incompatible` diagnostic state.
  - avoid performing further API calls (do not attempt to continue with a mismatched contract).
  - show a clear action: update Plugin and/or Worker to a compatible pair.

When the Worker is running but `GET /health` is unreachable:
- treat as `starting` (early) or `unhealthy` (after timeout), not as a compatibility mismatch.

---

## Related docs

- `docs/architecture/worker-diagnostics.md`
- `docs/requirements/v0.2-worker-core-api-acceptance.md`

Refs: #54
