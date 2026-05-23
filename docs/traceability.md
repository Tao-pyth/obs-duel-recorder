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

### v1.0 - YouTube Upload MVP

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
- Acceptance checklist: not created yet
- Release readiness checklist: not created yet
- Release record: not created yet
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` -> Architecture / Platform / Runtime Rules
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
