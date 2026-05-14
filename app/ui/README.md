# UI

This directory is reserved for UI assets and frontend surfaces that belong to application code.

UI work must preserve the Plugin and Worker responsibility split:
- OBS-specific Dock UI integration belongs with the Plugin.
- Worker APIs and runtime processing remain Worker-owned.
- Runtime data, generated media, logs, tokens, and databases do not belong here.

Implementation code is intentionally not added in v0.1 Repository Foundation.
