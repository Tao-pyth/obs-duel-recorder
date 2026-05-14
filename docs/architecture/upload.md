# Upload Flow

## Upload Sequence

recorded
-> ready_upload
-> uploading
-> uploaded

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
