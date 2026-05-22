from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .queue import QueueCommandError, QueueItem, QueueStore, QUEUE_STATES


YOUTUBE_UPLOAD_SCOPE = "https://www.googleapis.com/auth/youtube.upload"
DEFAULT_PRIVACY_STATUS = "private"
ALLOWED_PRIVACY_STATUSES = {"private", "unlisted"}
MOCK_RESULTS = {"success", "network_error", "quota_exceeded", "ambiguous_error", "auth_error"}
SECRET_KEYS = {
    "access_token",
    "authorization_code",
    "auth_code",
    "client_secret",
    "code_verifier",
    "credentials",
    "refresh_token",
    "token",
}


class UploadCommandError(ValueError):
    def __init__(self, code: str, details: dict[str, object]):
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class UploadSettings:
    oauth_scope: str
    privacy_status: str
    client_secret_path: Path
    token_path: Path
    client_secret_configured: bool
    token_configured: bool

    def as_payload(self) -> dict[str, object]:
        return {
            "oauth_scope": self.oauth_scope,
            "privacy_status": self.privacy_status,
            "client_secret_path": str(self.client_secret_path),
            "token_path": str(self.token_path),
            "client_secret_configured": self.client_secret_configured,
            "token_configured": self.token_configured,
        }


@dataclass(frozen=True)
class UploadOutcome:
    outcome: str
    youtube_video_id: str = ""
    youtube_url: str = ""
    error_code: str = ""
    error_message: str = ""
    next_attempt_at: str = ""
    evidence: dict[str, object] | None = None


class MockYouTubeUploader:
    """Deterministic local uploader used by v1.0 tests and smoke checks."""

    def upload(self, item: QueueItem, payload: dict[str, Any]) -> UploadOutcome:
        result = _string(payload, "mock_result", "success")
        if result == "success":
            video_id = _string(payload, "youtube_video_id", f"mock-video-{item.id}")
            return UploadOutcome(
                outcome="success",
                youtube_video_id=video_id,
                youtube_url=_youtube_url(video_id, _string(payload, "youtube_url")),
            )
        if result == "network_error":
            return UploadOutcome(
                outcome="network_error",
                error_code=_string(payload, "error_code", "network_timeout"),
                error_message=_string(payload, "error_message", "temporary network failure"),
                next_attempt_at=_string(payload, "next_attempt_at"),
            )
        if result == "quota_exceeded":
            return UploadOutcome(
                outcome="quota_exceeded",
                error_code=_string(payload, "error_code", "quota_exceeded"),
                error_message=_string(payload, "error_message", "quota exceeded"),
                next_attempt_at=_string(payload, "next_attempt_at"),
            )
        if result == "ambiguous_error":
            return UploadOutcome(
                outcome="ambiguous_error",
                error_code=_string(payload, "error_code", "upload_outcome_unknown"),
                error_message=_string(payload, "error_message", "upload outcome is unknown"),
                evidence=redact_upload_diagnostics(payload.get("manual_review_evidence", payload)),
            )
        if result == "auth_error":
            return UploadOutcome(
                outcome="auth_error",
                error_code=_string(payload, "error_code", "upload_auth_required"),
                error_message=_string(payload, "error_message", "YouTube OAuth token is missing or invalid"),
                evidence=redact_upload_diagnostics(payload),
            )
        raise UploadCommandError("upload_payload_invalid", {"mock_result": "unknown_result"})


