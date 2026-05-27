from __future__ import annotations

import json
import datetime as _dt
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .queue import QueueCommandError, QueueItem, QueueStore, QUEUE_STATES


YOUTUBE_UPLOAD_SCOPE = "https://www.googleapis.com/auth/youtube.upload"
DEFAULT_PRIVACY_STATUS = "private"
ALLOWED_PRIVACY_STATUSES = {"private", "unlisted"}
MOCK_RESULTS = {"success", "network_error", "quota_exceeded", "ambiguous_error", "auth_error"}
UPLOAD_PROVIDERS = {"mock", "google"}
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


class GoogleYouTubeUploader:
    """Optional production uploader backed by the official Google client libraries."""

    def __init__(self, settings: UploadSettings):
        self.settings = settings

    def upload(self, item: QueueItem, payload: dict[str, Any]) -> UploadOutcome:
        if not self.settings.client_secret_configured or not self.settings.token_configured:
            return UploadOutcome(
                outcome="auth_error",
                error_code="upload_oauth_missing",
                error_message="YouTube OAuth client secret or token file is missing",
                evidence=_safe_google_evidence(self.settings),
            )

        try:
            from google.oauth2.credentials import Credentials
            from googleapiclient.discovery import build
            from googleapiclient.errors import HttpError
            from googleapiclient.http import MediaFileUpload
        except ImportError as exc:
            return UploadOutcome(
                outcome="auth_error",
                error_code="google_dependencies_missing",
                error_message="Google upload dependencies are not installed",
                evidence={"missing_dependency": str(exc).split("'")[1] if "'" in str(exc) else str(exc)},
            )

        video_path = Path(_string(payload, "resolved_video_path", item.video_path))
        metadata = payload.get("upload_metadata", {})
        if not isinstance(metadata, dict):
            metadata = {}
        privacy_status = _string(payload, "privacy_status", self.settings.privacy_status)
        if privacy_status not in ALLOWED_PRIVACY_STATUSES:
            raise UploadCommandError(
                "upload_privacy_invalid",
                {"privacy_status": privacy_status, "allowed": sorted(ALLOWED_PRIVACY_STATUSES)},
            )

        try:
            credentials = Credentials.from_authorized_user_file(
                str(self.settings.token_path),
                scopes=[self.settings.oauth_scope],
            )
            service = build("youtube", "v3", credentials=credentials)
            request = service.videos().insert(
                part="snippet,status",
                body={
                    "snippet": {
                        "title": _metadata_string(metadata, "title", video_path.stem),
                        "description": _metadata_string(metadata, "description"),
                        "tags": _metadata_string_list(metadata, "tags"),
                    },
                    "status": {"privacyStatus": privacy_status},
                },
                media_body=MediaFileUpload(str(video_path), resumable=True),
            )
            response = _execute_google_upload(request)
        except HttpError as exc:
            status = int(getattr(getattr(exc, "resp", None), "status", 0) or 0)
            content = getattr(exc, "content", b"")
            if isinstance(content, bytes):
                content = content.decode("utf-8", errors="replace")
            return _google_failure_outcome(status=status, content=str(content))
        except OSError as exc:
            return UploadOutcome(
                outcome="network_error",
                error_code="google_upload_io_error",
                error_message=str(exc),
            )
        except Exception as exc:
            return UploadOutcome(
                outcome="ambiguous_error",
                error_code="google_upload_unknown",
                error_message=str(exc),
                evidence=redact_upload_diagnostics({"exception": type(exc).__name__, "message": str(exc)}),
            )

        if not isinstance(response, dict) or not isinstance(response.get("id"), str) or not response["id"]:
            return UploadOutcome(
                outcome="ambiguous_error",
                error_code="google_upload_response_invalid",
                error_message="YouTube upload response did not include a video id",
                evidence=redact_upload_diagnostics(response),
            )

        video_id = response["id"]
        return UploadOutcome(
            outcome="success",
            youtube_video_id=video_id,
            youtube_url=_youtube_url(video_id),
            evidence={"provider": "google"},
        )


