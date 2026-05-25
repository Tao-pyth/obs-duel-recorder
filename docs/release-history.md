# Release History

This document records completed release outcomes for OBS Duel Recorder.

Use this file as the durable index for release point information. If a release needs more detail, create a version-specific summary under `docs/release/` and link it from this file.

## Usability Versioning Note

As of #401, this project uses one active version sequence based on user-visible
usability.

- #387 records completed OBS Plugin DLL build and real OBS load smoke evidence for the v0.11 version gate.
- #396 records completed release packaging automation for the v0.12 version gate.
- #404 records completed Worker EXE bundled distribution and local download-to-first-run smoke for the v0.13 version gate.
- The project reached its first user-ready OBS Plugin release at `v1.0.1`.
- The active v1.0 publication tag is `v1.0.1` because `v1.0.0` already exists as a legacy non-product tag.
- #415 records the completed v1.1.0 post-trial usability hardening issue set.
- #440 tracks the active `v1.1.1` usability UI and documentation hardening release; final release records remain pending #451.
- Existing `v1.x` and `v2.x` tags created before this policy are legacy non-product tags. They are preserved for auditability, but they are not part of the active product version sequence.
- Older `Status: released` entries before this policy note mean "legacy implementation record completed" unless a record explicitly says it is a user-ready product release.

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
- Deferred items: Recording State Management remains in #247; Queue Recovery System remains v0.7
- Next version tracking issue: #247
- Status: released

### v0.6.0 - Recording State Management

- Version tracking issue: #247
- Milestone: `v0.6` (not set)
- Release readiness checklist: `docs/release/v0.6-release-readiness.md`
- Tag: `v0.6.0`
- Tag target: `0654f2b78a75426ab17310c3a58689a72be5c816`
- Release finalized at: `2026-05-23T03:03:08+09:00`
- Tag finalized at: `2026-05-23T03:03:52+09:00`
- Release summary: `docs/release/v0.6-release-summary.md`
- Major child issues: #258, #259, #260, #261, #262, #263, #310
- Smoke evidence gate: #310
- Major PRs: #307, #308, #309
- Superseded duplicate planning issue: #303
- Deferred items: Queue Recovery System remains in #248; Automatic Duel Recording remains v0.8
- Next version tracking issue: #248
- Status: released

### v0.7.0 - Queue Recovery System

- Version tracking issue: #248
- Milestone: `v0.7` (not set)
- Release readiness checklist: `docs/release/v0.7-release-readiness.md`
- Tag: `v0.7.0`
- Tag target: `b0a262ac9500a6ec0ee43f9091ad0fee0e79f0dd`
- Release finalized at: `2026-05-23T03:21:37+09:00`
- Tag finalized at: `2026-05-23T03:22:11+09:00`
- Release summary: `docs/release/v0.7-release-summary.md`
- Major child issues: #237, #264, #265, #266, #267, #268, #269
- Major PRs: #313
- Deferred items: Template Matching MVP remains in #249; Screenshot System remains v0.9
- Next version tracking issue: #249
- Status: released

### v0.8.0 - Template Matching MVP

- Version tracking issue: #249
- Milestone: `v0.8` (not set)
- Release readiness checklist: `docs/release/v0.8-release-readiness.md`
- Tag: `v0.8.0`
- Tag target: `a83fa4ca6d09e169bf8c9b175a1e4863644eb3ec`
- Release finalized at: `2026-05-23T03:46:21+09:00`
- Tag finalized at: `2026-05-23T03:46:55+09:00`
- Release summary: `docs/release/v0.8-release-summary.md`
- Major child issues: #270, #271, #272, #273, #274, #275
- Major PRs: #315
- Deferred items: Screenshot System remains in #250; YouTube Upload MVP remains v1.0
- Next version tracking issue: #250
- Status: released

### v0.9.0 - Screenshot System

- Version tracking issue: #250
- Milestone: `v0.9` (not set)
- Release readiness checklist: `docs/release/v0.9-release-readiness.md`
- Tag: `v0.9.0`
- Tag target: `fd2fca25b632f4aea471cca7044886e689a3faf1`
- Release finalized at: `2026-05-23T04:36:20+09:00`
- Tag finalized at: `2026-05-23T04:35:10+09:00`
- Release summary: `docs/release/v0.9-release-summary.md`
- Major child issues: #276, #277, #278, #279, #280, #281
- Major PRs: #317
- Deferred items: YouTube Upload MVP remains in #318 for v1.0
- Next version tracking issue: #318
- Status: released

