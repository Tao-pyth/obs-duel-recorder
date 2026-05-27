# Roadmap

## Versioning Note

This roadmap uses one version sequence based on user-visible usability.

- The project reached its first user-ready OBS Plugin release at `v1.0.1`.
- The current active development target is `v1.1.2`.
- Existing `v1.x` and `v2.x` tags are legacy non-product tags from before this rule.
- The `v0.11` gate completed built OBS Plugin DLL, real OBS load smoke evidence, Dock visibility, Worker heartbeat, and basic recording control evidence.
- The `v0.12` gate completed packaging workflow, release asset automation path, SHA256 checksum generation, and package layout validation.
- The `v0.13` gate completed Worker EXE bundled distribution and local download-to-first-run smoke evidence.
- The user-ready `v1.0` release was published as `v1.0.1` because legacy `v1.0.0` already exists.
- The completed `v1.1.0` issue set was coordinated through #415 and remains the previous hardening scope.
- The `v1.1.1` target followed #415 and completed UI, documentation, verification, packaging validation, and release-record handoff through #440.
- The `v1.1.2` target is coordinated through #457 and covers recovery reporting documentation, release package Worker executable validation, compact Dock navigation, and Japanese/English UI language selection.
- Existing legacy tags must not be moved or overwritten. Because a legacy `v1.1.0` tag exists, v1.1.x publication decisions must be recorded before release.

---

## Versioning Policy

- Major:
  breaking architecture or runtime changes

- Minor:
  planned roadmap features

- Patch:
  bug fixes, runtime fixes, migration fixes, non-roadmap changes

---

# Active Version Roadmap

This is the authoritative roadmap. Version numbers are assigned only when the
corresponding usability gate is satisfied.

---

## v0.10 - Roadmap And Versioning Realignment

### Goals

Reconcile roadmap and versioning with OBS plugin usability.

### Planned

- clarify `v0.x` usability status
- classify existing `v1.x` and `v2.x` tags as legacy non-product tags
- define `v1.0` promotion gates
- connect OBS real-load smoke evidence to the active roadmap

### Deliverables

- corrected README and roadmap wording
- release policy clarification
- release history usability note

---

## v0.11 - OBS Plugin Real Load Smoke

### Goals

Prove that the plugin can be built and loaded in a real OBS runtime.

### Planned

- Release build of `obs-duel-recorder.dll`
- real OBS Studio x64 load smoke
- Dock visibility confirmation
- plugin startup/shutdown log confirmation
- Worker heartbeat and v2.3 internal API compatibility confirmation
- manual recording start/stop smoke
- overlay Text Source creation/reuse smoke

### Deliverables

- version tracking coordinated through #393
- real OBS smoke evidence linked from #387
- confirmed plugin DLL path and build command
- redaction-checked smoke report

---

## v0.12 - Release Packaging Automation

### Goals

Create a reproducible GitHub Actions based release packaging flow.

### Planned

- packaging ZIP workflow
- release asset automation
- SHA256 checksum publication
- release artifact layout validation
- update.bat-compatible package contents

### Deliverables

- GitHub Actions generated release ZIP
- release asset upload procedure
- published checksum evidence
- documented package layout

---

## v0.13 - Practical Distribution Readiness

### Goals

Make the release ZIP practical for normal Windows users without requiring them
to set up Python, pip, or a virtual environment manually.

### Planned

- Worker EXE build and bundling path - complete
- release ZIP layout update for the bundled Worker executable - complete
- `update.bat` bundled-Worker-first behavior - complete
- Plugin Worker launch contract review for bundled Worker execution - complete
- clean Windows + OBS Studio x64 install smoke - complete
- download-to-first-run smoke covering Dock, Worker heartbeat, and basic recording - complete
- user-facing install, checksum, and recovery documentation - complete

### Deliverables

- version tracking coordinated through #404
- Worker EXE package artifact included in release ZIP
- updated packaging/update documentation
- clean install smoke evidence in `docs/release/v0.13-download-to-first-run-smoke.md`
- v1.0 handoff evidence

---

## v1.0 - First Usable OBS Plugin Release

### Goals

Publish the first user-ready release after OBS plugin smoke, packaging, and practical distribution gates pass.

