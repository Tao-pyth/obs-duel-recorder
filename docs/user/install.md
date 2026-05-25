# Installation

This page describes the normal Windows + OBS Studio x64 installation path for OBS Duel Recorder.

Status note: The current practical distribution format is a GitHub Release ZIP. Installer/MSI packaging is not available yet.

## Requirements

- Windows x64
- OBS Studio x64
- GitHub Release ZIP
- `SHA256SUMS.txt` from the same release

This project does not distribute Yu-Gi-Oh! Master Duel assets.

---

## Verify The ZIP

Before extracting the ZIP, compare its SHA256 hash with `SHA256SUMS.txt`.

```powershell
Get-FileHash .\obs-duel-recorder-v1.0.1.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

Continue only when the printed hash matches the release checksum.

---

## Install Into OBS

Extract the ZIP to a working folder first. Then run the install assistant from
the extracted release ZIP root:

```powershell
.\install.bat "C:\Program Files\obs-studio"
```

Use the root folder of your OBS installation. For Portable OBS, pass the
portable OBS folder. The assistant validates that the path looks like OBS,
copies the Plugin DLL and full Worker bundle, then runs the layout verifier.
It uses Windows PowerShell only and does not require Python, CMake, Visual
Studio, or other development tools.

The assistant does not modify runtime `user_data`. It does not delete existing
user files.

Manual copy is still possible if the assistant cannot write into the OBS
folder.

Expected release ZIP paths:

```text
<ZIP extract>\app\plugin\obs-duel-recorder.dll
<ZIP extract>\app\worker\odr-worker\odr-worker.exe
<ZIP extract>\app\worker\odr-worker\<other bundled Worker files>
```

Expected OBS target layout:

```text
<OBS install>\
`-- obs-plugins\
    |-- 64bit\
    |   `-- obs-duel-recorder.dll
    `-- worker\
        `-- odr-worker\
            |-- odr-worker.exe
            `-- <other bundled Worker files>
```

Copy mapping:

```text
<ZIP extract>\app\plugin\obs-duel-recorder.dll
  -> <OBS install>\obs-plugins\64bit\obs-duel-recorder.dll

<ZIP extract>\app\worker\odr-worker\
  -> <OBS install>\obs-plugins\worker\odr-worker\
```

Copy the entire `odr-worker` directory. Do not copy only `odr-worker.exe`; the Worker needs its bundled dependency files next to the executable.

Portable OBS uses the same layout under the portable OBS folder.

---

## Verify Installed Layout

After installing or manually copying files, run the verifier from the extracted release ZIP root:

```powershell
.\verify-install.bat "C:\Program Files\obs-studio"
```

Use the root folder of your OBS installation. For Portable OBS, pass the portable OBS folder.

The verifier prints the package source paths and OBS target paths it checked. It returns a failure if the Plugin DLL is missing, the Worker bundle is incomplete, or the Worker was placed under the known wrong `obs-plugins\64bit\worker\` path.

---

## Common Wrong Layout

Do not place the Worker under `obs-plugins\64bit\worker`.

Wrong layout:

```text
<OBS install>\
`-- obs-plugins\
    `-- 64bit\
        |-- obs-duel-recorder.dll
        `-- worker\
            `-- odr-worker\
                `-- odr-worker.exe
```

In packaged use, the Plugin searches for the Worker as a sibling of `64bit`:

```text
<OBS install>\obs-plugins\worker\odr-worker\odr-worker.exe
```

If the Worker is nested under `64bit`, the OBS Duel Recorder Dock can appear but Worker startup fails. Start and Stop stay disabled because those buttons are enabled only when the Worker state is `running`.

---

## First Start

1. Start OBS.
2. Confirm that the `OBS Duel Recorder` Dock appears.
3. Confirm that the Dock Worker state becomes `running`.
4. If needed, change `user_data_dir` from `Tools > OBS Duel Recorder Settings`.

When `user_data_dir` is not set explicitly, the default runtime folder is:

```text
%APPDATA%\obs-duel-recorder\user_data
```

Keep runtime data separate from the OBS plugin installation folder. The runtime folder stores configuration, DBs, logs, videos, screenshots, and exports.

---

## Troubleshooting

If Settings opens but Start and Stop are disabled:

1. Run `.\verify-install.bat "<OBS install>"` from the extracted release ZIP root.
2. Check that the Worker bundle exists at `<OBS install>\obs-plugins\worker\odr-worker\odr-worker.exe`.
3. Check that it is not incorrectly placed under `<OBS install>\obs-plugins\64bit\worker\`.
4. Restart OBS after correcting the layout.
5. Check the Dock Worker status and OBS log for Worker launch errors.

Next steps:

- [First setup](setup.md)
- [Usage](usage.md)
