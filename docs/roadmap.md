# Roadmap

## Versioning Policy

- Major:
  breaking architecture or runtime changes

- Minor:
  planned roadmap features

- Patch:
  bug fixes, runtime fixes, migration fixes, non-roadmap changes

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

# v1 - Upload Phase

Initial usable release phase.

Goals:
- stable uploads
- stable overlays
- stable queue recovery
- stable OBS integration

---

## v1.0 - YouTube Upload MVP

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

## v1.1 - Match Metadata

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

## v1.2 - Export System

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

## v1.3 - Setup Wizard

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

## v1.4 - Update System

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

# v2 - Advanced Runtime Phase

Advanced automation and analysis phase.

---

## v2.0 - OCR Integration

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

## v2.1 - Statistics System

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

## v2.2 - GitHub Pages Documentation

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

## v2.3 - Runtime Optimization

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
