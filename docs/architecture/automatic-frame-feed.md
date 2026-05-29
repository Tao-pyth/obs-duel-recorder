# Automatic Frame Feed Design

## Scope

This document defines the v1.1.4 target design for sending periodic OBS frames
to the Worker for automatic template detection.

The frame feed is intentionally separate from the manual automatic setup
capture path, but v1.1.4 shares the same OBS frontend screenshot capture
transport as a conservative first release implementation:

- manual setup may use `obs_frontend_take_screenshot()` because the user
  explicitly requested a one-off capture or test
- automatic frame feed is disabled by default and must be explicitly enabled
  in Plugin settings
- automatic frame feed reads one screenshot frame, sends the PNG bytes to the
  Worker, and removes the temporary screenshot file after reading it
- automatic frame feed must be bounded and disabled when Worker health is not
  known-good

## Responsibility Boundary

OBS Plugin responsibilities:

- decide whether the feed is enabled
- capture OBS Program frames at a bounded interval
- encode the bounded frame payload
- send `frame_base64` to `POST /detection/frame`
- log status without image bytes

Python Worker responsibilities:

- validate frame payloads
- load local templates from `user_data/`
- perform matching
- apply threshold and confirmation rules
- own lifecycle transitions and recording state requests

The Plugin must not perform template matching, OCR, or SQLite writes.

## v1.1.4 Capture Strategy

The v1.1.4 implementation uses a setting-gated screenshot transport because it
is the same OBS frontend surface already used by the guided setup capture/test
flow and can be validated in the current release. The Plugin:

1. checks `automatic_detection_enabled`
2. verifies Worker state is `running`
3. skips sending while a manual recording state is active
4. waits for `automatic_detection_interval_ms` to elapse
5. calls `obs_frontend_take_screenshot()`
6. reads the resulting PNG as `frame_base64`
7. removes the temporary screenshot file after reading it
8. posts the frame to `/detection/frame`

The default interval is `5000 ms`, with a supported settings range of
`1000..60000 ms`.

The next hardening step is an in-memory OBS graphics readback path. The OBS SDK
references for that future implementation are:

- `obs_add_main_rendered_callback`
- `obs_remove_main_rendered_callback`
- `obs_get_main_texture`
- `gs_texrender_create`
- `gs_texrender_begin`
- `gs_stage_texture`
- `gs_stagesurface_create`
- `gs_stagesurface_map`
- `gs_stagesurface_unmap`

The OBS DeckLink UI output implementation uses the same general pattern:

1. register a rendered callback
2. render or copy the current texture into a small `gs_texrender`
3. stage the texture through a small ring of `gs_stagesurface` buffers
4. map a completed surface on a later frame
5. copy only the bounded BGRA bytes needed for the downstream consumer

That future implementation should keep the detection feed smaller than the
recording canvas. The recommended target is:

- maximum width: `480`
- preserve aspect ratio
- interval: one frame every `1000 ms` by default
- ring buffer: at least `3` staged surfaces
- payload: PNG `frame_base64` sent to `/detection/frame`

## Runtime Gating

The feed may run only when all of the following are true:

- the user has enabled automatic recording
- `automatic_detection_enabled` is true in Plugin settings
- Worker diagnostic state is `running`
- the minimum interval has elapsed
- OBS is not shutting down
- the Worker recording state is not a non-idle manual recording state

The feed must pause or stop when:

- Worker is not reachable
- OBS exits or the Dock is unregistered
- repeated send failures exceed a small threshold

Manual Start/Stop must remain independent of this feed. The feed must not block
manual recording controls.

## Transport Contract

The Plugin transport path for the periodic feed is:

```text
WorkerProcessManager::send_detection_frame_base64
LocalhostApiClient::send_detection_frame_base64
POST /detection/frame
```

The request body is:

```json
{"frame_base64":"<base64 PNG bytes>"}
```

The Plugin must never log `frame_base64` or raw frame bytes.

## Open Validation

This design is not considered release-complete until the Plugin Release build
and real OBS smoke confirm:

- temporary screenshot files are not left behind by the feed
- CPU/GPU cost remains acceptable at the configured interval
- Worker unhealthy/unreachable state pauses feed safely
- start/end detection can drive OBS recording through Worker state
- manual Start/Stop remains usable while the feed is enabled