class YouTubeOAuthWorkflow:
    def __init__(self, settings: UploadSettings):
        self.settings = settings

    def authorization_url(self, *, redirect_uri: str) -> dict[str, object]:
        if not self.settings.client_secret_configured:
            raise UploadCommandError("oauth_client_secret_missing", {"path": str(self.settings.client_secret_path)})
        try:
            from google_auth_oauthlib.flow import Flow
        except ImportError as exc:
            raise UploadCommandError("oauth_dependencies_missing", {"dependency": _dependency_name(exc)}) from exc

        flow = Flow.from_client_secrets_file(
            str(self.settings.client_secret_path),
            scopes=[self.settings.oauth_scope],
            redirect_uri=redirect_uri,
        )
        auth_url, state = flow.authorization_url(
            access_type="offline",
            include_granted_scopes="true",
            prompt="consent",
        )
        return {
            "authorization_url": auth_url,
            "state": state,
            "redirect_uri": redirect_uri,
            "scope": self.settings.oauth_scope,
            "token_path": str(self.settings.token_path),
        }

    def exchange_code(self, *, code: str, redirect_uri: str) -> dict[str, object]:
        if not code:
            raise UploadCommandError("oauth_payload_invalid", {"code": "required"})
        if not self.settings.client_secret_configured:
            raise UploadCommandError("oauth_client_secret_missing", {"path": str(self.settings.client_secret_path)})
        try:
            from google_auth_oauthlib.flow import Flow
        except ImportError as exc:
            raise UploadCommandError("oauth_dependencies_missing", {"dependency": _dependency_name(exc)}) from exc

        flow = Flow.from_client_secrets_file(
            str(self.settings.client_secret_path),
            scopes=[self.settings.oauth_scope],
            redirect_uri=redirect_uri,
        )
        flow.fetch_token(code=code)
        self._save_credentials(flow.credentials)
        return build_oauth_status(self.settings)

    def refresh_token(self) -> dict[str, object]:
        if not self.settings.token_configured:
            raise UploadCommandError("oauth_token_missing", {"path": str(self.settings.token_path)})
        try:
            from google.auth.transport.requests import Request
            from google.oauth2.credentials import Credentials
        except ImportError as exc:
            raise UploadCommandError("oauth_dependencies_missing", {"dependency": _dependency_name(exc)}) from exc

        try:
            credentials = Credentials.from_authorized_user_file(
                str(self.settings.token_path),
                scopes=[self.settings.oauth_scope],
            )
            if not getattr(credentials, "refresh_token", None):
                raise UploadCommandError("oauth_refresh_token_missing", {"path": str(self.settings.token_path)})
            credentials.refresh(Request())
            self._save_credentials(credentials)
        except UploadCommandError:
            raise
        except Exception as exc:
            raise UploadCommandError(
                "oauth_refresh_failed",
                redact_upload_diagnostics({"error": str(exc), "type": type(exc).__name__}),
            ) from exc
        return build_oauth_status(self.settings)

    def _save_credentials(self, credentials: Any) -> None:
        if not hasattr(credentials, "to_json"):
            raise UploadCommandError("oauth_token_invalid", {"credentials": "missing_to_json"})
        self.settings.token_path.parent.mkdir(parents=True, exist_ok=True)
        tmp_path = self.settings.token_path.with_suffix(".json.tmp")
        tmp_path.write_text(credentials.to_json(), encoding="utf-8")
        tmp_path.replace(self.settings.token_path)


