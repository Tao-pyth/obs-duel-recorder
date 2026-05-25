# User Documentation

Status contract: These pages are roadmap-facing user docs and may include planned (not-yet-released) steps. Treat a step as currently available only when the latest release notes or [Roadmap](../roadmap.md) mark its version as released.

Select language / 言語を選択してください。

- 日本語: [`docs/user/ja/index.md`](ja/index.md)
- English (planned): [`docs/user/en/README.md`](en/README.md)

## Current user docs (English-base, pre-multilingual)

These pages remain the current source of truth until `docs/user/en/**` is introduced:

- [Installation](install.md)
- [First setup](setup.md)
- [Operation flow and system overview](operation-flow.md)
- [Usage](usage.md)
- [Troubleshooting](troubleshooting.md)

## Localization policy

- English documents are the canonical source of truth for user docs and design docs.
- Japanese documents under `docs/user/ja/` are translated/expanded from the English source and must not contradict it.
- Japanese pages may split a single English page into multiple guides (e.g. setup), but the **major sections** should remain traceable.

## Published documentation

GitHub Pages publication is defined by [`docs/pages.md`](../pages.md).

The Pages artifact is generated from repository documentation into `build/docs-site/`. It is documentation-only and does not include release ZIP assets, runtime data, credentials, screenshots, videos, logs, DBs, local template images, or game assets.
