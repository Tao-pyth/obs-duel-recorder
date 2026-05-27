# Traceability

This page links the canonical roadmap (`docs/roadmap.md`) to requirements, version-scoped acceptance checklists, release tracking issues, and release records.

Goals:
- Reduce duplicate/overlapping issues by making "where is this defined?" easy to answer.
- Keep `docs/roadmap.md` as the canonical feature plan.
- Keep `docs/requirements/requirements.md` as stable, cross-version requirements.
- Use acceptance checklists to define "done" for a specific roadmap minor version.
- Use version tracking issues to coordinate active child issues, related PRs, readiness, release records, and next-version handoff.
- Preserve completed release point information in release history docs.

---

## Active Product Version Sequence

### v0.10 - Roadmap And Versioning Realignment

- Roadmap: `docs/roadmap.md` -> **v0.10 - Roadmap And Versioning Realignment**
- Version tracking issue: `#389`
- Release policy: `docs/release.md` -> Usability-Based Versioning Policy
- Release record: `docs/release-history.md` -> Usability Versioning Note

### v0.11 - OBS Plugin Real Load Smoke

- Roadmap: `docs/roadmap.md` -> **v0.11 - OBS Plugin Real Load Smoke**
- Version tracking issue: `#393` - `[v0.11] OBS Plugin Real Load Smoke release tracking`
- Smoke evidence gate: `#387` - `[Verification] OBS Plugin DLL build and real OBS load smoke confirmation`
- Acceptance checklist: `docs/requirements/v0.11-obs-plugin-real-load-smoke-acceptance.md`
- Release readiness checklist: `docs/release/v0.11-release-readiness.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.11-release-summary.md`
- Smoke procedure: `docs/architecture/v0.11-obs-plugin-smoke.md`

### v0.12 - Release Packaging Automation

- Roadmap: `docs/roadmap.md` -> **v0.12 - Release Packaging Automation**
- Version tracking issue: `#396` - `[v0.12] Release Packaging Automation release tracking`
- Acceptance checklist: `docs/requirements/v0.12-release-packaging-automation-acceptance.md`
- Release readiness checklist: `docs/release/v0.12-release-readiness.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.12-release-summary.md`
- Packaging architecture contract: `docs/architecture/packaging.md`
- Packaging scripts: `scripts/build_release_package.ps1`, `scripts/validate_release_package.ps1`
- GitHub Actions workflow: `.github/workflows/release-package.yml`

### v0.13 - Practical Distribution Readiness

- Roadmap: `docs/roadmap.md` -> **v0.13 - Practical Distribution Readiness**
- Version tracking issue: `#404` - `[v0.13] Practical Distribution Readiness release tracking`
- Acceptance checklist: `docs/requirements/v0.13-practical-distribution-readiness-acceptance.md`
- Release readiness checklist: `docs/release/v0.13-release-readiness.md`
- Download-to-first-run smoke: `docs/release/v0.13-download-to-first-run-smoke.md`
- Packaging architecture contract: `docs/architecture/packaging.md`
- Update system architecture contract: `docs/architecture/update-system.md`
- Plugin Worker launch contract: `docs/architecture/plugin-worker.md`

### v1.0 - First Usable OBS Plugin Release

- Roadmap: `docs/roadmap.md` -> **v1.0 - First Usable OBS Plugin Release**
- Version tracking issue: `#401` - `[v1.0] First Usable OBS Plugin Release tracking`
- Acceptance checklist: `docs/requirements/v1.0-first-usable-release-acceptance.md`
- Release readiness checklist: `docs/release/v1.0-first-usable-release-readiness.md`
- Release summary: `docs/release/v1.0-first-usable-release-summary.md`
- Release policy: `docs/release.md`
- Release record: `docs/release-history.md`
- Previous version gate: `#404` - `[v0.13] Practical Distribution Readiness release tracking`
- Active publication tag: `v1.0.1`
- Legacy conflicting tag: `v1.0.0` - preserved; not part of the active usability-based sequence
- Release URL: `https://github.com/Tao-pyth/obs-duel-recorder/releases/tag/v1.0.1`

### v1.1 - First Trial Usability Hardening

- Roadmap: `docs/roadmap.md` -> **v1.1 - First Trial Usability Hardening**
- Version tracking issue: `#415` - `[v1.1] First Trial Usability Hardening release tracking`
- Acceptance checklist: `docs/requirements/v1.1-first-trial-usability-hardening-acceptance.md`
- Release readiness checklist: `docs/release/v1.1-first-trial-usability-hardening-readiness.md`
- Release policy: `docs/release.md`
- Previous released user-ready version: `v1.0.1`
- Target version: `v1.1.0`
- Legacy conflicting tag: `v1.1.0` - preserved; final publication tag decision required before release
- Installer/MSI follow-up: `#439` - `[future] Plan signed installer/MSI distribution path`
- Required child issues:
  - Installation guidance and safety: `#416`, `#417`
  - Worker launch diagnostics: `#418`, `#419`, `#420`
  - Recording result visibility: `#421`, `#422`
  - First-run setup: `#423`, `#424`
  - Recording handoff: `#425`, `#426`, `#427`
  - Automatic recording: `#428`, `#429`, `#430`
  - YouTube upload: `#431`, `#432`, `#433`
  - Metadata UI: `#434`, `#435`
  - Distribution: `#436`, `#437`
  - Documentation/release records: `#438`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Upload Rules / Match Data / Setup Rules / Packaging Rules / Detection Rules / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v1.1.1 - Usability UI And Documentation Hardening

