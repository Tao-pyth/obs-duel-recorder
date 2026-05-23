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

## Current Internal v2.3 Runtime Optimization

Readiness note: `v2.3` is the internal Worker/API compatibility milestone. It is not a practical user-ready release claim until the Plugin DLL is built and smoke-tested in a real OBS runtime.

The current scaffold keeps the v0.5 overlay surface, v0.6 manual recording lifecycle controls, v0.7 queue recovery API compatibility, v0.8 template detection API compatibility, v0.9 screenshot API compatibility, v1.0 upload API compatibility, v1.1 match metadata API compatibility, v1.2 export API compatibility, v1.3 setup wizard API compatibility, v1.4 update API compatibility, v2.0 image recognition API compatibility, v2.1 statistics API compatibility, v2.2 documentation publication compatibility, and v2.3 runtime optimization compatibility:

- Build a loadable OBS module.
- Register minimal OBS Frontend API lifecycle hooks.
- Log plugin startup and shutdown messages.
- Persist minimal Plugin settings as JSON.
- Probe and launch the Worker from persisted settings.
- Reuse a healthy singleton Worker for the same `ODR_USER_DATA_DIR`.
- Monitor Worker heartbeat through `GET /health`.
- Register a status-first OBS Dock for Worker diagnostics.
- Load additive `overlay` settings with v0.5 defaults.
- Find or create the fixed OBS Text Sources for deck name, sequence number, result, opponent deck, and recording state display.
- Log stable overlay diagnostics for missing, duplicated, unsupported, unavailable, or failed source operations.
- Add Dock buttons for manual OBS recording start and stop.
- Synchronize OBS recording started/stopped frontend events through the Worker recording-state API.
- Require a v0.7-compatible Worker for queue recovery and retry-safe upload queue state.
- Require a v0.8-compatible Worker for template detection and automatic recording trigger state.
- Require a v0.9-compatible Worker for screenshot capture, preview, DB linkage, and upload-safe cleanup.
- Require a v1.0-compatible Worker for upload status, mocked upload execution, and manual-review upload state.
- Require a v1.1-compatible Worker for match metadata editing and upload metadata generation.
- Require a v1.2-compatible Worker for export archive generation and export listing.
- Require a v1.3-compatible Worker for setup status, setup validation, and reset/rerun state.
- Require a v1.4-compatible Worker for update status, update validation, and DB backup/migration evidence.
- Require a v2.0-compatible Worker for recognition candidate analysis and manual review audit records.
- Require a v2.1-compatible Worker for read-only match/upload statistics and memo search.
- Require a v2.2-compatible Worker/Plugin version pair for the documented GitHub Pages release line.
- Require a v2.3-compatible internal Worker API for optimized queue/upload polling and recovery diagnostics.