class UploadStore:
    def __init__(self, *, queue_store: QueueStore, videos_dir: Path, settings: UploadSettings, metadata_store: Any = None):
        self.queue_store = queue_store
        self.videos_dir = videos_dir
        self.settings = settings
        self.metadata_store = metadata_store
        self.mock_uploader = MockYouTubeUploader()
        self.google_uploader = GoogleYouTubeUploader(settings)

    def update_settings(self, settings: UploadSettings) -> None:
        self.settings = settings
        self.google_uploader = GoogleYouTubeUploader(settings)

    def status(self) -> dict[str, object]:
        counts = self.queue_store.count_by_state()
        return {
            "settings": self.settings.as_payload(),
            "auth": build_oauth_status(self.settings),
            "queue_counts": counts,
            "providers": {
                "default": "mock",
                "available": sorted(UPLOAD_PROVIDERS),
                "google_optional_dependencies_required": True,
            },
            "manual_actions": ["retry", "discard", "mark_uploaded"],
        }

    def process_next(self, payload: dict[str, Any] | None = None) -> dict[str, object]:
        payload = payload or {}
        if not isinstance(payload, dict):
            raise UploadCommandError("upload_payload_invalid", {"payload": "must_be_object"})
        provider = _string(payload, "provider", "mock")
        if provider not in UPLOAD_PROVIDERS:
            raise UploadCommandError("upload_payload_invalid", {"provider": "unsupported_provider"})
        if provider == "mock" and _string(payload, "mock_result", "success") not in MOCK_RESULTS:
            raise UploadCommandError("upload_payload_invalid", {"mock_result": "unknown_result"})

        item = self._next_ready_item()
        if item is None:
            return {"processed": False, "reason": "no_ready_upload_items"}

        video_path = self._resolve_video_path(item.video_path)
        if not self._video_exists(video_path):
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
        upload_metadata = self._upload_metadata(uploading)
        upload_payload = dict(payload)
        upload_payload["resolved_video_path"] = str(video_path)
        if upload_metadata is not None:
            upload_payload["upload_metadata"] = upload_metadata
        outcome = self._uploader(provider).upload(uploading, upload_payload)
        final_item = self._apply_outcome(uploading, outcome)
        result = {
            "processed": True,
            "outcome": outcome.outcome,
            "item": final_item.as_payload(),
        }
        if upload_metadata is not None:
            result["upload_metadata"] = upload_metadata
        return result

    def _uploader(self, provider: str):
        if provider == "google":
            return self.google_uploader
        return self.mock_uploader

    def _upload_metadata(self, item: QueueItem) -> dict[str, object] | None:
        if self.metadata_store is None or item.match_id is None:
            return None
        return self.metadata_store.render_upload_metadata(item.match_id)

    def _next_ready_item(self) -> QueueItem | None:
        return self.queue_store.next_ready_item()

    def _resolve_video_path(self, video_path: str) -> Path:
        path = Path(video_path)
        if not path.is_absolute():
            path = self.videos_dir / path
        return path

    def _video_exists(self, path: Path) -> bool:
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


def build_oauth_status(settings: UploadSettings) -> dict[str, object]:
    token_details = _token_details(settings.token_path)
    token_state = token_details["state"]
    auth_ready = settings.client_secret_configured and token_state in {"valid", "configured"}
    return {
        "oauth_scope": settings.oauth_scope,
        "client_secret_configured": settings.client_secret_configured,
        "token_configured": settings.token_configured,
        "token_state": token_state,
        "token_expired": token_details["expired"],
        "token_refresh_configured": token_details["refresh_configured"],
        "token_expires_at": token_details["expires_at"],
        "auth_ready": auth_ready,
        "client_secret_path": str(settings.client_secret_path),
        "token_path": str(settings.token_path),
    }


