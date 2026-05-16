# Release and Tag Policy

This document defines release tagging rules for OBS Duel Recorder.

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

## When Tags Are Required

Tags are required when a major or minor version is completed and merged into `main`.

Required tag points:
- Major release completion: tag the merged release commit as `vX.0.0`, unless the release plan explicitly defines a different minor or patch number.
- Minor release completion: tag the merged release commit as `vX.Y.0`.

Patch tags are optional. They should be created when a patch release is packaged, published, or used as a recovery/update boundary.

## Tag Target

Tags should point to the merge commit on `main` that completes the release milestone.

Do not tag an unmerged feature branch as a release.

Do not move or overwrite an existing tag without explicit maintainer approval.

## Release Readiness

Use a version-scoped readiness checklist before tagging:

- v0.2: `docs/release/v0.2-release-readiness.md`

Do not retroactively create a `v0.1.0` tag unless explicitly approved.

## Agent Responsibilities

When a major or minor roadmap milestone is completed:

1. Confirm all required issues for the milestone are closed or intentionally deferred.
2. Confirm the release PR is merged into `main`.
3. Identify the merged commit SHA on `main`.
4. Create the release tag if the available GitHub tooling supports tag creation.
5. If tag creation is not available, report the intended tag name and target commit SHA.

Agents must preserve runtime data and must not include secrets, OAuth tokens, logs, databases, generated media, screenshots, or game assets in release work.

## Automation Notes

Automation may create release-preparation issues or PRs when a milestone is nearing completion.

Automation must not merge release PRs.

Automation must not move existing tags.