### Planned

- v0.11 real OBS plugin smoke evidence accepted
- v0.12 packaging automation evidence accepted
- v0.13 practical distribution readiness evidence accepted
- release naming/tagging decision recorded without moving legacy tags
- final ZIP/SHA verification completed
- install/update and `user_data/` preservation evidence recorded
- GitHub Release asset publication completed

### Deliverables

- first usable OBS plugin release package
- user-ready release notes
- release publication evidence
- active release tag `v1.0.1`

---

## v1.1 - First Trial Usability Hardening

### Goals

Turn the first real OBS trial findings from `v1.0.1` into a complete normal-user usability hardening release.

Tracking issue: #415

### Planned

- installation guidance and install layout verification
- Worker launch failure diagnostics in the Plugin, Dock, and OBS logs
- recording completion and output evidence in the Dock
- first-run/setup validation UI
- recording-to-match and recording-to-queue handoff
- practical automatic recording setup with template registration, testing, and Start/Stop bridge
- production YouTube upload provider, OAuth setup, and manual-review controls
- match metadata editing UI and upload metadata preview
- install/update assistant for packaged ZIP users and installer/MSI deferral
  decision, with signed installer planning moved to #439
- active v1.1 acceptance, readiness, README, Roadmap, traceability, and release-policy records

### Required Child Issues

- Installation guidance and safety: #416, #417
- Worker launch diagnostics: #418, #419, #420
- Recording result visibility: #421, #422
- First-run setup: #423, #424
- Recording handoff: #425, #426, #427
- Automatic recording: #428, #429, #430
- YouTube upload: #431, #432, #433
- Metadata UI: #434, #435
- Distribution: #436, #437
- Documentation/release records: #438

### Deliverables

- v1.1 normal-user install path with layout verification
- actionable Dock and log diagnostics for Worker launch failures
- visible manual recording completion and output evidence
- GUI path for first-run validation
- durable match and queue handoff for completed recordings
- practical template-based automatic recording workflow
- production YouTube upload path with OAuth and manual review
- graphical metadata editing and upload metadata preview
- ZIP install/update assistant and recorded installer/MSI deferral decision
- active v1.1 release readiness evidence

### Release Note

The target version is `v1.1.0`, but an existing legacy `v1.1.0` tag is preserved for auditability. The final publication tag decision must be recorded before release and must not move, delete, or overwrite the legacy tag.

---

## v1.1.1 - Usability UI And Documentation Hardening

### Goals

Complete the next usability release by turning the post-trial UI, documentation, setup-flow, verification, and release-asset requests into a coherent normal-user release.

Tracking issue: #440

Previous hardening scope: #415 / `v1.1.0`

### Planned

- v1.1.1 roadmap, acceptance, readiness, README, traceability, and release-policy records
- full release verification matrix for Worker, Plugin, package, and documentation checks
- user operation flow and system overview documentation with an overview diagram
- UI images in user documentation for Dock, Settings/theme selection, and Help
- colorful user-focused OBS Dock redesign that keeps diagnostics available without making them dominant
- selectable Dock color themes from Settings
- detailed documentation for OBS sources created or used by the Plugin
- simple Help message accessible from the Dock UI
- smoother automatic recording information registration flow for templates, tests, thresholds, and setup status
- comprehensive validation for the UI and documentation changes
- v1.1.1 release packaging, publication, checksum, and release-record completion

### Required Child Issues

- Planning and verification prerequisites: #441, #442
- User documentation and UI imagery: #443, #444, #447
- Dock UI and settings work: #445, #446, #448
- Automatic recording setup flow: #449
- Final validation and release: #450, #451

### Deliverables

- v1.1.1 acceptance, readiness, README, Roadmap, traceability, and release-policy records
- operation flow and system overview documentation for normal users
- redaction-safe UI images linked from user documentation
- user-focused Dock UI with selectable color themes and built-in Help
- documented OBS source names, behavior, customization expectations, and troubleshooting links
- improved automatic recording setup path that reduces manual JSON/TOML editing
- validation evidence covering Worker tests, Plugin build or smoke expectations, package validation, documentation links, and Japanese user-doc coverage
- release package, SHA256 checksums, release notes, tag decision, and durable release-history record

