# Traceability

This page links the canonical roadmap (`docs/roadmap.md`) to the requirements (`docs/requirements/requirements.md`) and version-scoped acceptance checklists.

Goals:
- Reduce duplicate/overlapping issues by making “where is this defined?” easy to answer.
- Keep `docs/roadmap.md` as the canonical feature plan.
- Keep `docs/requirements/requirements.md` as stable, cross-version requirements.
- Use acceptance checklists to define “done” for a specific roadmap minor version.

---

## v0.x

### v0.1 — Repository Foundation

- Roadmap: `docs/roadmap.md` → **v0.1 - Repository Foundation**
- Acceptance checklist: `docs/requirements/v0.1-acceptance.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` → Architecture / Platform / Runtime Rules

### v0.2 — Worker Core API

- Roadmap: `docs/roadmap.md` → **v0.2 - Worker Core API**
- Acceptance checklist: `docs/requirements/v0.2-worker-core-api-acceptance.md`
- Requirements (relevant sections):
  - `docs/requirements/requirements.md` → Architecture / Platform / Runtime Rules
  - `AGENTS.md` → Logging Rules / Runtime Directory Rules / Responsibility Separation

---

## Policy

### When to update `docs/requirements/requirements.md`

Update requirements when the rule is expected to remain true across versions (e.g., responsibility separation, runtime-data persistence, “no assets distributed”, logging location rules).

### When to add or update an acceptance checklist

Add (or extend) an acceptance checklist when you need version-scoped “definition of done” criteria to guide implementation and review for a specific roadmap minor version (e.g., v0.2 Worker Core API).

### When to create a new issue vs update docs

- If a change introduces a new responsibility or deliverable, create a new issue.
- If a change only clarifies an existing rule or acceptance criteria, update the appropriate doc.

---

## Related Issues

- Refs #56
