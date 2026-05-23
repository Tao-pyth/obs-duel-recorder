# Traceability

This page links the canonical roadmap (`docs/roadmap.md`) to requirements, version-scoped acceptance checklists, release tracking issues, and release records.

Goals:
- Reduce duplicate/overlapping issues by making "where is this defined?" easy to answer.
- Keep `docs/roadmap.md` as the canonical feature plan.
- Keep `docs/requirements/requirements.md` as stable, cross-version requirements.
- Use acceptance checklists to define "done" for a specific roadmap minor version.
- Use version tracking issues to coordinate child issues, PRs, release readiness, and next-version handoff.
- Preserve completed release point information in release history docs.

---

## v0.x

### v0.1 - Repository Foundation

- Roadmap: `docs/roadmap.md` -> **v0.1 - Repository Foundation**
- Acceptance checklist: `docs/requirements/v0.1-acceptance.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Architecture / Platform / Runtime Rules

### v0.2 - Worker Core API

- Roadmap: `docs/roadmap.md` -> **v0.2 - Worker Core API**
- Version tracking issue: `#72` - `[v0.2] Worker Core API release tracking`
- Milestone: `v0.2`
- Acceptance checklist: `docs/requirements/v0.2-worker-core-api-acceptance.md`
- Release readiness checklist: `docs/release/v0.2-release-readiness.md`
- Release record: `docs/release-history.md`; use `docs/release/v0.2-release-summary.md` if a detailed release summary is needed.
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Logging Rules / Runtime Directory Rules / Responsibility Separation

### v0.3 - SQLite Foundation

- Roadmap: `docs/roadmap.md` -> **v0.3 - SQLite Foundation**
- Version tracking issue: `#88` - `[v0.3] SQLite Foundation release tracking`
- Acceptance checklist: `docs/requirements/v0.3-sqlite-foundation-acceptance.md`
- Release readiness checklist: `docs/release/v0.3-release-readiness.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.3-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v0.4 - OBS Plugin Skeleton

- Roadmap: `docs/roadmap.md` -> **v0.4 - OBS Plugin Skeleton**
- Version tracking issue: `#110` - `[v0.4] OBS Plugin Skeleton release tracking`
- Acceptance checklist: `docs/requirements/v0.4-obs-plugin-skeleton-acceptance.md`
- Release readiness checklist: `docs/release/v0.4-release-readiness.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.4-release-summary.md`
- Plugin scaffold/build notes: `app/plugin/README.md`
- Smoke evidence gate: `#285`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v0.5 - Overlay Integration

- Roadmap: `docs/roadmap.md` -> **v0.5 - Overlay Integration**
- Version tracking issue: `#288` - `[v0.5] Overlay Integration release tracking`
- Acceptance checklist: `docs/requirements/v0.5-overlay-integration-acceptance.md`
- Release readiness checklist: `docs/release/v0.5-release-readiness.md`
- Overlay architecture contract: `docs/architecture/overlay.md`
- Smoke procedure: `docs/architecture/v0.5-overlay-smoke.md`
- Smoke evidence gate: `#296`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Overlay Rules / Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v0.6 - Recording State Management

- Roadmap: `docs/roadmap.md` -> **v0.6 - Recording State Management**
- Version tracking issue: `#247` - `[v0.6] Recording State Management release tracking`
- Acceptance checklist: `docs/requirements/v0.6-recording-state-management-acceptance.md`
- Release readiness checklist: `docs/release/v0.6-release-readiness.md`
- Recording architecture contract: `docs/architecture/recording.md`
- Smoke procedure: `docs/architecture/v0.6-recording-state-smoke.md`
- Smoke evidence gate: `#310`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.6-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v0.7 - Queue Recovery System

- Roadmap: `docs/roadmap.md` -> **v0.7 - Queue Recovery System**
- Version tracking issue: `#248` - `[v0.7] Queue Recovery System release tracking`
- Acceptance checklist: `docs/requirements/v0.7-queue-recovery-system-acceptance.md`
- Release readiness checklist: `docs/release/v0.7-release-readiness.md`
- Queue architecture contract: `docs/architecture/queue.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.7-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Match Data / Queue / Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v0.8 - Template Matching MVP

- Roadmap: `docs/roadmap.md` -> **v0.8 - Template Matching MVP**
- Version tracking issue: `#249` - `[v0.8] Template Matching MVP release tracking`
- Acceptance checklist: `docs/requirements/v0.8-template-matching-mvp-acceptance.md`
- Release readiness checklist: `docs/release/v0.8-release-readiness.md`
- Detection architecture contract: `docs/architecture/detection.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.8-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Detection Rules / Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v0.9 - Screenshot System

- Roadmap: `docs/roadmap.md` -> **v0.9 - Screenshot System**
- Version tracking issue: `#250` - `[v0.9] Screenshot System release tracking`
- Acceptance checklist: `docs/requirements/v0.9-screenshot-system-acceptance.md`
- Release readiness checklist: `docs/release/v0.9-release-readiness.md`
- Screenshot architecture contract: `docs/architecture/screenshots.md`
- Database contract: `docs/architecture/db.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.9-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Match Data / Queue / Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v0.10 - Roadmap And Versioning Realignment

- Roadmap: `docs/roadmap.md` -> **v0.10 - Roadmap And Versioning Realignment**
- Version tracking issue: `#389` - `[Planning] Reconcile roadmap and versioning with v0.x practical readiness`
- Release policy: `docs/release.md` -> Practical Readiness Policy
- Release record: `docs/release-history.md` -> Practical Readiness Note
- README status: `README.md` -> Current Development / Current Status
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Responsibility Separation

