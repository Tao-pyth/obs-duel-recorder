# Dock Workflow And UI State Architecture

`v1.1.3` reorganized the OBS Dock around the normal operator workflow:

1. Record
2. Upload
3. Template
4. Manage

The Dock should keep daily controls visible and move raw diagnostic parameters behind explicit detail controls.

`v1.1.4` keeps the same operator order but expands the design target to the
full #473-#504 release scope:

- Dock UI role refinement and icon treatment.
- YouTube/OAuth readiness inside the Upload tab.
- Guided automatic recording setup from current-screen capture.
- Target-video thumbnail/preview while editing metadata.
- Documentation and real OBS validation.

## Responsibilities

- The OBS Plugin owns Dock layout, language switching, user input controls, and Worker lifecycle controls.
- The Python Worker owns SQLite persistence, upload metadata rendering, queue state, and upload execution.
- Upload preview and real upload must use the same Worker-rendered metadata contract.

## Tabs

| Tab | Purpose |
|---|---|
| Record | Recording state, a switching start/stop control, latest match metadata, editable deck/opponent deck dropdowns, rank/DP carry-over, memo, save status, and the target video identity. |
| Upload | Upload queue actions, selected target preview, upload text summary, and YouTube integration summary. |
| Template | Upload title/description/tag template editing and preview. |
| Manage | Setup readiness, inline settings, save settings, automatic setup entrypoint, and hidden diagnostics details. |

## Agreed v1.1.4 Dock Mockup

```text
+------------------------------------------------------+
| OBS Duel Recorder                                    |
+------------------------------------------------------+
| [ Record / 録画 ] [ Upload / アップロード ] [ Template / テンプレート ] [ Manage / 管理 ] |
+------------------------------------------------------+

[ Record / 録画 ]
+------------------------------------------------------+
| Recording status                          [Details]   |
|                                                      |
|  State: idle / recording / stopping                  |
|                                                      |
|  +-------------------+                               |
|  | Start / Stop      |  switching button             |
|  +-------------------+                               |
+------------------------------------------------------+
| Metadata                                  [Details]   |
|                                                      |
|  Target video: 2026-05-28_duel_001.mp4               |
|                                                      |
|  Deck           [ previous value + editable combo ]  |
|  Opponent Deck  [ previous value + editable combo ]  |
|  Rank           [ previous value + editable combo ]  |
|  DP             [ previous value + editable combo ]  |
|                                                      |
|  [ Reload ]                              [ Save ]     |
+------------------------------------------------------+
| Target video content                                  |
|   +--------------------------------------------+     |
|   | thumbnail / representative frame preview   |     |
|   +--------------------------------------------+     |
|                 << 1 2 3 >>                          |
+------------------------------------------------------+

[ Upload / アップロード ]
+------------------------------------------------------+
| Upload queue                              [Details]   |
|  Ready: 2   Manual review: 1   Failed: 0              |
|  Current target: 2026-05-28_duel_001.mp4              |
|  State: ready to upload                               |
|  [ Retry ] [ Mark uploaded ] [ Discard ]              |
+------------------------------------------------------+
| Upload text summary                                  |
|  Title / description / tags are shown for the        |
|  selected upload target.                             |
+------------------------------------------------------+

[ Template / テンプレート ]
+------------------------------------------------------+
| Upload templates                                    |
|  Title                                               |
|  Description                                         |
|  Tags                                                |
|  Preview of the exact metadata sent to YouTube        |
+------------------------------------------------------+
| YouTube readiness                         [Details]   |
|  OAuth: ready                                         |
|  Token: valid                                         |
|  Quota: available                                    |
|  Next action: upload available                        |
|  [ Authorize ] [ Refresh token ] [ Help ]             |
+------------------------------------------------------+

[ Manage / 管理 ]
+------------------------------------------------------+
| Setup                                                |
|  [ Run setup ]                                       |
|  [ Automatic recording setup ]                       |
+------------------------------------------------------+
| Settings                                             |
|  Language          [ 日本語 / English ]               |
|  User data dir     [ ... ]                           |
|  Overlay settings  [ ... ]                           |
|  [ Save settings ]                                   |
+------------------------------------------------------+
| Diagnostics / Details                     [Details]   |
|  Plugin: OK                                          |
|  Worker: OK                                          |
|  DB: OK                                              |
|  Queue: OK                                           |
|  [ Show details ] [ Open logs ]                      |
+------------------------------------------------------+
```

The target-video preview area always represents the current metadata editing
target. It must not silently switch to an arbitrary latest recording or upload
queue item.

At minimum, a valid target video should show one thumbnail. v1.1.4 uses
representative still frames through `GET /matches/{match_id}/video-preview`.
The Worker resolves the `video_path` linked to the current metadata target,
extracts PNG frames with local ffmpeg when available, and returns base64 PNG
content to the Plugin. The Dock shows compact frame navigation with `1 2 3`.

If ffmpeg is unavailable or the linked video is missing, the Worker returns an
unavailable preview reason instead of mutating match or queue state.

## Guided Automatic Recording Setup

The normal automatic recording setup flow should avoid raw template paths and
raw frame text:

