# Release and Tag Policy

This document defines release tracking and release tagging rules for OBS Duel Recorder.

## Version Format

The project uses:

```text
Major.Minor.Patch
```

Git tags use a leading `v`:

```text
vMajor.Minor.Patch
```

Examples:
- `v0.1.0`
- `v0.2.0`
- `v1.0.0`

## Practical Readiness Policy

As of #389, version labels are split into two meanings:

- **Practical release track**: user-facing readiness for installing and running the OBS plugin.
- **Historical/internal milestone track**: implementation milestones that may have tags but do not by themselves prove a user-ready OBS plugin release.

The project is currently **pre-v1.0 on the practical release track**. A practical `v1.0` release requires accepted evidence that:

- the Plugin DLL can be built in Release configuration (completed by #387 / v0.11),
- the DLL loads in a real Windows OBS Studio x64 runtime without crashing (completed by #387 / v0.11),
- the OBS Duel Recorder Dock appears (completed by #387 / v0.11),
- Worker heartbeat and compatibility checks pass (completed by #387 / v0.11),
- basic manual recording start/stop behavior is smoke-tested (completed by #387 / v0.11),
- release ZIP packaging and checksum assets are reproducible (completed by #396 / v0.12),
- packaging/update instructions are usable for a normal user.

Existing historical/internal tags such as `v1.0.0` through `v2.3.0` must not be interpreted as practical user-ready release evidence. Do not move or overwrite those tags. If a future practical `v1.0` tag would conflict with an existing historical/internal tag, record the exact naming decision before tagging.

## Version Terms

Use separate terms for released and in-development versions:

- `released_version`: the latest completed and published version.
- `current_development_version`: the version currently being developed toward the next release boundary.
- `practical_readiness_version`: the user-facing readiness track for installable OBS plugin releases.
- `internal_milestone_version`: the Worker/API or implementation milestone track used for historical development records.

Avoid using a single `current_version` term when it is unclear whether it means a released version or an in-development target.

## Major and Minor Release Tracking

Each major or minor roadmap version should have one version tracking issue.

Tracking issue title format:

```text
[vX.Y] <roadmap scope> release tracking
```

Examples:
- `[v0.2] Worker Core API release tracking`
- `[v0.3] SQLite Foundation release tracking`

The tracking issue is for version management, progress aggregation, release readiness, release records, and handoff to the next version. Implementation work, documentation changes, validation work, and fixes must be handled by child issues or PRs.

Milestones are prepared by maintainers. The version tracking issue and its child issues should be assigned to the same version milestone when that milestone exists.

A parent-specific label is not required. Use existing labels and the milestone to classify work.

## Child Issues

All active work for a version should be represented by child issues unless it is already captured by an existing non-duplicate issue.

Child issues should remain scoped to one responsibility. Do not avoid creating a child issue merely because it is small, as long as it is not a duplicate and it belongs to the version scope.

Work that does not belong to the target version should be excluded or moved to another version. Work that belongs to the target version but will not be completed before release must be explicitly deferred to a follow-up issue or later version tracking issue.

## Patch Releases

Patch versions are not the normal unit for roadmap milestone progress.

Patch tags are optional. They should be created only when a patch release is packaged, published, or used as a recovery/update boundary.

Maintenance, documentation, and automation updates made during ongoing minor-version development are normally absorbed into the next minor release. Do not increment patch versions for every small change.

If a completed release needs a patch boundary, create a small patch tracking issue when useful, for example:

```text
[v0.2.1] Patch release tracking
```

## When Tags Are Required

Tags are required when a major or minor version is completed and merged into `main`.

For practical readiness releases, tags are required only after the practical readiness gate for that version is met. Internal milestone tags may exist, but they must be documented as internal milestones when practical readiness has not been proven.

Required tag points:
- Major release completion: tag the merged release commit as `vX.0.0`, unless the release plan explicitly defines a different minor or patch number.
- Minor release completion: tag the merged release commit as `vX.Y.0`.

Patch tags are optional and are created only when a patch release boundary is intentionally needed.

## Tag Target

Tags should point to the merge commit on `main` that completes the release milestone.

Do not tag an unmerged feature branch as a release.

Do not move or overwrite an existing tag without explicit maintainer approval.

## Release Readiness

Use a version-scoped readiness checklist before tagging:

- v0.2: `docs/release/v0.2-release-readiness.md`
- v0.3: `docs/release/v0.3-release-readiness.md`
- v0.4: `docs/release/v0.4-release-readiness.md`
- v0.11: `docs/release/v0.11-release-readiness.md`
- v0.12: `docs/release/v0.12-release-readiness.md`

The v0.11 readiness checklist is a practical-readiness gate, not a packaged release approval by itself. It was completed together with the OBS real-load smoke evidence tracked by #387 and coordinated through #393.

The v0.12 readiness checklist is a completed practical-readiness gate for release packaging automation, release asset automation, SHA256 checksum publication, and v1.0 handoff documentation.

Do not retroactively create a `v0.1.0` tag unless explicitly approved.

## Release Records

Release point information must be preserved outside the tracking issue before the tracking issue is closed.

Use:
- `docs/release-history.md` as the release history index.
- `docs/release/vX.Y-release-summary.md` when a release needs a detailed summary.

Record at least:
- release version and tag name
- tag target commit SHA
- release date/time
- version tracking issue
- major child issues and PRs
- deferred items and their follow-up links
- next version tracking issue

The tracking issue may contain the latest working summary, but it should not be the only durable release record.

## Agent Responsibilities

When a major or minor roadmap milestone is completed:

1. Confirm all required child issues for the milestone are closed or intentionally deferred.
2. Confirm open PRs for the version are merged or intentionally deferred.
3. Confirm the release readiness checklist is complete.
4. Confirm the release PR is merged into `main`.
5. Identify the merged commit SHA on `main`.
6. Create the release tag if the available GitHub tooling supports tag creation.
7. If tag creation is not available, report the intended tag name and target commit SHA.
8. Save release point information to persistent release docs.
9. Confirm version information such as `practical_readiness_version`, `internal_milestone_version`, `released_version`, and `current_development_version` is updated where defined.
10. Confirm the next version tracking issue is created from `docs/roadmap.md`.
11. Link the next version tracking issue from the completed tracking issue.

Agents must preserve runtime data and must not include secrets, OAuth tokens, logs, databases, generated media, screenshots, or game assets in release work.

## Automation Notes

Automation may create release-preparation issues or PRs when a milestone is nearing completion.

Automation must not merge release PRs.

Automation must not move existing tags.

Automation must not create labels or milestones.

Recurring issue review should treat version tracking issues as coordination records, not as implementation tasks. Implementation and documentation work should happen through child issues or PRs.

Documentation automation should report validation failures to the appropriate validation issue or configured memory/reporting location rather than repeatedly updating the version tracking issue.
