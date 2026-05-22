# OBS Plugin

This directory contains the OBS Plugin code.

The Plugin is responsible for:
- OBS Frontend API integration
- Dock UI integration
- overlay and Text Source updates
- Worker process launch
- Worker heartbeat monitoring
- OBS lifecycle detection

The Plugin must remain lightweight.

The Plugin must not:
- perform heavy image processing
- perform OCR
- directly manipulate SQLite
- directly upload to YouTube

## Current v0.4 Scaffold

The current scaffold is intentionally minimal for #119/#120/#121/#123/#144:

- Build a loadable OBS module.
- Register minimal OBS Frontend API lifecycle hooks.
- Log plugin startup and shutdown messages.
- Probe and launch the Worker when `ODR_USER_DATA_DIR` is explicitly configured.
- Reuse a healthy singleton Worker for the same `ODR_USER_DATA_DIR`.
- Monitor Worker heartbeat through `GET /health`.

Current non-goals:
- Settings UI.
- Browser Dock UI.

Those items are tracked by separate v0.4 child issues.

## Prerequisites

- Windows x64
- OBS Studio latest stable x64
- Visual Studio 2022 with C++ desktop workload
- CMake 3.24+
- OBS CMake package files and development headers/libraries available through `CMAKE_PREFIX_PATH`

The exact OBS SDK or source/build location depends on the contributor environment. Point `CMAKE_PREFIX_PATH` at the directory that exposes `libobs` and `obs-frontend-api` CMake packages.

## Build

Run from the repository root:

```powershell
cmake -S app/plugin -B build/plugin -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Path\To\obs-studio-or-sdk"
cmake --build build/plugin --config Release
```

Expected artifact:

```text
build/plugin/Release/obs-duel-recorder.dll
```

## Worker Launch Inputs

The v0.4 launch path is intentionally conservative until the settings page exists.

Required environment:
- `ODR_USER_DATA_DIR`: absolute runtime root to pass to the Worker.

Defaults:
- Worker command: `odr-worker`
- Host: `127.0.0.1`
- Port: `8787`
- Expected Worker API version: `0.3`
- Expected Worker version: `0.3.0`
- Heartbeat interval: 2000 ms
- Heartbeat timeout threshold: 3 consecutive failed probes

Startup behavior:
- On OBS frontend ready, the Plugin probes `GET /health` on the configured host/port.
- If a compatible Worker is already running for the same `ODR_USER_DATA_DIR`, the Plugin reuses it.
- If a reachable Worker reports a different runtime root, incompatible API version, or unexpected Worker version, startup is blocked and the Plugin logs diagnostics.
- If no Worker is reachable, the Plugin starts `odr-worker --host 127.0.0.1 --port 8787` with `ODR_USER_DATA_DIR` in the child process environment.

Heartbeat behavior:
- `/health` is the readiness and heartbeat source of truth.
- The Plugin records a heartbeat baseline from `api_version`, `version`, `instance_id`, `pid`, `started_at`, and `user_data_dir`.
- Consecutive probe failures are logged with the target `host:port`, `ODR_USER_DATA_DIR`, and baseline identity evidence.
- After the failure threshold is reached, the Plugin logs heartbeat timeout evidence.
- If `instance_id`, `pid`, or `started_at` changes during a healthy heartbeat sequence, the Plugin logs `unexpected_process_change` evidence and does not treat those fields as ownership gates.

Shutdown behavior:
- Plugin-spawned Workers are stopped on OBS shutdown/plugin unload.
- Reused existing Workers are not stopped by the Plugin.

## Manual Smoke

Use the canonical v0.4 smoke procedure:

- `docs/architecture/v0.4-smoke.md`

For #119/#120/#121/#123/#144, the minimum passing evidence is:

- The CMake configure and build commands used.
- The produced plugin DLL path.
- OBS loads the plugin without crashing.
- OBS or ODR logs include:
  - `OBS Duel Recorder plugin startup`
  - `OBS Duel Recorder plugin shutdown`
  - worker preflight or launch result
  - heartbeat baseline and either heartbeat success or failure evidence
  - `ODR_USER_DATA_DIR`
  - Worker ownership (`plugin-spawned` or `reused-existing`)

## Current Lifecycle Surface

The scaffold registers an OBS Frontend API callback, logs lightweight lifecycle events, and runs the initial Worker preflight/launch/heartbeat flow.

Heavy work must remain outside the plugin process. Settings and Dock behavior must be added through their own v0.4 issues.
