# Version Tracking Issue Template

Use this template for major and minor roadmap version tracking issues.

Tracking issue title format:

```text
[vX.Y] <roadmap scope> release tracking
```

Examples:
- `[v0.2] Worker Core API release tracking`
- `[v0.3] SQLite Foundation release tracking`

```markdown
## Summary
Track vX.Y release progress, child issues, readiness, release record, and handoff to the next version.

This is a version management issue. Implementation, documentation changes, validation work, and fixes must be handled by child issues or PRs.

## Scope
Roadmap section: `docs/roadmap.md` -> `vX.Y - <scope>`

## Non-goals
- Work outside the vX.Y milestone
- Roadmap semantic changes
- Implementation work directly from this tracking issue

## Canonical Sources
- Roadmap: `docs/roadmap.md`
- Requirements: `docs/requirements/requirements.md`
- Acceptance checklist:
- Release readiness checklist:
- Release and tag policy: `docs/release.md`

## Child Issues

### Required Scope
- [ ] #...

### Documentation / Validation Scope
- [ ] #...

### Deferred / Out of Scope
- #...

## Related PRs
- [ ] #...

## Release Readiness
- [ ] Required child issues are closed or explicitly deferred.
- [ ] Open version PRs are reviewed and merged or deferred.
- [ ] Acceptance checklist is reviewed.
- [ ] Release readiness checklist is complete.
- [ ] Release PR is merged into `main`.
- [ ] Tag target commit SHA on `main` is identified.
- [ ] Completion tag is created, or intended tag/SHA is reported if tooling cannot create it.
- [ ] Release point information is saved to persistent release docs.
- [ ] Version information is updated where defined.
- [ ] Next version tracking issue is created from `docs/roadmap.md`.
- [ ] Next version tracking issue is linked here.
- [ ] Deferred work is moved to follow-up issues or later version tracking.

## Release Record
After release, save finalized release point information in persistent docs:

- `docs/release-history.md`
- optionally `docs/release/vX.Y-release-summary.md`

Record at least:
- tag name
- commit SHA
- created date/time
- this tracking issue
- major child issues and PRs
- deferred items

## Automation Boundaries
- Recurring issue review may organize child issues and report status here when explicitly asked.
- Idea discussion may propose or promote child issues, but should not change release gates directly.
- Documentation automation should report validation failures to the appropriate docs validation issue, not repeatedly update this issue.
- 12-hour summaries should be stored in the configured summary or memory location and only linked here when useful.

## Next Version
- Parent issue: TBD
```

## Patch Release Tracking

Patch tracking issues are optional and should be created only when a completed release needs an intentional patch boundary.

Patch tracking title format:

```text
[vX.Y.Z] Patch release tracking
```
