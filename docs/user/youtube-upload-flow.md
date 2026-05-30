# YouTube Upload Flow

Status note: This page describes the v1.1.7 Dock upload experience. Real upload
requires the Worker to be running, a valid YouTube OAuth setup, local video files
that still exist, and packaged Google upload dependencies.

## Normal Flow

1. Record a duel from the Recording tab.
2. Confirm the target video thumbnail and representative frames.
3. Fill or confirm Deck, Opponent deck, Rank, DP, result, and memo.
4. Save metadata.
5. Open the Upload tab.
6. Confirm the upload target, title, description, tags, readiness message, and
   privacy status.
7. Choose `private` or `unlisted`, then save the privacy setting.
8. Select `Upload to YouTube`.
9. Confirm the queue item, video path, title, and privacy status before the
   upload starts.
10. After success, use the shown YouTube URL to open the uploaded video.

Beginner note: the Plugin only drives the OBS Dock UI and calls the local Worker
API. The Worker owns the queue, metadata rendering, OAuth files, and YouTube
upload call. If the Worker is not running, the Dock cannot upload.

## Upload Target Checks

The Dock should block upload when any of these conditions are present:

- the queue item is not `ready_upload` or `upload_failed`
- the local video file is missing
- the title, description, or tags are empty
- the queue item already has a YouTube video ID
- the queue item is already uploaded or discarded

When upload is blocked, check the readiness text and details before retrying.
Do not retry a manual-review item until you have checked YouTube for a possible
partial or duplicate upload.

## Recovery Actions

Use `Retry Upload` only when the previous failure is safe to repeat, such as a
network failure. Use `Discard Upload` when the local file is missing or the item
should no longer be uploaded. Use `Mark Uploaded` only after you confirm the
video exists on YouTube and you can enter its YouTube video ID.

Quota failures move the item to `quota_waiting`. Wait for the quota reset before
retrying. Authorization failures move the item to manual review; reauthorize or
refresh the token before retrying.

## Safe Reporting

When reporting an upload problem, include the queue item ID, queue state,
readiness state, next action, redacted Worker log lines, and whether the local
video exists. Do not attach OAuth files, tokens, authorization URLs containing
`code=...`, local videos, screenshots, template images, or game assets.