- Roadmap: `docs/roadmap.md` -> **v1.1.1 - Usability UI And Documentation Hardening**
- Version tracking issue: `#440` - `[v1.1.1] Usability UI and documentation hardening release tracking`
- Acceptance checklist: `docs/requirements/v1.1.1-usability-ui-documentation-hardening-acceptance.md`
- Release readiness checklist: `docs/release/v1.1.1-usability-ui-documentation-hardening-readiness.md`
- Verification matrix: `docs/release/v1.1.1-verification-matrix.md`
- Validation report: `docs/release/v1.1.1-validation-report.md`
- Release summary: `docs/release/v1.1.1-release-summary.md`
- Release policy: `docs/release.md`
- Previous hardening target: `#415` / `v1.1.0` - First Trial Usability Hardening
- Target version: `v1.1.1`
- Completion rule: #440 closes only after all required child issues are complete, release records are durable, and the final publication decision or publication-blocker handoff is recorded.
- Publication follow-up: `#452` - final asset rebuild and publication from this configured terminal
- Required child issues:
  - Planning and verification prerequisites: `#441`, `#442`
  - User documentation and UI imagery: `#443`, `#444`, `#447`
  - Dock UI and settings work: `#445`, `#446`, `#448`
  - Automatic recording setup flow: `#449`
  - Final validation and release: `#450`, `#451`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Setup Rules / Packaging Rules / Detection Rules / Runtime Rules / Upload Rules / Match Data
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v1.1.2 - Recovery Reporting, Distribution, UI, And i18n Update

- Roadmap: `docs/roadmap.md` -> **v1.1.2 - Recovery Reporting, Distribution, UI, And i18n Update**
- Version tracking issue: `#457` - `[v1.1.2] Recovery reporting, distribution fix, Dock compactness, and i18n update tracking`
- Acceptance checklist: `docs/requirements/v1.1.2-recovery-reporting-distribution-ui-i18n-acceptance.md`
- Release readiness checklist: `docs/release/v1.1.2-recovery-reporting-distribution-ui-i18n-readiness.md`
- Release summary: `docs/release/v1.1.2-release-summary.md`
- Automatic error reporting contract: `docs/architecture/automatic-error-reporting.md`
- Packaging architecture contract: `docs/architecture/packaging.md`
- Previous released user-ready version: `v1.1.1`
- Target version: `v1.1.2`
- Required child issues:
  - Automatic error report operation: `#453`
  - Distribution package Worker executable correction: `#454`
  - Dock compact navigation: `#455`
  - Japanese/English language setting and Help note: `#456`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Setup Rules / Packaging Rules / Detection Rules / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation / Documentation Rules

### v1.1.3 - Dock Workflow, Metadata, Upload, And UI State Update

- Roadmap: `docs/roadmap.md` -> **v1.1.3 - Dock Workflow, Metadata, Upload, And UI State Update**
- Version tracking issue: `#470` - `[v1.1.3] Dock workflow, metadata, upload preview, and Material UI state redesign`
- Acceptance checklist: `docs/requirements/v1.1.3-dock-workflow-metadata-upload-ui-acceptance.md`
- Release readiness checklist: `docs/release/v1.1.3-dock-workflow-metadata-upload-ui-readiness.md`
- Release summary: `docs/release/v1.1.3-release-summary.md`
- UI architecture contract: `docs/architecture/dock-workflow-ui-state.md`
- Previous released user-ready version: `v1.1.2`
- Target version: `v1.1.3`
- Required child issues:
  - Recording preview during metadata entry: `#459`
  - Deck/opponent deck dropdown candidates: `#460`
  - UI state design: `#461`
  - Combined management tab and reduced popups: `#462`
  - Metadata popup language support: `#463`
  - Upload text template editing and preview: `#464`
  - Direct Dock metadata editing and carry-over: `#465`
  - Recording -> metadata -> upload flow order: `#466`
  - Automatic setup wizard language switching: `#467`
  - Material Design alignment: `#468`
  - Hide raw/debug parameters behind details controls: `#469`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Setup Rules / Upload Rules / Overlay Rules / Runtime Rules
  - `AGENTS.md` -> Responsibility Separation / Documentation Rules / Recovery Rules

---

## Legacy Implementation Records

The following records preserve earlier implementation planning. They are legacy non-product records unless explicitly listed above in the Active Product Version Sequence.

### v0.x Legacy Foundation Records