### v0.11.0 - OBS Plugin Real Load Smoke

- Version tracking issue: #393
- Smoke evidence gate: #387
- Release readiness checklist: `docs/release/v0.11-release-readiness.md`
- Acceptance checklist: `docs/requirements/v0.11-obs-plugin-real-load-smoke-acceptance.md`
- Smoke procedure: `docs/architecture/v0.11-obs-plugin-smoke.md`
- Tag: `v0.11.0`
- Tag target: `c91a6a3ac2530417871683eff535092d9903fc46`
- Release finalized at: `2026-05-23T18:05:02+09:00`
- Tag finalized at: `2026-05-23T18:18:36+09:00`
- Release summary: `docs/release/v0.11-release-summary.md`
- Major child issues: #387
- Major PRs: #394, #395, #397
- Smoke evidence: portable OBS 32.1.2 load smoke; Dock display; Worker API `2.3` / Worker version `2.3.0`; manual recording start/stop; overlay Text Source reuse/update; plugin startup/shutdown logs
- Deferred items: packaging ZIP workflow, release asset automation, and SHA256 checksum publication remain in #396 for v0.12
- Next version tracking issue: #396
- Status: version gate complete; not a packaged user-ready OBS plugin release

### v0.12.0 - Release Packaging Automation

- Version tracking issue: #396
- Release readiness checklist: `docs/release/v0.12-release-readiness.md`
- Acceptance checklist: `docs/requirements/v0.12-release-packaging-automation-acceptance.md`
- Packaging architecture: `docs/architecture/packaging.md`
- Packaging workflow: `.github/workflows/release-package.yml`
- Packaging scripts: `scripts/build_release_package.ps1`, `scripts/validate_release_package.ps1`
- Tag: `v0.12.0`
- Tag target: `9853d8e5840b7a7a4f9a993761a1a6538e68d28b`
- Release finalized at: `2026-05-23T18:30:01+09:00`
- Tag finalized at: `2026-05-23T18:38:13+09:00`
- Release summary: `docs/release/v0.12-release-summary.md`
- Major child issues: none
- Major PRs: #399
- Packaging evidence: local package build with real `build/plugin/Release/obs-duel-recorder.dll`; CI package builder validation with fixture DLL; ZIP layout validation; external `SHA256SUMS.txt` generation
- Deferred items: install/update verification, runtime data preservation verification, release publication, and tag naming decision remain for #401
- Next version tracking issue: #401
- Status: version gate complete; not a final user-ready OBS plugin release

### v0.13.0 - Practical Distribution Readiness

- Version tracking issue: #404
- Release readiness checklist: `docs/release/v0.13-release-readiness.md`
- Acceptance checklist: `docs/requirements/v0.13-practical-distribution-readiness-acceptance.md`
- Download-to-first-run smoke: `docs/release/v0.13-download-to-first-run-smoke.md`
- Tag: `v0.13.0` intended; tag creation remains with v1.0 publication approval
- Intended tag target: pending final v0.13 merge
- Release finalized at: pending final v0.13 merge
- Major child issues: #405, #406, #407
- Major PRs: #408, #409, #410, #411
- Distribution evidence: Worker EXE build with PyInstaller; release ZIP includes bundled Worker executable; SHA256 checksum verification passed; portable OBS 32.1.2 loaded the packaged Plugin DLL; bundled Worker launched from OBS plugin layout; Dock registered; Worker heartbeat reached API `2.3` / Worker version `2.3.0`; OBS recording start/stop smoke produced an MP4 under the local smoke recording directory
- Deferred items: public GitHub Release publication, final release asset selection, and tag approval remain in #401 for v1.0
- Next version tracking issue: #401
- Status: version gate complete; v1.0 publication is still pending

### v1.0.1 - First Usable OBS Plugin Release

