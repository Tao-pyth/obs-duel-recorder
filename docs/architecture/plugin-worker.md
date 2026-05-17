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

### Ownership

- The OBS plugin owns the Worker process lifecycle (spawn / stop).
- The Worker is responsible for initializing its own runtime dirs and logging under `user_data/`.
- The plugin must avoid spawning multiple Workers for the same `ODR_USER_DATA_DIR`.

### Inputs

The plugin must define these launch inputs deterministically:

- `ODR_USER_DATA_DIR` (required): absolute path to the runtime root directory.
  - The Worker creates subdirectories under this root (`config/`, `data/`, `logs/`).
  - If unset, the Worker defaults to `<repo>/user_data/` (repo-layout dependent).
- `host` / `port` (recommended): bind address for the localhost HTTP API.
  - Defaults (Worker v0.2 scaffold): `127.0.0.1:8787`.
  - Overrides are supported via CLI: `--host` / `--port`.

Optional:
- `user_data/config/worker.toml` may define `host` / `port` as defaults.

### Recommended Invocation

The plugin should prefer invoking the packaged CLI entrypoint when available:

- `odr-worker --host 127.0.0.1 --port 8787`

If the plugin embeds Python or launches via `python -m`, the command must still behave equivalently and use the same `ODR_USER_DATA_DIR`.

### Startup Handshake

- After spawning the Worker, the plugin must wait until the API becomes ready.
- Readiness signal: successful response from `GET /health`.
- The plugin should apply a bounded timeout and surface actionable diagnostics when startup fails.

### Single-instance and Port-Collision Handling

The plugin must avoid accidentally talking to the wrong localhost process.

Minimum policy (v0.4):

- Preflight check: before spawning, try `GET /health` on the target `host:port`.
- If `/health` is reachable and the response is compatible with the plugin expectation, the plugin may treat it as an already-running Worker and reuse it.
- If `/health` is reachable but is not compatible, or the plugin cannot confirm it is the intended Worker instance, treat it as a **launch failure** (port collision / foreign process) and do not continue as "running".
- If the port is in use but `/health` is not reachable or returns invalid responses, treat it as a **launch failure** (stale/unknown process) and do not automatically kill the process.

Optional refinement:
- If the Worker implements `GET /version`, the plugin may use it as a lighter preflight (read `version` / `api_version`) before calling `/health`.

The definition of "intended Worker instance" may be minimal initially (e.g. same `ODR_USER_DATA_DIR` and compatible `/health` fields) and can be tightened later.

### Handoff to Other v0.4 Concerns

- Heartbeat cadence/timeout and failure handling lives in #121.
- Diagnostics mapping and wrapper behavior for collision cases lives in #123.

### Shutdown Contract

- The plugin must stop the Worker when OBS is shutting down or when the user explicitly stops it.
- Preferred flow:
  - graceful terminate with a short timeout, then
  - force kill as a fallback.

### Failure Reporting

The plugin should surface at least:

- worker exit code (if available)
- the resolved `ODR_USER_DATA_DIR`
- the Worker log directory (`<ODR_USER_DATA_DIR>/logs/`)
- the API target (`host:port`) used for health checks

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