Current non-goals:
- Full control UI beyond the v0.6 manual start/stop controls.
- Overlay UX.
- Full metadata editing UI.
- Full export management UI.
- Full graphical setup wizard UI.
- Full graphical updater UI.
- Full graphical recognition correction UI beyond the Worker/metadata API boundary.
- Full statistics dashboard UI beyond the Worker API boundary.
- Packaging/update distribution UI beyond the Worker API boundary.

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
  "restart_worker_on_change": true,
  "overlay": {
    "enabled": true,
    "auto_create_sources": true,
    "sources": {
      "deck_name": "ODR Deck Name",
      "sequence_number": "ODR Sequence",
      "result": "ODR Result",
      "opponent_deck": "ODR Opponent Deck",
      "recording_state": "ODR Recording State"
    },
    "defaults": {
      "deck_name": "Deck: -",
      "sequence_number": "#---",
      "result": "Result: unknown",
      "opponent_deck": "Opponent: unknown",
      "recording_state": "Idle"
    }
  }
}
```

`ODR_USER_DATA_DIR` remains a compatibility fallback for initial defaults. Once the settings file is saved, `user_data_dir` in the JSON file is the Plugin launch input.
Missing `overlay` settings are loaded with defaults so existing v0.4 settings files remain valid.

Settings flow:
- Open `Tools > OBS Duel Recorder Settings` or the `Settings` button in the Dock.
- Edit `host`, `port`, or `user_data_dir`.
- Save the dialog.
- The Plugin persists the JSON file and restarts the Worker manager with the saved settings.

v0.5 treats settings changes conservatively: saving settings restarts the Worker manager and re-runs overlay source management. OBS restart is an exception path, not the default.

## Overlay Text Sources

On OBS frontend ready and after settings are saved, the Plugin checks the configured overlay sources.

Default source names:
- `ODR Deck Name`
- `ODR Sequence`
- `ODR Result`
- `ODR Opponent Deck`
- `ODR Recording State`

Behavior:
- Existing supported OBS Text Sources are reused.
- Missing sources are created in the current scene when `overlay.auto_create_sources` is true.
- Unsupported, duplicate, unavailable, and failed operations are logged with stable diagnostic names from `docs/architecture/overlay.md`.
- The Plugin does not delete or replace unrelated user-created sources.

## Status Dock

The Plugin registers an `OBS Duel Recorder` Dock through the OBS Frontend API.

The Dock is status-first. It displays:
- diagnostic state (`not_started`, `starting`, `running`, `unhealthy`, `config_error`, `runtime_dir_error`, `api_incompatible`, `crashed`)
- endpoint
- resolved `user_data_dir`
- expected log directory
- Worker ownership (`plugin-spawned` or `reused-existing`)
- latest probe/detail text
- recommended action
- manual start/stop recording buttons

The Dock intentionally does not provide full Worker controls beyond the v0.6 manual recording lifecycle actions.

## Manual Recording Controls

The Dock `Start Recording` and `Stop Recording` buttons use the v0.6 recording-state API:

- `Start Recording` sends `POST /recording/command` with `{"action":"start","source":"manual"}` and then requests OBS recording start.
- `Stop Recording` sends `POST /recording/command` with `{"action":"stop","source":"manual"}` and then requests OBS recording stop.
- OBS `RECORDING_STARTED` and `RECORDING_STOPPED` frontend events send `confirm_started` and `confirm_stopped` commands back to the Worker.
- Command failures are logged with stable HTTP/status evidence and do not crash OBS.

## Worker Launch Inputs

Defaults:
- Worker command: bundled `app\worker\odr-worker\odr-worker.exe` when present; developer fallback `odr-worker`
- Host: `127.0.0.1`
- Port: `8787`
- Expected Worker API version: `2.3`
- Expected Worker version: `2.3.0`
- Heartbeat interval: 2000 ms
- Heartbeat timeout threshold: 3 consecutive failed probes

These Worker values are internal compatibility gates. They do not replace the practical release gate that requires a built DLL and real OBS load smoke evidence.

Startup behavior:
- On OBS frontend ready, the Plugin loads `plugin-settings.json` and starts the Worker manager.
- If `user_data_dir` is empty, startup is blocked with `config_error` and the Dock points to settings.
- The Plugin probes `GET /health` on the configured host/port.
- If a compatible Worker is already running for the same `user_data_dir`, the Plugin reuses it.
- If a reachable Worker reports a different runtime root, incompatible API version, or unexpected Worker version, startup is blocked and the Plugin logs diagnostics.
- If no Worker is reachable, the Plugin starts the bundled Worker executable when it exists in the release ZIP layout.
- If the bundled Worker executable is not present, the Plugin falls back to `odr-worker --host <host> --port <port>` for developer checkouts.
- In both cases, `ODR_USER_DATA_DIR` is set in the child process environment.

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

The scaffold registers OBS Frontend API callbacks, logs lightweight lifecycle events, runs the initial Worker preflight/launch/heartbeat flow, surfaces status-first diagnostics in a Dock, and provides v0.6 manual recording start/stop controls.

Heavy work must remain outside the plugin process. Full controls, overlay UX, and upload/OAuth setup remain later-version work.