- v0.1 Repository Foundation: `docs/requirements/v0.1-acceptance.md`
- v0.2 Worker Core API: `#72`, `docs/requirements/v0.2-worker-core-api-acceptance.md`, `docs/release/v0.2-release-readiness.md`
- v0.3 SQLite Foundation: `#88`, `docs/requirements/v0.3-sqlite-foundation-acceptance.md`, `docs/release/v0.3-release-readiness.md`
- v0.4 OBS Plugin Skeleton: `#110`, `docs/requirements/v0.4-obs-plugin-skeleton-acceptance.md`, `docs/release/v0.4-release-readiness.md`
- v0.5 Overlay Integration: `#288`, `docs/requirements/v0.5-overlay-integration-acceptance.md`, `docs/release/v0.5-release-readiness.md`
- v0.6 Recording State Management: `#247`, `docs/requirements/v0.6-recording-state-management-acceptance.md`, `docs/release/v0.6-release-readiness.md`
- v0.7 Queue Recovery System: `#248`, `docs/requirements/v0.7-queue-recovery-system-acceptance.md`, `docs/release/v0.7-release-readiness.md`
- v0.8 Template Matching MVP: `#249`, `docs/requirements/v0.8-template-matching-mvp-acceptance.md`, `docs/release/v0.8-release-readiness.md`
- v0.9 Screenshot System: `#250`, `docs/requirements/v0.9-screenshot-system-acceptance.md`, `docs/release/v0.9-release-readiness.md`

### Legacy v1 Records

- Legacy record: v1.0 YouTube Upload MVP: `#318`, `docs/requirements/v1.0-youtube-upload-mvp-acceptance.md`, `docs/release/v1.0-release-readiness.md`, `docs/release/v1.0-release-summary.md`
- Legacy record: v1.1 Match Metadata: `#320`, `docs/requirements/v1.1-match-metadata-acceptance.md`, `docs/release/v1.1-release-readiness.md`, `docs/release/v1.1-release-summary.md`
- Legacy record: v1.2 Export System: `#321`, `docs/requirements/v1.2-export-system-acceptance.md`, `docs/release/v1.2-release-readiness.md`, `docs/release/v1.2-release-summary.md`
- Legacy record: v1.3 Setup Wizard: `#322`, `docs/requirements/v1.3-setup-wizard-acceptance.md`, `docs/release/v1.3-release-readiness.md`, `docs/release/v1.3-release-summary.md`
- Legacy record: v1.4 Update System: `#323`, `docs/requirements/v1.4-update-system-acceptance.md`, `docs/release/v1.4-release-readiness.md`, `docs/release/v1.4-release-summary.md`

### Legacy v2 Records

- Legacy record: v2.0 OCR Integration: `#324`, `docs/requirements/v2.0-ocr-integration-acceptance.md`, `docs/release/v2.0-release-readiness.md`, `docs/release/v2.0-release-summary.md`
- Legacy record: v2.1 Statistics System: `#325`, `docs/requirements/v2.1-statistics-system-acceptance.md`, `docs/release/v2.1-release-readiness.md`, `docs/release/v2.1-release-summary.md`
- Legacy record: v2.2 GitHub Pages Documentation: `#326`, `docs/requirements/v2.2-github-pages-documentation-acceptance.md`, `docs/release/v2.2-release-readiness.md`, `docs/release/v2.2-release-summary.md`
- Legacy record: v2.3 Runtime Optimization: `#327`, `docs/requirements/v2.3-runtime-optimization-acceptance.md`, `docs/release/v2.3-release-readiness.md`, `docs/release/v2.3-release-summary.md`

---

## Policy

### Source roles

- `docs/roadmap.md` is the canonical plan for what each roadmap version covers.
- `docs/requirements/requirements.md` stores stable cross-version requirements.
- Version acceptance checklists define the version-specific definition of done.
- Version tracking issues coordinate active child issues, related PRs, readiness, release records, and next-version handoff.
- `docs/release-history.md` and optional `docs/release/vX.Y-release-summary.md` files preserve finalized release point information after release.

### When to update `docs/requirements/requirements.md`

Update requirements when the rule is expected to remain true across versions, such as responsibility separation, runtime-data persistence, no assets distributed, and logging location rules.

### When to add or update an acceptance checklist

Add or extend an acceptance checklist when version-scoped definition-of-done criteria are needed to guide implementation and review for a roadmap minor version.

### When to create a child issue

Create or use a child issue for active implementation, documentation, validation, or release-preparation work that belongs to a version and is not already covered by a non-duplicate issue.

The version tracking issue itself is not an implementation work item.

### When to create a new issue vs update docs

- If a change introduces a new responsibility or deliverable, create a new issue.
- If a change only clarifies an existing rule or acceptance criteria, update the appropriate doc.

---

## Related Issues

- Refs #56
- Refs #72
- Refs #88
- Refs #110
- Refs #247
- Refs #248
- Refs #249
- Refs #250
- Refs #288
- Refs #310
- Refs #318
- Refs #320
- Refs #321
- Refs #322
- Refs #323
- Refs #324
- Refs #325
- Refs #326
- Refs #327
- Refs #389
- Refs #393
- Refs #396
- Refs #401
- Refs #404
- Refs #415