def _token_details(token_path: Path) -> dict[str, object]:
    if not token_path.exists():
        return _token_detail_payload("missing")
    try:
        raw = json.loads(token_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return _token_detail_payload("invalid")
    if not isinstance(raw, dict):
        return _token_detail_payload("invalid")
    expires_at = raw.get("expiry", "")
    refresh_configured = isinstance(raw.get("refresh_token"), str) and bool(raw.get("refresh_token"))
    expired = _is_expired(expires_at) if isinstance(expires_at, str) and expires_at else False
    if expired and refresh_configured:
        state = "expired_refreshable"
    elif expired:
        state = "expired_reauthorization_required"
    else:
        state = "valid" if isinstance(raw.get("token"), str) and raw.get("token") else "configured"
    return _token_detail_payload(
        state,
        expired=expired,
        refresh_configured=refresh_configured,
        expires_at=expires_at if isinstance(expires_at, str) else "",
    )


def _token_detail_payload(
    state: str,
    *,
    expired: bool = False,
    refresh_configured: bool = False,
    expires_at: str = "",
) -> dict[str, object]:
    return {
        "state": state,
        "expired": expired,
        "refresh_configured": refresh_configured,
        "expires_at": expires_at,
    }


def _is_expired(value: str) -> bool:
    normalized = value.replace("Z", "+00:00")
    try:
        expires_at = _dt.datetime.fromisoformat(normalized)
    except ValueError:
        return False
    if expires_at.tzinfo is None:
        expires_at = expires_at.replace(tzinfo=_dt.timezone.utc)
    return expires_at <= _dt.datetime.now(tz=_dt.timezone.utc)


def _safe_google_evidence(settings: UploadSettings) -> dict[str, object]:
    return {
        "client_secret_configured": settings.client_secret_configured,
        "token_configured": settings.token_configured,
        "client_secret_path": str(settings.client_secret_path),
        "token_path": str(settings.token_path),
    }


def _dependency_name(exc: ImportError) -> str:
    text = str(exc)
    return text.split("'")[1] if "'" in text else text


def _metadata_string(metadata: dict[str, object], key: str, default: str = "") -> str:
    value = metadata.get(key, default)
    return value if isinstance(value, str) else default


def _metadata_string_list(metadata: dict[str, object], key: str) -> list[str]:
    value = metadata.get(key, [])
    if not isinstance(value, list):
        return []
    return [item for item in value if isinstance(item, str) and item]


def _google_failure_outcome(*, status: int, content: str) -> UploadOutcome:
    reason, message = _google_error_details(content)
    if reason in {"quotaExceeded", "dailyLimitExceeded", "userRateLimitExceeded"} or status == 429:
        return UploadOutcome(
            outcome="quota_exceeded",
            error_code=reason or "quota_exceeded",
            error_message=message or "YouTube quota exceeded",
            evidence={"http_status": status, "reason": reason},
        )
    if status in {401, 403}:
        return UploadOutcome(
            outcome="auth_error",
            error_code=reason or "google_auth_error",
            error_message=message or "YouTube authorization failed",
            evidence={"http_status": status, "reason": reason},
        )
    if status == 408 or status >= 500:
        return UploadOutcome(
            outcome="network_error",
            error_code=reason or "google_retryable_error",
            error_message=message or "Retryable YouTube upload failure",
        )
    return UploadOutcome(
        outcome="ambiguous_error",
        error_code=reason or "google_upload_ambiguous",
        error_message=message or "YouTube upload outcome is unknown",
        evidence=redact_upload_diagnostics({"http_status": status, "content": content}),
    )


def _execute_google_upload(request: Any) -> object:
    if hasattr(request, "next_chunk"):
        response = None
        while response is None:
            _, response = request.next_chunk()
        return response
    return request.execute()


def _google_error_details(content: str) -> tuple[str, str]:
    try:
        parsed = json.loads(content)
    except json.JSONDecodeError:
        return "", content
    if not isinstance(parsed, dict):
        return "", content
    error = parsed.get("error")
    if not isinstance(error, dict):
        return "", content
    message = error.get("message", "")
    reason = ""
    errors = error.get("errors", [])
    if isinstance(errors, list) and errors and isinstance(errors[0], dict):
        raw_reason = errors[0].get("reason", "")
        if isinstance(raw_reason, str):
            reason = raw_reason
    return reason, message if isinstance(message, str) else ""


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
