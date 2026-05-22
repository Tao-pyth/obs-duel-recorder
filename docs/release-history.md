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

### v0.3.0 - SQLite Foundation

- Version tracking issue: #88
- Milestone: `v0.3` (not set)
- Release readiness checklist: `docs/release/v0.3-release-readiness.md`
- Tag: `v0.3.0`
- Tag target: `5a31a55b76f5cd2d3e2a64cbf1d4cfed04642dd0`
- Tag finalized at: `2026-05-18T07:55:10+09:00`
- Release summary: `docs/release/v0.3-release-summary.md`
- Major PRs: #101, #104, #107
- Version PRs: #112, #118
- Next version tracking issue: #110
- Status: released

### v0.4.0 - OBS Plugin Skeleton

- Version tracking issue: #110
- Milestone: `v0.4` (not set)
- Release readiness checklist: `docs/release/v0.4-release-readiness.md`
- Tag: `v0.4.0` intended; tag creation tooling was unavailable in this run
- Intended tag target: `629f4f6d8d28d0d9dcca96381340a04077d2bb62`
- Release finalized at: `2026-05-23T00:24:03+09:00`
- Release summary: `docs/release/v0.4-release-summary.md`
- Major child issues: #119, #120, #121, #122, #123, #144
- Smoke evidence gate: #285
- Major PRs: #244, #245, #251, #282, #287
- Validation PRs: #283, #284, #286
- Documentation/design PRs: #128, #131, #132, #134, #136, #138, #143, #146, #150
- Deferred items: Overlay Integration remains in #288; Recording State Management remains v0.6
- Next version tracking issue: #288
- Status: released; tag still needs maintainer/tooling creation at the intended target above

### v0.5.0 - Overlay Integration

- Version tracking issue: #288
- Milestone: `v0.5` (not set)
- Release readiness checklist: `docs/release/v0.5-release-readiness.md`
- Tag: `v0.5.0`
- Tag target: `474c657fa8f3e90cfe12202105b0f69c8c9f7643`
- Release finalized at: `2026-05-23T02:00:52+09:00`
- Tag finalized at: `2026-05-23T02:08:53+09:00`
- Release summary: `docs/release/v0.5-release-summary.md`
- Major child issues: #290, #291, #292, #293, #294, #295, #296
- Smoke evidence gate: #296
- Major PRs: #297, #298, #299, #300, #301, #302
- Release documentation PRs: #304, #305
- Superseded duplicate planning issues: #246, #252, #253, #254, #255, #256, #257
- Deferred items: Recording State Management remains in #303; Queue Recovery System remains v0.7
- Next version tracking issue: #303
- Status: released
