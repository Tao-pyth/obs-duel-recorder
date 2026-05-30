# User Documentation

Status contract: These pages are roadmap-facing user docs and may include planned, not-yet-released steps. Treat a step as currently available only when the latest release notes or [Roadmap](../roadmap.md) mark its version as released.

Select language / 言語を選択してください。

- 日本語: [docs/user/ja/index.md](ja/index.md)
- English: this page

## Current user docs (English-base, pre-multilingual)

- [Installation](install.md)
- [First setup](setup.md)
- [Automatic recording setup](automatic-recording-setup.md)
- [YouTube OAuth setup](youtube-oauth.md)
- [YouTube upload flow](youtube-upload-flow.md)
- [Operation flow and system overview](operation-flow.md)
- [UI images](ui-images.md)
- [OBS sources used by the Plugin](obs-sources.md)
- [Usage](usage.md)
- [Troubleshooting](troubleshooting.md)

## Localization Policy

- English documents are the canonical source of truth for user docs and design docs.
- Japanese documents under `docs/user/ja/` are translated or expanded from the English source and must not contradict it.
- Japanese pages may split a single English page into multiple guides, but the major sections should remain traceable.

## Published Documentation

GitHub Pages publication is defined by [docs/pages.md](../pages.md). The Pages artifact is generated into `build/docs-site/` and does not include release ZIP assets, runtime data, credentials, screenshots, videos, logs, DBs, local template images, or game assets.
