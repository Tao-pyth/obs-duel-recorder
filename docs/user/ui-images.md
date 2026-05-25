# UI Images

Status contract: These images describe the v1.1.1 target UI documentation set. Final release validation may replace illustrative SVGs with real screenshots if the captured images are redaction-safe.

The images in this page are intentionally generated documentation assets. They do not contain local paths, OAuth data, screenshots, videos, templates, or game assets.

## Dock Main View

![Annotated OBS Duel Recorder Dock overview](assets/v1.1.1-dock-overview.svg)

The Dock image shows the intended v1.1.1 information hierarchy:
- status and Worker readiness first,
- setup, recording, upload, metadata, and diagnostics grouped by user task,
- diagnostics available without making raw debug text the main view,
- Settings and Help visible from the Dock.

## Settings And Theme Selection

![Settings theme selection](assets/v1.1.1-settings-themes.svg)

The Settings image shows the intended theme selection behavior. Theme choice should be visible and easy to change, but it must not block runtime path, Worker, overlay, recording, metadata, or upload setup work.

## Help Message

![Dock Help message](assets/v1.1.1-help-panel.svg)

The Help image shows the intended scope for the simple Dock Help message: setup, manual recording, automatic recording, metadata review, upload review, and diagnostics.

## Safety Rules For Replacing These Images

- Use only documentation-safe generated images or redacted screenshots.
- Do not capture game screens, private file paths, OAuth data, tokens, videos, screenshots, local templates, or logs.
- Keep the images aligned with the v1.1.1 Dock, Settings theme, and Help implementation before #450 closes.
