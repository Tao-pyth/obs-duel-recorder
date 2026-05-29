# Python Worker

This directory is reserved for Python Worker code.

The Worker is responsible for:
- SQLite management
- queue persistence
- match state management
- template matching
- OCR processing
- YouTube uploads
- export generation
- recovery processing

The Worker must support restart-safe execution.

## v0.2 scaffold

A minimal package scaffold exists under `app/worker/odr_worker/`.

- Print version: `python -m odr_worker --version`
- Run API server (default host/port): `python -m odr_worker`

### Quick start (Windows PowerShell)

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -e app/worker
python -m odr_worker
```

Health check:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/health | Select-Object -ExpandProperty Content
```

Recording state check:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/recording/state | Select-Object -ExpandProperty Content
```

Manual recording command example:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/recording/command -Method Post -ContentType "application/json" -Body '{"action":"start","source":"manual"}' | Select-Object -ExpandProperty Content
```

Queue check:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/queue/items | Select-Object -ExpandProperty Content
```

Create a queue item:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/queue/items -Method Post -ContentType "application/json" -Body '{"video_path":"C:/path/to/duel.mp4"}' | Select-Object -ExpandProperty Content
```

Detection state check:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/detection/state | Select-Object -ExpandProperty Content
```

Template detection frame example:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/detection/frame -Method Post -ContentType "application/json" -Body '{"frame_text":"sample frame bytes or fixture text"}' | Select-Object -ExpandProperty Content
```

Template matching test without recording:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/detection/test -Method Post -ContentType "application/json" -Body '{"kind":"start","frame_text":"sample frame bytes or fixture text"}' | Select-Object -ExpandProperty Content
```

PNG template matching test without recording:

```powershell
$png = [Convert]::ToBase64String([IO.File]::ReadAllBytes("C:/path/to/current-screen.png"))
Invoke-WebRequest http://127.0.0.1:8787/detection/test -Method Post -ContentType "application/json" -Body (@{kind="start"; frame_base64=$png} | ConvertTo-Json -Compress) | Select-Object -ExpandProperty Content
```

Register a start template through setup:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/setup/templates/register -Method Post -ContentType "application/json" -Body '{"kind":"start","path":"duel-start.tpl","threshold":1.0,"confirmations":2}' | Select-Object -ExpandProperty Content
```

Register an end template through setup:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/setup/templates/register -Method Post -ContentType "application/json" -Body '{"kind":"end","path":"duel-end.tpl","threshold":1.0,"confirmations":2}' | Select-Object -ExpandProperty Content
```

Capture and register a start template through setup:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/setup/templates/capture -Method Post -ContentType "application/json" -Body '{"kind":"start","extension":"png","content_base64":"<base64 image bytes>","threshold":1.0,"confirmations":2}' | Select-Object -ExpandProperty Content
```

Screenshot capture example:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/screenshots/capture -Method Post -ContentType "application/json" -Body '{"kind":"duel-start","content_text":"local fixture bytes"}' | Select-Object -ExpandProperty Content
```

Screenshot preview example:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/screenshots/1/preview | Select-Object -ExpandProperty Content
```

Target-video representative frame preview example:

```powershell
Invoke-WebRequest "http://127.0.0.1:8787/matches/1/video-preview?frame=2" | Select-Object -ExpandProperty Content
```

This endpoint uses the video linked to the match upload queue item. It returns
base64 PNG content only when the local video exists and `ffmpeg` is available.

Upload status check:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/status | Select-Object -ExpandProperty Content
```

Mock upload processing example:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/process-next -Method Post -ContentType "application/json" -Body '{"mock_result":"success","youtube_video_id":"abc123"}' | Select-Object -ExpandProperty Content
```

Google upload processing example:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/process-next -Method Post -ContentType "application/json" -Body '{"provider":"google"}' | Select-Object -ExpandProperty Content
```

OAuth authorization URL:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/authorization-url -Method Post -ContentType "application/json" -Body '{}' | Select-Object -ExpandProperty Content
```

OAuth token refresh:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/refresh -Method Post | Select-Object -ExpandProperty Content
```

Create match metadata:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/matches -Method Post -ContentType "application/json" -Body '{"deck_name":"Sample Deck","opponent_deck":"Sample Opponent","result":"win","memo":"Match notes"}' | Select-Object -ExpandProperty Content
```

Generate upload metadata:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/matches/1/upload-metadata | Select-Object -ExpandProperty Content
```

Create export archive:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/exports -Method Post -ContentType "application/json" -Body '{"created_at":"2026-05-23T12:01:00Z"}' | Select-Object -ExpandProperty Content
```

List export archives:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/exports | Select-Object -ExpandProperty Content
```

Setup wizard status:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/setup/status | Select-Object -ExpandProperty Content
```

Setup wizard validation:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/setup/validate -Method Post | Select-Object -ExpandProperty Content
```

Update status:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/update/status | Select-Object -ExpandProperty Content
```

Update validation:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/update/validate -Method Post -ContentType "application/json" -Body '{"current_version":"1.3.0"}' | Select-Object -ExpandProperty Content
```

Canonical Windows update entrypoint:

```powershell
.\update.bat validate --from-version 1.3.0
.\update.bat apply --from-version 1.3.0
```

Configuration scaffold (v0.2):
- default config path: `user_data/config/worker.toml`
- docs: `docs/architecture/worker-config.md`

Implementation work is tracked by issues #11-#16.
