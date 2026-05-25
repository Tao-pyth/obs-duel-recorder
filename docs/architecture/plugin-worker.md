# Plugin Worker Architecture

## Responsibility Separation

| Component | Responsibility |
|---|---|
| OBS Plugin | OBS integration, UI, overlay updates |
| Python Worker | DB, queue, OCR, uploads |

---

## Plugin / Worker Architecture

This diagram shows the responsibility boundary between the lightweight OBS Plugin and the Python Worker.

```mermaid
flowchart LR
    subgraph OBS["OBS Studio"]
        Frontend["OBS Frontend API"]
        Dock["Dock UI"]
        TextSources["Text Sources / Overlays"]
        Recording["Recording Control"]
    end

    subgraph Plugin["OBS Plugin"]
        PluginCore["Plugin Core"]
        WorkerLifecycle["Worker Lifecycle"]
        Heartbeat["Heartbeat Monitor"]
        ApiClient["Localhost API Client"]
    end

    subgraph Worker["Python Worker"]
        Api["FastAPI Localhost API"]
        Queue["Queue Processing"]
        Database["SQLite Management"]
        Detection["Template Matching / OCR"]
        Upload["YouTube Upload"]
        Recovery["Recovery Processing"]
    end

    subgraph Runtime["user_data/"]
        Config["config/"]
        Data["data/"]
        Logs["logs/"]
    end

    Frontend --> PluginCore
    Dock --> PluginCore
    PluginCore --> TextSources
    PluginCore --> Recording
    PluginCore --> WorkerLifecycle
    WorkerLifecycle --> Api
    Heartbeat --> ApiClient
    ApiClient --> Api

    Api --> Queue
    Api --> Recovery
    Queue --> Database
    Detection --> Queue
    Upload --> Queue
    Worker --> Config
    Worker --> Data
    Worker --> Logs
```

---

## Plugin Responsibilities

- OBS Frontend API
- Dock UI
- Text Source updates
- Worker process management
- Worker heartbeat monitoring

---

## Worker Responsibilities

- SQLite
- Queue processing
- Recovery
- Template matching
- OCR
- YouTube upload
- Export generation

---

## Worker Launch Contract (v0.4)

This section defines the minimal contract for launching the Worker process from the OBS plugin.

Tracking:
- Child issue: #120
- Parent tracking issue: #110

Related design inputs:
- #126 instance identity and port ownership
- #127 single-instance and port-collision contract
- #144 instance_id adoption decision
- #145 reused-Worker ownership contract
- #149 persisted DB / queue reuse boundary (docs-first)

### Ownership

- The Worker is a singleton per `ODR_USER_DATA_DIR` runtime root.
- The OBS plugin owns discovery and use of the singleton Worker for its configured runtime root.
- The plugin should reuse an existing healthy singleton Worker for the same `ODR_USER_DATA_DIR`.
- The plugin should spawn a Worker only when no healthy singleton exists for that runtime root.
- The Worker is responsible for initializing its own runtime dirs and logging under `user_data/`.
- The plugin must avoid spawning multiple Workers for the same `ODR_USER_DATA_DIR`.

### Inputs

The plugin must define these launch inputs deterministically:

- `ODR_USER_DATA_DIR` (required): absolute path to the runtime root directory.
  - The Worker creates subdirectories under this root (`config/`, `data/`, `logs/`).
  - The Plugin uses `%APPDATA%\obs-duel-recorder\user_data` as the normal packaged default when `ODR_USER_DATA_DIR` is not set.
  - The singleton scope is this resolved runtime root.
- `host` / `port` (recommended): bind address for the localhost HTTP API.
  - Defaults (Worker v0.2 scaffold): `127.0.0.1:8787`.
  - Overrides are supported via CLI: `--host` / `--port`.

Optional:
- `user_data/config/worker.toml` may define `host` / `port` as defaults.

### Recommended Invocation

Packaged releases should prefer invoking the bundled Worker executable from the
OBS plugin install layout:

- `<OBS>/obs-plugins/worker/odr-worker/odr-worker.exe --host 127.0.0.1 --port 8787`

The Plugin resolves the bundled executable relative to the Plugin DLL:

```text
obs-plugins/
|-- 64bit/
|   `-- obs-duel-recorder.dll
`-- worker/
    `-- odr-worker/
        `-- odr-worker.exe
```

The release ZIP source layout uses the same parent/sibling relationship before
users copy files into OBS:

- `<package>/app/worker/odr-worker/odr-worker.exe --host 127.0.0.1 --port 8787`

```text
app/
|-- plugin/
|   `-- obs-duel-recorder.dll
`-- worker/
    `-- odr-worker/
        `-- odr-worker.exe
```

For developer checkouts, the plugin may fall back to the installed CLI
entrypoint:

- `odr-worker --host 127.0.0.1 --port 8787`

If the plugin embeds Python or launches via `python -m`, the command must still behave equivalently and use the same `ODR_USER_DATA_DIR`. That path is not the normal user install path for packaged releases.

### Startup Handshake

