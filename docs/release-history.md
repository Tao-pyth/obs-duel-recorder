# Release History

This document records completed release outcomes for OBS Duel Recorder.

Use this file as the durable index for release point information. If a release needs more detail, create a version-specific summary under `docs/release/` and link it from this file.

## Recording Policy

For each completed release, record at least:

- version and tag name
- tag target commit SHA
- release date/time
- version tracking issue
- release readiness checklist
- major child issues and PRs
- deferred items and their follow-up links
- next version tracking issue

The version tracking issue may contain the working release summary, but finalized release point information should be copied here before the tracking issue is closed.

## Releases

### v0.2.0 - Worker Core API

- Version tracking issue: #72
- Milestone: `v0.2`
- Release readiness checklist: `docs/release/v0.2-release-readiness.md`
- Tag: `v0.2.0`
- Tag target: `f64c8a5cdeecadfcc4d5406045d9ea4af9e268a0`
- Release summary: `docs/release/v0.2-release-summary.md`
- Major PRs: #84, #87, #89
- Version cleanup PR: #99 (merged after the existing tag target; tag was intentionally kept unchanged)
- Deferred item: #97 (workflow validation automation; post-release because it changes `.github/workflows/**`)
- Next version tracking issue: #88
- Status: released
