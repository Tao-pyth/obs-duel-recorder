# Documentation validation policy

This page defines what "documentation validation" means for this repository.

## Canonical source and translations

- English documents are the canonical source of truth for both design docs and user docs.
- Japanese user docs under `docs/user/ja/` are translated/expanded from the English source and must not contradict it.

## Validation scope

Validate these paths:

- `README.md`, `README*`
- `docs/**` (including `docs/user/**`)

## Markdown link checks

Validate:

- repository-local link targets (relative paths)
- same-file and cross-file anchors (`#...`) when practical

External URLs (`https://...`) are best-effort and may be skipped when network access is unavailable.

### Recommended check script (PowerShell)

Run the repository's link validator (includes best-effort anchor checking):

```powershell
powershell -ExecutionPolicy Bypass -File docs/tools/validate_markdown_links.ps1
```

### Local-only check (PowerShell, best-effort)

This snippet is intentionally self-contained (no extra tooling). It detects missing local link targets.

```powershell
$root = (Get-Location).Path
$files = @()
$files += Get-ChildItem -File -Filter 'README*.md' -Path $root
$files += Get-ChildItem -File -Recurse -Filter '*.md' -Path (Join-Path $root 'docs')
$files = $files | Sort-Object FullName -Unique

$fail = @()
foreach ($f in $files) {
  $content = Get-Content -Raw -Encoding UTF8 $f.FullName
  $scrub = [regex]::Replace($content, '(?s)```.*?```', '')
  foreach ($m in [regex]::Matches($scrub, '(?<!!)\[[^\]]*\]\(([^)]+)\)')) {
    $target = $m.Groups[1].Value.Trim()
    if ($target -match '^(https?|mailto):') { continue }
    $path = ($target -split '#', 2)[0]
    if ($path -eq '') { continue }
    $resolved = [System.IO.Path]::GetFullPath((Join-Path $f.Directory.FullName $path))
    if (-not (Test-Path $resolved)) {
      $fail += [pscustomobject]@{ file=$f.FullName; link=$target }
    }
  }
}
$fail | Format-Table -AutoSize
if ($fail.Count -gt 0) { exit 1 }
```

## Localization coverage (English -> Japanese)

Validate, at a minimum:

- Japanese pages exist for the user-facing topics described by the English user docs.
- Major sections remain traceable (Japanese may split an English page into multiple guides, but should cover the same core topics).
- "TODO-only" pages are allowed early, but they should still contain the intended section structure and a link back to the Japanese index.

### Automated check (Python)

Run:

```powershell
python scripts/validate_jp_user_docs_coverage.py
```

This check:

- Treats the list under `docs/user/index.md` -> "Current user docs (English-base, pre-multilingual)" as canonical user topics.
- Requires minimum v0.2 Japanese entrypoints for those topics:
  - `docs/user/ja/install.md`
  - `docs/user/ja/first-setup.md`
  - `docs/user/ja/usage.md`
- Emits warnings if `docs/user/ja/index.md` does not link to expected pages.

## Recurring validation

Scheduled documentation validation runs from `.github/workflows/docs-validation.yml`.
It runs weekly and can also be started manually with `workflow_dispatch`.

This section defines the recommended **reporting behavior** for scheduled documentation validation runs.

### Recommended reporting contract

Define one stable "report unit" per validator run:

- `validator_name`: e.g. `markdown-links`, `jp-user-docs-coverage`
- `checked_scope`: e.g. `README* + docs/**`
- `failure_fingerprint`: a stable, deterministic fingerprint of the failure set (examples below)
- `report_destination`: where to post (issue / comment)
- `re_notify_condition`: when to post again for the same fingerprint

Suggested fingerprints:

- Markdown link validation: sorted list of `file + link_target + failure_type`
- JP coverage validation: sorted list of `missing_page + missing_major_section` (if the validator models sections)

### Low-noise reporting rules

- On success: do not post anything (avoid noise).
- On failure:
  - Prefer commenting on an existing "documentation validation tracking" issue if one exists.
  - Otherwise create a new issue titled `Docs validation failed: <validator_name>` and include the full failure list.
- Re-notify only when the `failure_fingerprint` changes, or when the failure persists beyond a human-defined stale window (e.g. weekly reminder).

### Suggested GitHub Actions steps (outline)

This repository already defines the local commands; a scheduled workflow can run them:

```powershell
powershell -ExecutionPolicy Bypass -File docs/tools/validate_markdown_links.ps1
python scripts/validate_jp_user_docs_coverage.py
```