### v0.11 - OBS Plugin Real Load Smoke

- Roadmap: `docs/roadmap.md` -> **v0.11 - OBS Plugin Real Load Smoke**
- Version tracking issue: `#393` - `[v0.11] OBS Plugin Real Load Smoke release tracking`
- Smoke evidence gate: `#387` - `[Verification] OBS Plugin DLL build and real OBS load smoke confirmation`
- Acceptance checklist: `docs/requirements/v0.11-obs-plugin-real-load-smoke-acceptance.md`
- Release readiness checklist: `docs/release/v0.11-release-readiness.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.11-release-summary.md`
- Smoke procedure: `docs/architecture/v0.11-obs-plugin-smoke.md`
- Plugin scaffold/build notes: `app/plugin/README.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Logging Rules / Runtime Directory Rules / Responsibility Separation

### v0.12 - Release Packaging Automation

- Roadmap: `docs/roadmap.md` -> **v0.12 - Release Packaging Automation**
- Version tracking issue: `#396` - `[v0.12] Release Packaging Automation release tracking`
- Acceptance checklist: `docs/requirements/v0.12-release-packaging-automation-acceptance.md`
- Release readiness checklist: `docs/release/v0.12-release-readiness.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v0.12-release-summary.md`
- Packaging architecture contract: `docs/architecture/packaging.md`
- Packaging scripts: `scripts/build_release_package.ps1`, `scripts/validate_release_package.ps1`
- GitHub Actions workflow: `.github/workflows/release-package.yml`
- Release policy: `docs/release.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Packaging Rules / Runtime Rules / Architecture / Platform
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### Practical v1.0 - First Practical OBS Plugin Release

- Roadmap: `docs/roadmap.md` -> **v1.0 - First Practical OBS Plugin Release**
- Release policy: `docs/release.md`
- Previous practical readiness item: `#396` - `[v0.12] Release Packaging Automation release tracking`
- Required evidence:
  - v0.11 OBS plugin real-load smoke accepted
  - v0.12 packaging automation accepted
  - practical install/update verification
  - runtime data preservation verification
  - practical tag naming decision

### Internal v1.0 - YouTube Upload MVP

- Roadmap: `docs/roadmap.md` -> **v1.0 - YouTube Upload MVP**
- Version tracking issue: `#318` - `[v1.0] YouTube Upload MVP release tracking`
- Acceptance checklist: `docs/requirements/v1.0-youtube-upload-mvp-acceptance.md`
- Release readiness checklist: `docs/release/v1.0-release-readiness.md`
- Upload architecture contract: `docs/architecture/upload.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v1.0-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Upload Rules / Queue States / Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v1.1 - Match Metadata

- Roadmap: `docs/roadmap.md` -> **v1.1 - Match Metadata**
- Version tracking issue: `#320` - `[v1.1] Match Metadata release tracking`
- Acceptance checklist: `docs/requirements/v1.1-match-metadata-acceptance.md`
- Release readiness checklist: `docs/release/v1.1-release-readiness.md`
- Metadata architecture contract: `docs/architecture/metadata.md`
- Database contract: `docs/architecture/db.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v1.1-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Match Data / Upload Rules / Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v1.2 - Export System

- Roadmap: `docs/roadmap.md` -> **v1.2 - Export System**
- Version tracking issue: `#321` - `[v1.2] Export System release tracking`
- Acceptance checklist: `docs/requirements/v1.2-export-system-acceptance.md`
- Release readiness checklist: `docs/release/v1.2-release-readiness.md`
- Export architecture contract: `docs/architecture/export.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v1.2-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Export Rules / Match Data / Queue / Screenshot Rules / Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v1.3 - Setup Wizard

- Roadmap: `docs/roadmap.md` -> **v1.3 - Setup Wizard**
- Version tracking issue: `#322` - `[v1.3] Setup Wizard release tracking`
- Acceptance checklist: `docs/requirements/v1.3-setup-wizard-acceptance.md`
- Release readiness checklist: `docs/release/v1.3-release-readiness.md`
- Setup wizard architecture contract: `docs/architecture/setup-wizard.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v1.3-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Setup Rules / Architecture / Platform / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v1.4 - Update System

- Roadmap: `docs/roadmap.md` -> **v1.4 - Update System**
- Version tracking issue: `#323` - `[v1.4] Update System release tracking`
- Acceptance checklist: `docs/requirements/v1.4-update-system-acceptance.md`
- Release readiness checklist: `docs/release/v1.4-release-readiness.md`
- Update system architecture contract: `docs/architecture/update-system.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release-history.md`; detailed summary: `docs/release/v1.4-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Update Rules / Runtime Rules / Architecture / Platform
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

---

## v2.x

### v2.0 - OCR Integration

- Roadmap: `docs/roadmap.md` -> **v2.0 - OCR Integration**
- Version tracking issue: `#324` - `[v2.0] OCR Integration release tracking`
- Acceptance checklist: `docs/requirements/v2.0-ocr-integration-acceptance.md`
- Release readiness checklist: `docs/release/v2.0-release-readiness.md`
- Image recognition architecture contract: `docs/architecture/image-recognition.md`
- Packaging architecture contract: `docs/architecture/packaging.md`
- Upload provider architecture contract: `docs/architecture/upload-provider.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release/v2.0-release-summary.md`
- Scope note: packaging and upload-provider documents record v2.x preparation contracts; v2.0 implementation remains image-recognition-first unless child issues explicitly expand scope.
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Image Recognition Rules / Upload Rules / Packaging Rules / Detection Rules / Match Data / Runtime Rules
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v2.1 - Statistics System

