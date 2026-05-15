# Queue Design

## Queue Persistence

Queue state must survive:
- OBS restart
- Worker restart
- PC reboot

---

## Queue State Machine

This diagram shows the upload queue lifecycle owned by the Python Worker.

```mermaid
stateDiagram-v2
    [*] --> ready_upload: recording finalized

    ready_upload --> uploading: upload worker starts item
    uploading --> uploaded: videos.insert success
    uploading --> upload_failed: network or API failure
    uploading --> quota_waiting: quota exceeded
    uploading --> need_manual_review: unsafe or ambiguous failure
    uploading --> [*]: missing file discarded

    upload_failed --> ready_upload: retry allowed
    quota_waiting --> ready_upload: quota reset reached
    need_manual_review --> ready_upload: user resolves item
    need_manual_review --> [*]: user discards item
    uploaded --> [*]
```

---

## Recovery Rules

Interrupted recordings are discarded.

Unuploaded items are resumed in order.

---

## Retry Rules

| Failure Type | Action |
|---|---|
| Network failure | Retry |
| Quota exceeded | Wait for quota reset |
| Missing file | Discard |
