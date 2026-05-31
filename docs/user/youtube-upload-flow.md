# YouTube Upload Flow

Status note: This page describes the v1.1.9 Dock upload experience. Real upload
requires the Worker to be running, a valid YouTube OAuth setup, local video files
that still exist, and packaged Google upload dependencies.

## Normal Flow

1. Record a duel from the Recording tab.
2. Confirm the target video thumbnail and representative frames in the Record
   tab.
3. Fill or confirm Deck, Opponent deck, Result, and memo. Rank and DP are
   editable only when enabled from the Manage tab.
4. Save metadata. The linked queue item remains blocked until required metadata
   is present.
5. Use `Go to Upload`, or open the Upload tab manually.
6. Select the queue item. The Upload tab switches the thumbnail, representative
   frame, match metadata, rendered title/description/tags, and actions to that
   selected item.
7. Confirm the selected video name, match ID, final metadata, readiness message,
   blocking reason, and privacy status.
8. Choose `private` or `unlisted`, then save the privacy setting.
9. Select `Upload`.
10. Confirm the selected queue item, video name, title, and privacy status before
    the upload starts.
11. After success, use `View on YouTube` to open the uploaded video URL.

Beginner note: the Plugin only drives the OBS Dock UI and calls the local Worker
API. The Worker owns the queue, metadata rendering, OAuth files, and YouTube
upload call. If the Worker is not running, the Dock cannot upload.

## Upload Target Checks

The Dock should block upload when any of these conditions are present:

- the queue item is not `ready_upload` or `upload_failed`
- the local video file is missing
- Deck, Opponent deck, or Result is missing
- the title, description, or tags are empty
- the queue item already has a YouTube video ID
- the queue item is already uploaded or discarded

When upload is blocked, check the readiness text and the blocking reason beside
the selected target. Details remain available for support/debugging, but normal
operation should not require reading raw parameters. Do not retry a
manual-review item until you have checked YouTube for a possible partial or
duplicate upload.

## Queue Display

The Upload tab lists active items before history:

- ready upload
- failed or manual review
- quota waiting or uploading
- uploaded/discarded history

Each row includes a state icon, state text, queue item ID, match ID, created
time, and video file name. Full local paths stay out of the primary row so the
active workflow remains readable.

## Recovery Actions

Use `Retry Upload` only when the previous failure is safe to repeat, such as a
network failure. Use `Discard Upload` when the local file is missing or the item
should no longer be uploaded. Use `Mark Uploaded` only after you confirm the
video exists on YouTube and you can enter its YouTube video ID. The Dock previews
the deterministic URL `https://youtu.be/{youtube_video_id}` before it stores the
manual completion.

Quota failures move the item to `quota_waiting`. Wait for the quota reset before
retrying. Authorization failures move the item to manual review; reauthorize or
refresh the token before retrying.

## Safe Reporting

When reporting an upload problem, include the queue item ID, queue state,
readiness state, next action, redacted Worker log lines, and whether the local
video exists. Do not attach OAuth files, tokens, authorization URLs containing
`code=...`, local videos, screenshots, template images, or game assets.