1. Show the duel start screen in OBS.
2. Capture it with a `start screen` action.
3. Show the duel end/result screen in OBS.
4. Capture it with an `end screen` action.
5. Test the current screen against the registered templates.
6. Show automatic recording readiness only when both templates and detection
   checks are usable.

The OBS Plugin captures and transports current-screen content. The Worker stores
captured templates under `user_data/`, performs image matching, owns detection
state, and applies threshold/confirmation rules.

v1.1.4 uses OBS frontend screenshots for user-triggered setup and current-screen
tests. This gives users a concrete `現在の画面から取得` workflow without adding
image matching to the Plugin. Continuous automatic frame feed remains a separate
bounded-capture concern: it must not repeatedly create OBS screenshot files.

Captured templates, screenshots, and local media are runtime data. They must not
be committed to git or included in release packages.

## Icon And Tooltip Contract

v1.1.4 uses Qt standard icons for core Dock actions so the Plugin does not need
to package external SVG assets before the Material Design icon set is finalized.
The icon role is semantic rather than decorative:

| Action | Icon role | Tooltip requirement |
|---|---|---|
| Start recording | media play | Explain that it starts manual OBS recording. |
| Stop recording | media stop | Explain that it stops manual OBS recording. |
| Reload metadata | reload | Explain that the latest metadata target is reloaded. |
| Save metadata/settings | save/apply | Explain what is persisted and whether Worker restart may happen. |
| Preview upload text | information/preview | Explain that title, description, and tags are rendered for review. |
| Retry upload | reload | Explain that only the current failed/manual-review item is retried. |
| Discard upload | discard | Explain that the current queue item is discarded. |
| Mark uploaded | apply | Explain that a YouTube video ID is required. |
| Automatic setup capture | computer/capture | Explain that the current OBS Program screen is captured. |
| Details/diagnostics | detailed view | Explain that diagnostics are shown or hidden. |

Future MDI icon adoption remains acceptable, but it should be done as a
separate asset pass after selecting icons and recording the license/source
mapping. The fallback Qt standard icons must remain usable if external icon
assets are absent.

## UI State Contract

Normal surfaces should show a short state label instead of raw debug payloads.

| State | Meaning | Display rule |
|---|---|---|
| `empty` | No current match, queue item, or preview source exists. | Show a short empty message and keep destructive controls disabled. |
| `not_configured` | Worker path or runtime data is incomplete. | Show setup readiness and action text in Manage. |
| `idle` | Worker is ready and no operation is active. | Keep daily controls available. |
| `running` | Worker or recording is active. | Use short labels such as `Recording`. |
| `success` | Save, preview, or queue action completed. | Show a short confirmation near the affected panel. |
| `warning` | Missing metadata, missing output path, or manual review is needed. | Keep editing possible and show a concise warning. |
| `error` | Worker/API rejected a request. | Preserve user input and show the Worker error. |
| `disabled` | The required Worker state is unavailable. | Disable buttons instead of opening failing dialogs. |
| `recovery` | Queue/manual review requires operator decision. | Keep retry/discard/mark-uploaded controls in Upload. |

## Metadata Persistence

The Dock persists deck and opponent deck candidate lists in Plugin settings so dropdown suggestions survive OBS and Worker restarts.

The Dock also remembers the last saved deck, opponent deck, rank, and DP. When a newly completed match has empty fields, those values are offered as defaults but can still be edited manually.

## Upload Metadata Templates

Upload metadata templates are stored with the match record in SQLite:

- `title_template`
- `description_template`
- `tags_template`

The Worker renders these templates for both preview and upload processing. The Google uploader sends title, description, and tags through the YouTube `videos.insert` snippet.

Supported variables include:

- `{match_id}`
- `{deck_name}`
- `{opponent_deck}`
- `{result}`
- `{rank}`
- `{dp}`
- `{started_at}`
- `{ended_at}`
- `{created_at}`
- `{memo}`

Missing variables render as `unknown`. Missing metadata fields produce warnings but do not block editing.

## Material Design Alignment

The Dock uses restrained Material-inspired roles:

| Role | Use |
|---|---|
| primary | Header and major action emphasis. |
| secondary | Non-destructive supporting actions. |
| success | Saved/ready state and mark-uploaded actions. |
| warning | Recording and metadata-needed states. |
| error | Stop/destructive state and Worker failure states. |
| surface | Cards and tab backgrounds. |

The Dock palette is light-green based. Primary actions, surfaces, and borders should stay in the same green family; only destructive/error states may use red. Decorative color without a state or hierarchy reason should be avoided.

## v1.1.8 Usability Adjustment

The real OBS smoke review for #595 split upload text template editing out of
the Upload tab. Upload remains focused on queue selection, target preview, and
YouTube readiness. Template editing and preview live in the Template tab so the
narrow OBS Dock does not compress queue actions and multi-line template fields
into the same viewport.

Frame selectors display `1`, `2`, and `3` without previous/next arrows. They
represent three still frames, not chronological navigation buttons.

The Manage tab uses inline settings plus a single Save Settings action. The
duplicate Open Settings button is removed from the Dock surface because it
opened another copy of the same settings controls.
