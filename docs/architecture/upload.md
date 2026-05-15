# Upload Flow

## Upload Sequence

recorded
-> ready_upload
-> uploading
-> uploaded

---

## Upload Flow Diagram

This diagram shows the Worker-owned upload flow and how upload outcomes map back to queue states.

```mermaid
flowchart TD
    Recorded["Recorded video finalized"] --> Ready["ready_upload"]
    Ready --> Pick["Worker selects next queue item"]
    Pick --> Exists{"Video file exists?"}

    Exists -- No --> Discard["Discard queue item"]
    Exists -- Yes --> Uploading["uploading"]

    Uploading --> Insert["YouTube videos.insert"]
    Insert --> Success{"Upload response success?"}

    Success -- Yes --> Store["Store youtube_video_id and youtube_url"]
    Store --> Uploaded["uploaded"]

    Success -- Network failure --> Failed["upload_failed"]
    Failed --> Retry["Retry later"]
    Retry --> Ready

    Success -- Quota exceeded --> Quota["quota_waiting"]
    Quota --> Reset["Wait for quota reset"]
    Reset --> Ready

    Success -- Ambiguous failure --> Review["need_manual_review"]
    Review --> Manual{"Manual decision"}
    Manual -- Retry --> Ready
    Manual -- Discard --> Discard
```

---

## Upload Success

Upload success is determined by:
- videos.insert response
- youtube_video_id generation

---

## Upload Failure

Network failures:
- retry later

Quota exceeded:
- move to quota_waiting

Missing files:
- discard queue entry