- Roadmap: `docs/roadmap.md` -> **v2.1 - Statistics System**
- Version tracking issue: `#325` - `[v2.1] Statistics System release tracking`
- Acceptance checklist: `docs/requirements/v2.1-statistics-system-acceptance.md`
- Release readiness checklist: `docs/release/v2.1-release-readiness.md`
- Statistics architecture contract: `docs/architecture/statistics.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release/v2.1-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Statistics Rules / Match Data / Upload Rules / Runtime Rules / Architecture / Platform
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v2.2 - GitHub Pages Documentation

- Roadmap: `docs/roadmap.md` -> **v2.2 - GitHub Pages Documentation**
- Version tracking issue: `#326` - `[v2.2] GitHub Pages Documentation release tracking`
- Acceptance checklist: `docs/requirements/v2.2-github-pages-documentation-acceptance.md`
- Release readiness checklist: `docs/release/v2.2-release-readiness.md`
- Documentation validation policy: `docs/validation.md`
- Packaging architecture contract: `docs/architecture/packaging.md`
- Release record: `docs/release/v2.2-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Packaging Rules / Runtime Rules / Architecture / Platform
  - `docs/README.md` -> Documentation Rules / Validation
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

### v2.3 - Runtime Optimization

- Roadmap: `docs/roadmap.md` -> **v2.3 - Runtime Optimization**
- Version tracking issue: `#327` - `[v2.3] Runtime Optimization release tracking`
- Acceptance checklist: `docs/requirements/v2.3-runtime-optimization-acceptance.md`
- Release readiness checklist: `docs/release/v2.3-release-readiness.md`
- Runtime optimization architecture contract: `docs/architecture/runtime-optimization.md`
- Plugin architecture contract: `docs/architecture/plugin-worker.md`
- Worker API contract: `docs/architecture/worker-api.md`
- Release record: `docs/release/v2.3-release-summary.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Update Rules / Runtime Rules / Upload Rules / Queue States / Architecture / Platform
  - `AGENTS.md` -> Runtime Directory Rules / Responsibility Separation

---

## Policy

### Source roles

- `docs/roadmap.md` is the canonical plan for what each roadmap version covers.
- `docs/requirements/requirements.md` stores stable cross-version requirements.
- Version acceptance checklists define the version-specific definition of done.
- Version tracking issues coordinate active child issues, related PRs, readiness, release records, and next-version handoff.
- `docs/release-history.md` and optional `docs/release/vX.Y-release-summary.md` files preserve finalized release point information after release.

### When to update `docs/requirements/requirements.md`

Update requirements when the rule is expected to remain true across versions (e.g., responsibility separation, runtime-data persistence, "no assets distributed", logging location rules).

### When to add or update an acceptance checklist

Add (or extend) an acceptance checklist when you need version-scoped "definition of done" criteria to guide implementation and review for a specific roadmap minor version (e.g., v0.2 Worker Core API).

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
- Refs #288
- Refs #291
- Refs #296
- Refs #247
- Refs #248
- Refs #310
- Refs #249
- Refs #250
- Refs #318
- Refs #320
- Refs #321
- Refs #322
- Refs #323
- Refs #324
- Refs #325
- Refs #326
- Refs #327