### Release Note

The completed version is `v1.1.1`. #440 closes after #441 through #451 are complete, release assets or approved publication blockers are recorded, and the final tag/publication decision is documented without moving or overwriting legacy tags. Final public asset publication is handed off to #452 because this workstation cannot rebuild the Plugin DLL.

---

## v1.1.2 - Recovery Reporting, Distribution, UI, And i18n Update

### Goals

Complete the next patch update by correcting the Worker executable packaging
validation gap and improving the post-`v1.1.1` Dock usability path.

Tracking issue: #457

Previous release: `v1.1.1`

### Planned

- define and document automatic error report GitHub Issue operation without
  distributing GitHub tokens
- reject CI fixture or non-PE `odr-worker.exe` files during package build and
  package validation
- reorganize the OBS Dock with tab navigation so Setup, Settings, Help,
  Automatic Setup, and Diagnostics do not all occupy the Dock at once
- add Japanese/English language selection from Settings
- add Help recovery text that explains language changes in the opposite
  language from the current UI selection
- update README, Roadmap, traceability, user docs, release readiness, release
  summary, and release history for the final `v1.1.2` behavior
- build, validate, and publish the `v1.1.2` release package

### Required Child Issues

- #453 - automatic error report GitHub Issue operation specification
- #454 - distribution package includes CI fixture Worker executable
- #455 - compact Dock navigation
- #456 - Japanese/English language setting and Help note

### Deliverables

- automatic error reporting architecture contract
- release package validation that proves the Worker executable is a real PE
  executable
- compact tabbed Dock layout
- persisted `en` / `ja` UI language setting
- cross-language Help recovery note
- updated release and user documentation
- `v1.1.2` release assets, checksum, and release record

### Release Note

#457 closes only after #453 through #456 are complete or explicitly deferred,
documentation matches the final behavior, and the `v1.1.2` release publication
or an approved publication handoff is recorded.

---

# Legacy Implementation Archive

The following records preserve earlier implementation planning. They are not
part of the active product version sequence unless a record is also listed in
the Active Version Roadmap above.

---

# v0 - Prototype Phase

Initial architecture establishment phase.

Goals:
- establish repository structure
- establish Worker architecture
- establish OBS Plugin architecture
- establish SQLite persistence
- establish queue model

---

## v0.1 - Repository Foundation

### Goals

Create the initial repository and documentation structure.

### Planned

- repository scaffold
- README.md
- AGENTS.md
- LICENSE
- docs structure
- .gitignore
- runtime directory structure
- initial roadmap
- architecture documentation

### Deliverables

- public GitHub repository
- documentation-first structure
- initial runtime layout

---

## v0.2 - Worker Core API

### Goals

Create the Python Worker base architecture.

### Planned

- FastAPI Worker
- localhost HTTP API
- Worker startup flow
- health check endpoint
- Worker configuration loading
- logging initialization
- runtime directory creation

### Deliverables

- Worker executable prototype
- GET /health endpoint
- initial logging system

---

## v0.3 - SQLite Foundation

### Goals

Create persistent runtime storage.

### Planned

- SQLite initialization
- schema version management
- migration framework
- matches table
- upload queue table
- runtime state persistence
- queue persistence

### Deliverables

- initial SQLite schema
- migration execution flow
- DB recovery-safe startup

---

## v0.4 - OBS Plugin Skeleton

### Goals

Establish OBS integration layer.

Tracking issue: #110

### Planned

- OBS Plugin scaffold
- OBS Frontend API integration
- Worker process launch
- Worker heartbeat monitoring
- Plugin settings page
- Browser Dock integration
- localhost API wrapper

### Deliverables

- loadable OBS Plugin
- Worker startup from OBS
- Dock UI prototype

---

## v0.5 - Overlay Integration

### Goals

Establish OBS overlay control.

### Planned

- fixed Text Source management
- deck name overlay
- sequence number overlay
- overlay template settings
- overlay update API
- recording state overlay

### Deliverables

- live overlay updates
- OBS Text Source integration

---

## v0.6 - Recording State Management

### Goals

Create recording lifecycle management.

### Planned

