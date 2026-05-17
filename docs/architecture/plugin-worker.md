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
- POST /events/recording-started
- POST /events/recording-stopped
