# Troubleshooting

This page is the normal-user troubleshooting entry point. Do not post secrets, OAuth tokens, client secrets, local videos, screenshots, logs, or databases to issues or pull requests.

## Settings Opens But Start And Stop Are Disabled

Settings can open even when the Worker failed to start. Manual Start and Stop are enabled only after the Worker state becomes `running`.

Run the install layout verifier from the extracted release ZIP:

```powershell
.\verify-install.bat "C:\Program Files\obs-studio"
```

Use the root folder of your OBS installation. Portable OBS uses the portable OBS folder.

The verifier checks these paths:

```text
<OBS install>\obs-plugins\64bit\obs-duel-recorder.dll
<OBS install>\obs-plugins\worker\odr-worker\odr-worker.exe
```

It also detects the known wrong placement:

```text
<OBS install>\obs-plugins\64bit\worker\
```

If the Worker is under `64bit\worker`, move or copy the full `odr-worker` directory to:

```text
<OBS install>\obs-plugins\worker\odr-worker\
```

Restart OBS after correcting the layout.

## Worker Does Not Start

1. Confirm that the `OBS Duel Recorder` Dock is visible.
2. Run `verify-install.bat` against the OBS root.
3. Confirm that the Worker bundle contains `odr-worker.exe` and bundled dependency files, not only the executable.
4. Check the OBS log for Worker launch errors.

If the verifier reports a missing package source path, run it from the extracted release ZIP root.
