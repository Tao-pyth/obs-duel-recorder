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

The current scaffold is intentionally minimal for #119:

- Build a loadable OBS module.
- Register minimal OBS Frontend API lifecycle hooks.
- Log plugin startup and shutdown messages.

Current non-goals:
- Worker process launch.
- Worker heartbeat monitoring.
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

## Manual Smoke

Use the canonical v0.4 smoke procedure:

- `docs/architecture/v0.4-smoke.md`

For #119, the minimum passing evidence is:

- The CMake configure and build commands used.
- The produced plugin DLL path.
- OBS loads the plugin without crashing.
- OBS or ODR logs include:
  - `OBS Duel Recorder plugin startup`
  - `OBS Duel Recorder plugin shutdown`

## Current Lifecycle Surface

The scaffold registers an OBS Frontend API callback and logs only lightweight lifecycle events.

Heavy work must remain outside the plugin process. Worker launch, heartbeat, settings, and Dock behavior must be added through their own v0.4 issues.
