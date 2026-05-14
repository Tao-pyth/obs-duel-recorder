# Application Code

This directory contains application code only.

Runtime data must not be stored here. User runtime data belongs under `user_data/`.

## Areas

- `plugin/`: OBS Plugin integration and Worker lifecycle ownership.
- `worker/`: Python Worker ownership for queue, SQLite, detection, upload, export, and recovery behavior.
- `ui/`: UI assets or frontend surfaces shared by the Plugin or Worker where applicable.

Implementation code is intentionally not added in v0.1 Repository Foundation.