- recording state tracking
- manual start button
- manual stop button
- automatic/manual state synchronization
- interrupted recording discard logic
- recording recovery handling

### Deliverables

- stable recording state machine
- manual override support

---

## v0.7 - Queue Recovery System

### Goals

Create recovery-safe queue processing.

### Planned

- queue resume handling
- interrupted upload handling
- upload retry system
- network retry handling
- quota_waiting state
- need_manual_review state
- startup recovery processing

### Deliverables

- recovery-safe queue runtime
- retry-safe upload queue

---

## v0.8 - Template Matching MVP

### Goals

Create automatic duel detection.

### Planned

- template matching engine
- state transition logic
- duel start detection
- duel end detection
- local template loading
- template configuration management

### Deliverables

- automatic recording trigger
- duel lifecycle detection

---

## v0.9 - Screenshot System

### Goals

Create screenshot capture system.

### Planned

- screenshot capture
- screenshot naming rules
- screenshot DB linkage
- upload-related screenshot cleanup
- screenshot preview support

### Deliverables

- screenshot archive system
- screenshot linkage to matches

---

# Legacy v1 Records - Upload Phase

Initial usable release phase.

Goals:
- stable uploads
- stable overlays
- stable queue recovery
- stable OBS integration

---

## Legacy record: v1.0 - YouTube Upload MVP

### Goals

Create automatic YouTube uploads.

### Planned

- YouTube OAuth
- videos.insert upload flow
- upload success detection
- youtube_video_id persistence
- youtube_url persistence
- upload state management
- upload failure handling

### Deliverables

- automatic upload system
- upload recovery support

---

## Legacy record: v1.1 - Match Metadata

### Goals

Support duel metadata management.

### Planned

- opponent deck input
- Match Memo support
- title template engine
- Description metadata output
- Notes section generation
- metadata DB persistence

### Deliverables

- searchable duel metadata
- upload metadata support

---

## Legacy record: v1.2 - Export System

### Goals

Support external archive export.

### Planned

- ZIP export
- SQLite export
- metadata export
- screenshot export
- video linkage export

### Deliverables

- archive backup package
- exportable runtime state

---

## Legacy record: v1.3 - Setup Wizard

### Goals

Improve first-time user onboarding.

### Planned

- first startup setup
- OBS integration setup
- OAuth setup guide
- template setup guide
- runtime path setup

### Deliverables

- guided initial setup

---

## Legacy record: v1.4 - Update System

### Goals

Create safe runtime update support.

### Planned

- update.bat
- version checking
- DB backup
- migration execution
- rollback-safe update flow
- runtime preservation

### Deliverables

- safe update system
- persistent user_data

---

# Legacy v2 Records - Advanced Runtime Phase

Advanced automation and analysis phase.

---

## Legacy record: v2.0 - OCR Integration

### Goals

Add image-recognition-assisted metadata analysis.

Implementation note:
- v2.0 starts with image recognition fixtures and an abstract Worker provider boundary.
- Heavyweight OCR or ML runtime dependencies are not required for v2.0.

### Planned

- result recognition
- rank recognition
- DP recognition
- recognition-assisted verification
- manual correction fallback support

### Deliverables

- image-recognition-assisted metadata extraction

---

## Legacy record: v2.1 - Statistics System

### Goals

Create long-term duel analysis support.

### Planned

- win rate aggregation
- deck statistics
- opponent statistics
- memo search
- upload statistics

### Deliverables

- duel analytics support

---

## Legacy record: v2.2 - GitHub Pages Documentation

### Goals

Create user-facing documentation site.

### Planned

- GitHub Pages support
- docs publication flow
- user setup documentation
- troubleshooting documentation
- FAQ system

### Deliverables

- public documentation site

---

## Legacy record: v2.3 - Runtime Optimization

### Goals

Improve long-term runtime stability.

### Planned

- memory optimization
- upload optimization
- queue optimization
- DB optimization
- recovery optimization

### Deliverables

- long-session runtime stability

---

# Future Considerations

Potential future features:

- advanced OCR
- replay analysis
- match tagging
- statistics dashboard
- stream integration
- cloud sync
- multi-platform support
- AI-assisted tagging
