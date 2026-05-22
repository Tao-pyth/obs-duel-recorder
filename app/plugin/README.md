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

The current scaffold is intentionally minimal for #119/#120/#121/#122/#123/#144:

- Build a loadable OBS module.
- Register minimal OBS Frontend API lifecycle hooks.
- Log plugin startup and shutdown messages.
- Persist minimal Plugin settings as JSON.
- Probe and launch the Worker from persisted settings.
- Reuse a healthy singleton Worker for the same `ODR_USER_DATA_DIR`.
- Monitor Worker heartbeat through `GET /health`.
- Register a status-first OBS Dock for Worker diagnostics.

Current non-goals:
- Full control UI.
- Overlay UX.
- OAuth/upload setup.

## Prerequisites

- Windows x64
- OBS Studio latest stable x64
- Visual Studio 2022 with C++ desktop workload
- CMake 3.24+
- OBS CMake package files and development headers/libraries available through `CMAKE_PREFIX_PATH`
- Qt 6 Widgets development package available to CMake

The exact OBS SDK or source/build location depends on the contributor environment. Point `CMAKE_PREFIX_PATH` at the directory that exposes `libobs`, `obs-frontend-api`, and Qt 6 CMake packages.

## Build

Run from the repository root:

```powershell
cmake -S app/plugin -B build/plugin -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Path\To\obs-studio-or-sdk;C:\Path\To\Qt\6.x.x\msvc2022_64"
cmake --build build/plugin --config Release
```

Expected artifact:

```text
build/plugin/Release/obs-duel-recorder.dll
```

## Settings

Settings are saved as JSON at:

```text
%APPDATA%\obs-duel-recorder\plugin-settings.json
```

Current keys:

```json
{
  "host": "127.0.0.1",
  "port": 8787,
  "user_data_dir": "C:/path/to/user_data",
  "restart_worker_on_change": true
}
```

`ODR_USER_DATA_DIR` remains a compatibility fallback for initial defaults. Once the settings file is saved, `user_data_dir` in the JSON file is the Plugin launch input.

Settings flow:
- Open `Tools > OBS Duel Recorder Settings` or the `Settings` button in the Dock.
- Edit `host`, `port`, or `user_data_dir`.
- Save the dialog.
- The Plugin persists the JSON file and restarts the Worker manager with the saved settings.

v0.4 treats settings changes conservatively: saving settings restarts the Worker manager. Immediate apply is reserved for explicitly safe future fields. OBS restart is an exception path, not the default.

## Status Dock

The Plugin registers an `OBS Duel Recorder` Dock through the OBS Frontend API.

The v0.4 Dock is status-first. It displays:
- diagnostic state (`not_started`, `starting`, `running`, `unhealthy`, `config_error`, `runtime_dir_error`, `api_incompatible`, `crashed`)
- endpoint
- resolved `user_data_dir`
- expected log directory
- Worker ownership (`plugin-spawned` or `reused-existing`)
- latest probe/detail text
- recommended action

The Dock intentionally does not provide full Worker controls in v0.4.

## Worker Launch Inputs

Defaults:
- Worker command: `odr-worker`
- Host: `127.0.0.1`
- Port: `8787`
- Expected Worker API version: `0.3`
- Expected Worker version: `0.3.0`
- Heartbeat interval: 2000 ms
- Heartbeat timeout threshold: 3 consecutive failed probes

Startup behavior:
- On OBS frontend ready, the Plugin loads `plugin-settings.json` and starts the Worker manager.
- If `user_data_dir` is empty, startup is blocked with `config_error` and the Dock points to settings.
- The Plugin probes `GET /health` on the configured host/port.
- If a compatible Worker is already running for the same `user_data_dir`, the Plugin reuses it.
- If a reachable Worker reports a different runtime root, incompatible API version, or unexpected Worker version, startup is blocked and the Plugin logs diagnostics.
- If no Worker is reachable, the Plugin starts `odr-worker --host <host> --port <port>` with `ODR_USER_DATA_DIR` in the child process environment.

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

For #119/#120/#121/#122/#123/#144, the minimum passing evidence is:

- The CMake configure and build commands used.
- The produced plugin DLL path.
- OBS loads the plugin without crashing.
- OBS or ODR logs include:
  - `OBS Duel Recorder plugin startup`
  - `OBS Duel Recorder plugin shutdown`
  - dock registration result
  - settings save path or loaded settings path
  - worker preflight or launch result
  - heartbeat baseline and either heartbeat success or failure evidence
  - `ODR_USER_DATA_DIR` or persisted `user_data_dir`
  - Worker ownership (`plugin-spawned` or `reused-existing`)
- The Dock displays the Worker diagnostic state and settings entry point.

After manual smoke, collect a redaction-ready report with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\collect_v0_4_plugin_smoke.ps1 `
  -ObsPrefix "C:\Path\To\obs-studio-or-sdk" `
  -QtPrefix "C:\Path\To\Qt\6.x.x\msvc2022_64" `
  -UserDataDir "C:\Path\To\user_data" `
  -ObsLogPath "$env:APPDATA\obs-studio\logs\<obs-log>.txt" `
  -DockState "running" `
  -SettingsFlow "saved host/port/user_data_dir; apply=worker_restart observed" `
  -WorkerOwnership "plugin-spawned"
```

The script writes `build/plugin/v0.4-plugin-smoke-report.md` by default.

## Current Lifecycle Surface

The scaffold registers an OBS Frontend API callback, logs lightweight lifecycle events, runs the initial Worker preflight/launch/heartbeat flow, and surfaces status-first diagnostics in a Dock.

Heavy work must remain outside the plugin process. Full controls, overlay UX, and upload/OAuth setup remain later-version work.
