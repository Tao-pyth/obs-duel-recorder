# Dock Iconography

Tracking issue: #476

## Policy

v1.1.4 uses icon + text for major Dock actions. Text remains visible because
the Dock is used in Japanese and English and the action must remain clear in a
narrow OBS panel.

The implemented fallback uses Qt standard icons so the Plugin can build and run
without packaging external SVG files. Material Design Icons from Pictogrammers
remain the selected external icon source for a future asset pass.

## Pictogrammers / MDI License

Source:

- https://pictogrammers.com/library/mdi/
- https://pictogrammers.com/docs/general/license/

License decision:

- Material Design Icons, `@mdi/js`, `@mdi/svg`, and `@mdi/font` are listed as
  Apache License 2.0.
- Brand / Logos category icons are excluded because Pictogrammers states those
  are not covered by the common license note and are being deprecated for legal
  reasons.
- If individual SVG files are bundled later, the release package must include a
  third-party notice naming Pictogrammers, Material Design Icons, and Apache
  License 2.0.

## Icon Mapping

| Control | Implemented Qt icon role | Future MDI candidate | Rationale |
|---|---|---|---|
| Start Recording | media play | `play-circle` or `record-circle` | Start action; keep green emphasis. |
| Stop Recording | media stop | `stop-circle` | Stop action; keep red/error emphasis. |
| Reload | reload | `refresh` | Refreshes the metadata target. |
| Save | save | `content-save` | Persists metadata or settings. |
| Preview | information/preview | `eye` or `file-eye` | Shows generated upload text before use in the Template tab. |
| Retry Upload | reload | `replay` or `cloud-sync` | Reattempts current failed/manual item. |
| Discard Upload | discard | `trash-can-outline` | Destructive queue action. |
| Mark Uploaded | apply/check | `cloud-check` or `check-circle` | Marks an item complete with video ID evidence. |
| Save Settings | save | `content-save-cog` | Persists settings and may restart Worker. |
| Automatic Recording Setup | computer/capture | `cog-play` or `tune` | Opens guided start/end template setup. |
| Current-screen capture | computer/capture | `monitor-screenshot` | Captures OBS Program screen for setup/test. |
| Show/Hide Details | detailed view | `information-outline` or `chevron-down` | Reveals diagnostics without dominating the layout. |

The representative-frame selectors intentionally use plain text labels `1`,
`2`, and `3` instead of arrow icons. They choose one of three still frames and
are not previous/next navigation controls.

The Dock no longer exposes an `Open Settings` button inside Manage. Runtime,
theme, and language fields are already editable inline, and saving them from the
same surface avoids duplicate settings dialogs.

## Tooltip Rules

- Every icon-bearing button must have a tooltip.
- Tooltips must describe the action and any important side effect.
- Destructive actions must remain text-labeled and visually distinct.
- Icon-only controls are not allowed for v1.1.4 unless a separate design review
  proves they remain unambiguous in Japanese and English.