- Before spawning, the plugin should discover whether a healthy singleton Worker already exists for the resolved `ODR_USER_DATA_DIR`.
- After spawning the Worker, the plugin must wait until the API becomes ready.
- Readiness signal: successful response from `GET /health`.
- The plugin should apply a bounded timeout and surface actionable diagnostics when startup fails.

### Single-instance and Port-Collision Handling

The plugin must avoid accidentally talking to the wrong localhost process.

Minimum policy (v0.4):

- Preflight check: before spawning, try `GET /health` on the target `host:port`.
- If `/health` is reachable, compatible, and identifies the same `ODR_USER_DATA_DIR` scope, the plugin should treat it as the already-running singleton Worker and reuse it.
- If `/health` is reachable but is not compatible, or the plugin cannot confirm the same runtime-root singleton scope, treat it as a **launch failure** (port collision / foreign process) and do not continue as "running".
- If the port is in use but `/health` is not reachable or returns invalid responses, treat it as a **launch failure** (stale/unknown process) and do not automatically kill the process.
- If the Worker exposes `instance_id`, the plugin should use it as a diagnostic signal for singleton invariant violations, stale processes, and wrong-port connections. `instance_id` is not the primary ownership gate.

Optional refinement:
- If the Worker implements `GET /version`, the plugin may use it as a lighter preflight (read `version` / `api_version` / `instance_id`) before calling `/health`.

The definition of the intended Worker is the singleton Worker for the resolved `ODR_USER_DATA_DIR`. `instance_id` can help diagnose unexpected process changes, but the runtime-root singleton is the primary contract.

### Persisted DB / Queue Reuse Boundary (docs-first)

When the Plugin discovers an already-running healthy singleton Worker for the same `ODR_USER_DATA_DIR`, the Plugin should prefer reuse. However, **reusing the runtime root does not automatically mean every persisted state under it is safe to reuse**.

Minimum policy (v0.4 docs-first):

- The Plugin/Worker MUST treat `ODR_USER_DATA_DIR` as the persisted-state boundary (config, DB, queue, logs all live under this root).
- The Worker SHOULD surface enough persisted-state evidence on startup (logs and/or diagnostics) so that "right runtime root / wrong persisted state" can be distinguished.
- If the Worker detects a persisted-state condition it cannot safely interpret, it MUST fail closed (do not silently continue) and surface **manual action required** diagnostics instead.

Recommended persisted-state evidence (v0.4):
- resolved `ODR_USER_DATA_DIR`
- `version` / `api_version` (from `/health`)
- DB openability + `schema_version` (or an equivalent migration marker)
- queue state summary (counts of in-flight items such as `uploading`, `need_manual_review`, etc., when available)

This policy is intentionally minimal and is expanded/validated in:
- persisted DB / queue reuse boundary: #149
- SQLite migration/contract: #90

### Handoff to Other v0.4 Concerns

- Heartbeat cadence/timeout and failure handling lives in #121.
- Diagnostics mapping and wrapper behavior for collision cases lives in #123.
- The exact `instance_id` mismatch behavior lives in #144.

### Shutdown Contract

- The plugin must not stop a reused singleton Worker unless ownership rules say it is safe to do so.
- If the plugin spawned the singleton Worker for the current runtime root, it should stop that Worker when OBS is shutting down or when the user explicitly stops it.
- Preferred flow for plugin-owned Workers:
  - graceful terminate with a short timeout, then
  - force kill as a fallback.
- For reused Workers, the plugin should first surface a diagnostic or confirmation path instead of blindly terminating a process that may be shared by another OBS instance.

### Failure Reporting

The plugin should surface at least:

- worker exit code (if available)
- the resolved `ODR_USER_DATA_DIR`
- the Worker log directory (`<ODR_USER_DATA_DIR>/logs/`)
- the API target (`host:port`) used for health checks
- observed `instance_id` when available
- the expected packaged Worker path:
  `<OBS>/obs-plugins/worker/odr-worker/odr-worker.exe`
- whether the known wrong nested path exists:
  `<OBS>/obs-plugins/64bit/worker/odr-worker/odr-worker.exe`

For v1.1 launch failures, the Dock detail and OBS log should share the same
failure category (`missing`, `not_executable_or_access_denied`,
`not_executable_or_wrong_architecture`, or `failed_to_start`) and should include
the checked command plus the expected packaged Worker path. These diagnostics
must not include OAuth tokens, client secrets, bearer strings, or environment
dumps.

---

## Communication

Plugin and Worker communicate using localhost HTTP APIs.

For Dock UI behavior and actionable diagnostics, see:
- `docs/architecture/worker-diagnostics.md`

For compatibility gating between the Plugin and Worker, see:
- `docs/architecture/compatibility.md`

For runtime directory rules and startup-safe directory creation, see:
- `docs/architecture/runtime-dirs.md`

For v0.2 Worker logging behavior, see:
- `docs/architecture/worker-logging.md`

For the v0.2 Worker API contract, see:
- `docs/architecture/worker-api.md`

Example:
- GET /health
- GET /version
- POST /events/recording-started
- POST /events/recording-stopped
