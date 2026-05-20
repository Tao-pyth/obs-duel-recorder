# Canonical doc inventory

This page defines the **canonical documentation inventory** used by repository validators.

## Inventory rule

Validators should treat these paths as the canonical doc scope:

- repo root `README*.md`
- `docs/**/*.md` (includes `docs/user/**`)

Validators should treat these files as canonical sources for user-doc scope:

- English topic list: `docs/user/index.md`
- Japanese index: `docs/user/ja/index.md`

## Notes

- English documents are the canonical source of truth for both design docs and user docs.
- Japanese user docs under `docs/user/ja/` are translated/expanded from the English source and must not contradict it.