- Version tracking issue: #401
- Acceptance checklist: `docs/requirements/v1.0-first-usable-release-acceptance.md`
- Release readiness checklist: `docs/release/v1.0-first-usable-release-readiness.md`
- Release summary: `docs/release/v1.0-first-usable-release-summary.md`
- Tag: `v1.0.1`
- Tag target: `14cbfe256a20e6263537d28b4d6df0156e2452a3`
- Tag finalized at: `2026-05-24T04:03:31+09:00`
- Release finalized at: `2026-05-24T04:06:42+09:00`
- GitHub Release: `https://github.com/Tao-pyth/obs-duel-recorder/releases/tag/v1.0.1`
- Prior gates: #393 / v0.11, #396 / v0.12, #404 / v0.13
- Assets: `obs-duel-recorder-v1.0.1.zip`, `SHA256SUMS.txt`
- ZIP SHA256: `28CF71B3C68736412B95495CDACEFDA7546EB1A11B4077426D7C9F79159CBBA3`
- Verification: final package validation passed; packaged `update.bat validate --from-version v0.13.0` passed; packaged `update.bat apply --from-version v0.13.0` passed twice; `user_data/` sentinel files were preserved; SQLite backup was created under `user_data/data/db/backups/`
- Legacy conflict: existing `v1.0.0` is preserved and must not be moved, deleted, or overwritten
- Next version tracking issue: #415
- Next-version handoff: #415 tracks `v1.1.0` First Trial Usability Hardening,
  including install diagnostics, recording visibility, first-run setup,
  recording-to-queue handoff, automatic recording setup, production upload,
  metadata UI, ZIP install/update assistance, installer/MSI deferral, and
  release-record synchronization.
- Follow-up release tracking: #440 tracks `v1.1.1` Usability UI and
  Documentation Hardening after the v1.1.0 issue set, including user operation
  flow docs, system overview, UI images, Dock redesign, theme selection, OBS
  source docs, Help UI, automatic recording setup-flow improvements,
  comprehensive validation, release assets, and release records.
- Status: released user-ready OBS Plugin package

### Legacy tag v1.0.0 - YouTube Upload MVP

- Version tracking issue: #318
- Milestone: `v1.0` (not set)
- Release readiness checklist: `docs/release/v1.0-release-readiness.md`
- Tag: `v1.0.0`
- Tag target: `30dfd9b0d1670a4cbbe3bc0f4bf328bcc7bc67d1`
- Release finalized at: `2026-05-23T06:16:38+09:00`
- Tag finalized at: `2026-05-23T06:16:08+09:00`
- Release summary: `docs/release/v1.0-release-summary.md`
- Major child issues: #328, #329, #330, #331
- Major PRs: #364
- Deferred items: Match Metadata remains in #320 for v1.1
- Next version tracking issue: #320
- Status: legacy implementation record; not part of the active usability-based version sequence

### Legacy tag v1.1.0 - Match Metadata

- Version tracking issue: #320
- Milestone: `v1.1` (not set)
- Release readiness checklist: `docs/release/v1.1-release-readiness.md`
- Tag: `v1.1.0`
- Tag target: `afbb6fa73db60afb50a20e1de4e0dccb8b25b65b`
- Release finalized at: `2026-05-23T11:35:46+09:00`
- Tag finalized at: `2026-05-23T11:34:46+09:00`
- Release summary: `docs/release/v1.1-release-summary.md`
- Major child issues: #332, #333, #334, #335
- Major PRs: #366
- Deferred items: Export System remains in #321 for v1.2
- Next version tracking issue: #321
- Status: legacy implementation record; not part of the active usability-based version sequence

### Legacy tag v1.2.0 - Export System

- Version tracking issue: #321
- Milestone: `v1.2` (not set)
- Release readiness checklist: `docs/release/v1.2-release-readiness.md`
- Tag: `v1.2.0`
- Tag target: `1f307e8dba227bed8de3cbd10a089210959f07e5`
- Release finalized at: `2026-05-23T12:10:23+09:00`
- Tag finalized at: `2026-05-23T12:09:43+09:00`
- Release summary: `docs/release/v1.2-release-summary.md`
- Major child issues: #336, #337, #338, #339
- Major PRs: #368
- Deferred items: Setup Wizard remains in #322 for v1.3
- Next version tracking issue: #322
- Status: legacy implementation record; not part of the active usability-based version sequence

### Legacy tag v1.3.0 - Setup Wizard

- Version tracking issue: #322
- Milestone: `v1.3` (not set)
- Release readiness checklist: `docs/release/v1.3-release-readiness.md`
- Tag: `v1.3.0`
- Tag target: `c0579c1f9f3c892059d4bf27dc43e90754453730`
- Release finalized at: `2026-05-23T12:26:36+09:00`
- Tag finalized at: `2026-05-23T12:26:04+09:00`
- Release summary: `docs/release/v1.3-release-summary.md`
- Major child issues: #340, #341, #342, #343
- Major PRs: #370
- Deferred items: Update System remains in #323 for v1.4
- Next version tracking issue: #323
- Status: legacy implementation record; not part of the active usability-based version sequence

### Legacy tag v1.4.0 - Update System

