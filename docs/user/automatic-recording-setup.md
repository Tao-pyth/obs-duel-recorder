# Automatic Recording Setup

Status note: This page describes the v1.1.4 guided setup target. Automatic
recording still requires Plugin build and real OBS smoke validation before it
can be treated as released.

## What It Does

Automatic recording uses local start/end templates to decide when a duel begins
and ends. The OBS Plugin captures the current OBS Program image for setup. The
Worker stores templates, performs matching, and owns recording-state decisions.

The project does not distribute game assets or template images. Captured
templates are user runtime data under `user_data/` and must not be committed,
published, or included in release packages.

## Guided Setup Flow

1. Open OBS Studio and show the screen state that should mean duel start.
2. Open `OBS Duel Recorder` Dock.
3. Go to `Manage`.
4. Open automatic recording setup.
5. Click `Capture Start Screen`.
6. Show the screen state that should mean duel end.
7. Click `Capture End Screen`.
8. Click `Test Current Screen` while the target screen is visible.
9. Adjust threshold and confirmation count only if detection is too loose or
   too strict.
10. Enable `Automatic detection` in Manage settings only after both start and
    end tests are understandable.

## Controls

| Control | Purpose |
|---|---|
| `Capture Start Screen` | Saves the current OBS Program image as the start template. |
| `Capture End Screen` | Saves the current OBS Program image as the end template. |
| `Test Current Screen` | Tests the current image against both templates without changing recording state. |
| `Threshold` | Minimum match confidence. Higher values are stricter. |
| `Confirmations` | Number of consecutive matches required before a state transition. Higher values reduce false positives but react more slowly. |
| `Automatic detection` | Enables the periodic frame feed from OBS to the Worker. Keep it off while setting up templates. |
| `Frame interval` | Minimum interval between automatic frame submissions. Larger values reduce load but react more slowly. |

## Troubleshooting

Capture unavailable:
- Confirm OBS is running and the Worker is healthy.
- Confirm OBS can write screenshots to its configured output directory.
- Try again after switching to the scene/source you want to capture.

Template missing:
- Capture start and end again.
- Confirm `user_data/templates/` and `user_data/config/templates.toml` exist.

No match:
- Test with the same screen state used during capture.
- Lower threshold gradually.
- Make sure the scene, resolution, and crop are not different from the capture.

False positives:
- Raise threshold gradually.
- Increase confirmations.
- Capture a more specific screen state.

Performance issues:
- Increase `Frame interval`.
- Disable `Automatic detection` if OBS or the Worker becomes unstable.
- The v1.1.4 feed removes temporary screenshot files after reading them, but
  real OBS smoke should still confirm that screenshot files are not left behind.

Worker unreachable:
- Check Manage diagnostics first.
- Restart Worker from settings if the Dock reports it as unhealthy or stopped.

## Real OBS Smoke Checklist

- [ ] Plugin loads in OBS x64.
- [ ] Worker starts and health status becomes running.
- [ ] `Capture Start Screen` creates a runtime template without writing under the OBS install directory.
- [ ] `Capture End Screen` creates a runtime template without writing under the OBS install directory.
- [ ] `Test Current Screen` returns start/end match diagnostics without starting or stopping recording.
- [ ] `Automatic detection` can be enabled and sends bounded periodic frames only while the Worker is healthy.
- [ ] Temporary screenshot files are not left behind by the automatic feed.
- [ ] Manual Start/Stop remains usable while automatic detection is enabled.
- [ ] Logs do not contain captured image bytes, local template content, OAuth secrets, or game assets.
