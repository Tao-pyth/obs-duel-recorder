# Dock Workflow And UI State Architecture

`v1.1.3` reorganizes the OBS Dock around the normal operator workflow:

1. Record
2. Metadata
3. Upload
4. Manage

The Dock should keep daily controls visible and move raw diagnostic parameters behind explicit detail controls.

## Responsibilities

- The OBS Plugin owns Dock layout, language switching, user input controls, and Worker lifecycle controls.
- The Python Worker owns SQLite persistence, upload metadata rendering, queue state, and upload execution.
- Upload preview and real upload must use the same Worker-rendered metadata contract.

## Tabs

| Tab | Purpose |
|---|---|
| Record | Manual recording state, recording output identity, and start/stop controls. |
| Metadata | Latest match metadata, editable deck/opponent deck dropdowns, rank/DP carry-over, memo, and save status. |
| Upload | Upload queue actions plus title, description, and tag template editing with preview. |
| Manage | Setup readiness, inline settings, Help, automatic setup entrypoint, and hidden diagnostics details. |

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

Colors must communicate state consistently across Record, Metadata, Upload, and Manage. Decorative color without a state or hierarchy reason should be avoided.
