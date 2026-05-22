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

Screenshot capture example:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/screenshots/capture -Method Post -ContentType "application/json" -Body '{"kind":"duel-start","content_text":"local fixture bytes"}' | Select-Object -ExpandProperty Content
```

Screenshot preview example:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/screenshots/1/preview | Select-Object -ExpandProperty Content
```

Upload status check:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/status | Select-Object -ExpandProperty Content
```

Mock upload processing example:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/process-next -Method Post -ContentType "application/json" -Body '{"mock_result":"success","youtube_video_id":"abc123"}' | Select-Object -ExpandProperty Content
```

Configuration scaffold (v0.2):
- default config path: `user_data/config/worker.toml`
- docs: `docs/architecture/worker-config.md`

Implementation work is tracked by issues #11-#16.