class UploadStore:
    def __init__(self, *, queue_store: QueueStore, videos_dir: Path, settings: UploadSettings):
        self.queue_store = queue_store
        self.videos_dir = videos_dir
        self.settings = settings
        self.uploader = MockYouTubeUploader()

    def status(self) -> dict[str, object]:
        counts = {state: 0 for state in sorted(QUEUE_STATES)}
        for item in self.queue_store.list_items():
            counts[item.state] = counts.get(item.state, 0) + 1
        return {
            "settings": self.settings.as_payload(),
            "queue_counts": counts,
            "manual_actions": ["retry", "discard", "mark_uploaded"],
        }

    def process_next(self, payload: dict[str, Any] | None = None) -> dict[str, object]:
        payload = payload or {}
        if not isinstance(payload, dict):
            raise UploadCommandError("upload_payload_invalid", {"payload": "must_be_object"})
        mock_result = _string(payload, "mock_result", "success")
        if mock_result not in MOCK_RESULTS:
            raise UploadCommandError("upload_payload_invalid", {"mock_result": "unknown_result"})

        item = self._next_ready_item()
        if item is None:
            return {"processed": False, "reason": "no_ready_upload_items"}

        if not self._video_exists(item.video_path):
            discarded = self.queue_store.apply_command(
                item.id,
                {
                    "action": "discard",
                    "reason": "local_video_missing",
                    "manual_review_evidence": {"video_path": item.video_path},
                },
            )
            return {
                "processed": True,
                "outcome": "discarded_missing_file",
                "item": discarded.as_payload(),
            }

        uploading = self.queue_store.apply_command(item.id, {"action": "start_upload"})
        outcome = self.uploader.upload(uploading, payload)
        final_item = self._apply_outcome(uploading, outcome)
        return {
            "processed": True,
            "outcome": outcome.outcome,
            "item": final_item.as_payload(),
        }

    def _next_ready_item(self) -> QueueItem | None:
        items = self.queue_store.list_items(state="ready_upload")
        return items[0] if items else None

    def _video_exists(self, video_path: str) -> bool:
        if not video_path:
            return False
        path = Path(video_path)
        if not path.is_absolute():
            path = self.videos_dir / path
        return path.exists() and path.is_file()

    def _apply_outcome(self, item: QueueItem, outcome: UploadOutcome) -> QueueItem:
        if outcome.outcome == "success":
            return self.queue_store.apply_command(
                item.id,
                {
                    "action": "mark_uploaded",
                    "youtube_video_id": outcome.youtube_video_id,
                    "youtube_url": outcome.youtube_url,
                },
            )
        if outcome.outcome == "network_error":
            return self.queue_store.apply_command(
                item.id,
                {
                    "action": "mark_upload_failed",
                    "error_code": outcome.error_code,
                    "error_message": outcome.error_message,
                    "next_attempt_at": outcome.next_attempt_at,
                },
            )
        if outcome.outcome == "quota_exceeded":
            return self.queue_store.apply_command(
                item.id,
                {
                    "action": "mark_quota_waiting",
                    "error_code": outcome.error_code,
                    "error_message": outcome.error_message,
                    "next_attempt_at": outcome.next_attempt_at,
                },
            )
        if outcome.outcome in {"ambiguous_error", "auth_error"}:
            return self.queue_store.apply_command(
                item.id,
                {
                    "action": "mark_need_manual_review",
                    "reason": outcome.error_code,
                    "manual_review_evidence": {
                        "outcome": outcome.outcome,
                        "error_message": outcome.error_message,
                        "evidence": outcome.evidence or {},
                    },
                },
            )
        raise UploadCommandError("upload_outcome_invalid", {"outcome": outcome.outcome})


def build_upload_settings(*, user_data_dir: Path, privacy_status: str = DEFAULT_PRIVACY_STATUS) -> UploadSettings:
    if privacy_status not in ALLOWED_PRIVACY_STATUSES:
        raise UploadCommandError(
            "upload_privacy_invalid",
            {"privacy_status": privacy_status, "allowed": sorted(ALLOWED_PRIVACY_STATUSES)},
        )
    secrets_dir = user_data_dir / "config" / "secrets"
    token_path = secrets_dir / "youtube-token.json"
    client_secret_path = secrets_dir / "youtube-client-secret.json"
    return UploadSettings(
        oauth_scope=YOUTUBE_UPLOAD_SCOPE,
        privacy_status=privacy_status,
        client_secret_path=client_secret_path,
        token_path=token_path,
        client_secret_configured=client_secret_path.exists(),
        token_configured=token_path.exists(),
    )


def redact_upload_diagnostics(value: object) -> object:
    if isinstance(value, dict):
        return {
            str(key): "[REDACTED]" if _is_secret_key(str(key)) else redact_upload_diagnostics(raw_value)
            for key, raw_value in value.items()
        }
    if isinstance(value, list):
        return [redact_upload_diagnostics(item) for item in value]
    if isinstance(value, str):
        return _redact_secret_text(value)
    return value


def _is_secret_key(key: str) -> bool:
    normalized = key.lower().replace("-", "_")
    return normalized in SECRET_KEYS or normalized.endswith("_token") or normalized.endswith("_secret")


def _redact_secret_text(value: str) -> str:
    if "Bearer " in value or "authorization_code=" in value:
        return "[REDACTED]"
    return value


def _string(payload: dict[str, Any], key: str, default: str = "") -> str:
    value = payload.get(key, default)
    if value is None:
        return default
    if not isinstance(value, str):
        raise UploadCommandError("upload_payload_invalid", {key: "must_be_string"})
    return value


def _youtube_url(video_id: str, provided_url: str = "") -> str:
    return provided_url or f"https://youtu.be/{video_id}"