- Version tracking issue: #323
- Milestone: `v1.4` (not set)
- Release readiness checklist: `docs/release/v1.4-release-readiness.md`
- Tag: `v1.4.0`
- Tag target: `333d3ce757b3724f223cec1f9a8b33046df7007b`
- Release finalized at: `2026-05-23T12:48:01+09:00`
- Tag finalized at: `2026-05-23T12:47:19+09:00`
- Release summary: `docs/release/v1.4-release-summary.md`
- Major child issues: #344, #345, #346, #347
- Major PRs: #372
- Deferred items: Advanced Runtime Phase remains in #324 for v2.0
- Next version tracking issue: #324
- Status: legacy implementation record; not part of the active usability-based version sequence

### Legacy tag v2.0.0 - OCR Integration

- Version tracking issue: #324
- Milestone: `v2.0` (not set)
- Release readiness checklist: `docs/release/v2.0-release-readiness.md`
- Tag: `v2.0.0`
- Tag target: `ad16666fcc4d12a0c8adc0d4a71f123e2b346a0a`
- Release finalized at: `2026-05-23T14:19:47+09:00`
- Tag finalized at: `2026-05-23T14:27:00+09:00`
- Release summary: `docs/release/v2.0-release-summary.md`
- Major child issues: #348, #349, #350, #351
- Major PRs: #375
- Deferred items: Statistics System remains in #325 for v2.1; GitHub Pages Documentation remains in #326 for v2.2; Runtime Optimization remains in #327 for v2.3; OBS plugin smoke was completed later by #387 / v0.11; packaging automation was completed later by #396 / v0.12
- Next version tracking issue: #325
- Status: legacy implementation record; not part of the active usability-based version sequence

### Legacy tag v2.1.0 - Statistics System

- Version tracking issue: #325
- Milestone: `v2.1` (not set)
- Release readiness checklist: `docs/release/v2.1-release-readiness.md`
- Tag: `v2.1.0`
- Tag target: `c7ba88b3e4524ac472ebab9f336afbfcdcf3330e`
- Release finalized at: `2026-05-23T14:42:54+09:00`
- Tag finalized at: `2026-05-23T14:51:00+09:00`
- Release summary: `docs/release/v2.1-release-summary.md`
- Major child issues: #352, #353, #354, #355
- Major PRs: #378
- Deferred items: GitHub Pages Documentation remains in #326 for v2.2; Runtime Optimization remains in #327 for v2.3; OBS plugin smoke was completed later by #387 / v0.11; packaging automation was completed later by #396 / v0.12
- Next version tracking issue: #326
- Status: legacy implementation record; not part of the active usability-based version sequence

### Legacy tag v2.2.0 - GitHub Pages Documentation

- Version tracking issue: #326
- Milestone: `v2.2` (not set)
- Release readiness checklist: `docs/release/v2.2-release-readiness.md`
- Tag: `v2.2.0`
- Tag target: `ab8b18ae63c854addfaaca1c40922118360d2907`
- Release finalized at: `2026-05-23T15:20:28+09:00`
- Tag finalized at: `2026-05-23T15:30:14+09:00`
- Release summary: `docs/release/v2.2-release-summary.md`
- Major child issues: #356, #357, #358, #359
- Major PRs: #381
- Deferred items: Runtime Optimization remains in #327 for v2.3; OBS plugin smoke was completed later by #387 / v0.11; packaging automation was completed later by #396 / v0.12
- Next version tracking issue: #327
- Status: legacy implementation record; not part of the active usability-based version sequence

### Legacy tag v2.3.0 - Runtime Optimization

- Version tracking issue: #327
- Milestone: `v2.3` (not set)
- Release readiness checklist: `docs/release/v2.3-release-readiness.md`
- Runtime baseline: `docs/release/v2.3-runtime-baseline.md`
- Tag: `v2.3.0`
- Tag target: `5c6a8ea7f01d364dcdf9f24e656b1a4d262c3142`
- Release finalized at: `2026-05-23T15:45:18+09:00`
- Tag finalized at: `2026-05-23T15:52:12+09:00`
- Release summary: `docs/release/v2.3-release-summary.md`
- Major child issues: #360, #361, #362, #363
- Major PRs: #384
- Deferred items: OBS plugin smoke was completed later by #387 / v0.11; packaging ZIP workflow, release asset automation, and checksum publication were completed later by #396 / v0.12
- Next version tracking issue: none yet
- Status: legacy implementation record; not part of the active usability-based version sequence
