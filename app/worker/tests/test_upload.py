from __future__ import annotations

import sys
import tempfile
import types
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class UploadApiTests(unittest.TestCase):
    def _client(self):
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig
        from odr_worker.db import init_db
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        runtime_dirs = ensure_runtime_dirs(user_data_dir=Path(tmp.name))
        db_info = init_db(runtime_dirs=runtime_dirs)
        loaded_config = LoadedWorkerConfig(
            config=WorkerConfig(),
            config_path=runtime_dirs.config_dir / "worker.toml",
            config_loaded=False,
        )
        client = TestClient(create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config, db_info=db_info))
        self.addCleanup(client.close)
        return client, runtime_dirs

    def test_successful_mock_upload_persists_video_id_and_url(self) -> None:
        client, runtime_dirs = self._client()
        self._write_video(runtime_dirs, "duel.mp4")
        item = self._create_queue_item(client, "duel.mp4")

        uploaded = client.post(
            "/upload/process-next",
            json={"mock_result": "success", "youtube_video_id": "abc123"},
        )

        self.assertEqual(uploaded.status_code, 200)
        self.assertTrue(uploaded.json()["processed"])
        self.assertEqual(uploaded.json()["outcome"], "success")
        self.assertEqual(uploaded.json()["item"]["id"], item["id"])
        self.assertEqual(uploaded.json()["item"]["state"], "uploaded")
        self.assertEqual(uploaded.json()["item"]["youtube_video_id"], "abc123")
        self.assertEqual(uploaded.json()["item"]["youtube_url"], "https://youtu.be/abc123")

        status = client.get("/upload/status")
        self.assertEqual(status.status_code, 200)
        self.assertEqual(status.json()["queue_counts"]["uploaded"], 1)
        self.assertIn("google", status.json()["providers"]["available"])

    def test_network_failure_moves_to_retryable_state(self) -> None:
        client, runtime_dirs = self._client()
        self._write_video(runtime_dirs, "network.mp4")
        self._create_queue_item(client, "network.mp4", max_retries=2)

        failed = client.post(
            "/upload/process-next",
            json={
                "mock_result": "network_error",
                "error_code": "network_timeout",
                "next_attempt_at": "2026-05-24T00:00:00Z",
            },
        )

        self.assertEqual(failed.status_code, 200)
        item = failed.json()["item"]
        self.assertEqual(item["state"], "upload_failed")
        self.assertEqual(item["retry_count"], 1)
        self.assertEqual(item["last_error_code"], "network_timeout")
        self.assertEqual(item["next_attempt_at"], "2026-05-24T00:00:00Z")

    def test_quota_exceeded_moves_to_quota_waiting(self) -> None:
        client, runtime_dirs = self._client()
        self._write_video(runtime_dirs, "quota.mp4")
        self._create_queue_item(client, "quota.mp4")

        quota = client.post(
            "/upload/process-next",
            json={"mock_result": "quota_exceeded", "next_attempt_at": "2026-05-25T00:00:00Z"},
        )

        self.assertEqual(quota.status_code, 200)
        item = quota.json()["item"]
        self.assertEqual(item["state"], "quota_waiting")
        self.assertEqual(item["last_error_code"], "quota_exceeded")
        self.assertEqual(item["next_attempt_at"], "2026-05-25T00:00:00Z")

    def test_missing_local_file_is_discarded_with_evidence(self) -> None:
        client, runtime_dirs = self._client()
        missing_path = str(runtime_dirs.videos_dir / "missing.mp4")
        self._create_queue_item(client, missing_path)

        discarded = client.post("/upload/process-next", json={})

        self.assertEqual(discarded.status_code, 200)
        item = discarded.json()["item"]
        self.assertEqual(discarded.json()["outcome"], "discarded_missing_file")
        self.assertEqual(item["state"], "discarded")
        self.assertEqual(item["manual_review_reason"], "local_video_missing")
        self.assertEqual(item["manual_review_evidence"]["video_path"], missing_path)

    def test_ambiguous_upload_moves_to_manual_review_with_redaction(self) -> None:
        client, runtime_dirs = self._client()
        self._write_video(runtime_dirs, "ambiguous.mp4")
        self._create_queue_item(client, "ambiguous.mp4")

        review = client.post(
            "/upload/process-next",
            json={
                "mock_result": "ambiguous_error",
                "manual_review_evidence": {
                    "refresh_token": "secret-refresh-token",
                    "error_code": "connection_reset",
                    "message": "Bearer should-not-leak",
                },
            },
        )

        self.assertEqual(review.status_code, 200)
        item = review.json()["item"]
        self.assertEqual(item["state"], "need_manual_review")
        evidence = item["manual_review_evidence"]["evidence"]
        self.assertEqual(evidence["refresh_token"], "[REDACTED]")
        self.assertEqual(evidence["message"], "[REDACTED]")
        self.assertEqual(evidence["error_code"], "connection_reset")

    def test_no_ready_upload_item_is_stable_noop(self) -> None:
        client, _ = self._client()

        resp = client.post("/upload/process-next", json={})

        self.assertEqual(resp.status_code, 200)
        self.assertFalse(resp.json()["processed"])
        self.assertEqual(resp.json()["reason"], "no_ready_upload_items")

    def test_invalid_mock_result_does_not_start_upload(self) -> None:
        client, runtime_dirs = self._client()
        self._write_video(runtime_dirs, "invalid.mp4")
        item = self._create_queue_item(client, "invalid.mp4")

        resp = client.post("/upload/process-next", json={"mock_result": "invalid"})

        self.assertEqual(resp.status_code, 400)
        self.assertEqual(resp.json()["code"], "upload_payload_invalid")
        queued = client.get(f"/queue/items/{item['id']}")
        self.assertEqual(queued.json()["state"], "ready_upload")

    def test_unsupported_upload_provider_does_not_start_upload(self) -> None:
        client, runtime_dirs = self._client()
        self._write_video(runtime_dirs, "unsupported.mp4")
        item = self._create_queue_item(client, "unsupported.mp4")

        resp = client.post("/upload/process-next", json={"provider": "unknown"})

        self.assertEqual(resp.status_code, 400)
        self.assertEqual(resp.json()["code"], "upload_payload_invalid")
        self.assertEqual(resp.json()["details"]["provider"], "unsupported_provider")
        queued = client.get(f"/queue/items/{item['id']}")
        self.assertEqual(queued.json()["state"], "ready_upload")

    def test_google_provider_missing_oauth_moves_to_manual_review(self) -> None:
        client, runtime_dirs = self._client()
        self._write_video(runtime_dirs, "google.mp4")
        self._create_queue_item(client, "google.mp4")

        review = client.post("/upload/process-next", json={"provider": "google"})

        self.assertEqual(review.status_code, 200)
        self.assertEqual(review.json()["outcome"], "auth_error")
        item = review.json()["item"]
        self.assertEqual(item["state"], "need_manual_review")
        self.assertEqual(item["manual_review_reason"], "upload_oauth_missing")
        self.assertNotIn("access_token", str(item["manual_review_evidence"]))

    def test_google_failure_classification_is_stable(self) -> None:
        from odr_worker.upload import _google_failure_outcome

        quota = _google_failure_outcome(
            status=403,
            content='{"error":{"message":"quota","errors":[{"reason":"quotaExceeded"}]}}',
        )
        auth = _google_failure_outcome(
            status=401,
            content='{"error":{"message":"bad auth","errors":[{"reason":"authError"}]}}',
        )
        retryable = _google_failure_outcome(status=503, content="backend unavailable")

        self.assertEqual(quota.outcome, "quota_exceeded")
        self.assertEqual(quota.error_code, "quotaExceeded")
        self.assertEqual(auth.outcome, "auth_error")
        self.assertEqual(auth.error_code, "authError")
        self.assertEqual(retryable.outcome, "network_error")

    def test_google_uploader_uses_optional_client_for_videos_insert(self) -> None:
        from odr_worker.upload import GoogleYouTubeUploader, build_upload_settings

        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        user_data_dir = Path(tmp.name)
        secrets_dir = user_data_dir / "config" / "secrets"
        secrets_dir.mkdir(parents=True, exist_ok=True)
        (secrets_dir / "youtube-token.json").write_text("{}", encoding="utf-8")
        (secrets_dir / "youtube-client-secret.json").write_text("{}", encoding="utf-8")
        video_path = user_data_dir / "video.mp4"
        video_path.write_bytes(b"fake video")
        calls: dict[str, object] = {}

        class FakeCredentials:
            @staticmethod
            def from_authorized_user_file(path, scopes):
                calls["token_path"] = path
                calls["scopes"] = scopes
                return "credentials"

        class FakeMediaFileUpload:
            def __init__(self, path, resumable):
                calls["media_path"] = path
                calls["resumable"] = resumable

        class FakeRequest:
            def next_chunk(self):
                return None, {"id": "prod123"}

        class FakeVideos:
            def insert(self, **kwargs):
                calls["insert"] = kwargs
                return FakeRequest()

        class FakeService:
            def videos(self):
                return FakeVideos()

        def fake_build(api_name, api_version, credentials):
            calls["build"] = (api_name, api_version, credentials)
            return FakeService()

        modules = {
            "google": types.ModuleType("google"),
            "google.oauth2": types.ModuleType("google.oauth2"),
            "google.oauth2.credentials": types.ModuleType("google.oauth2.credentials"),
            "googleapiclient": types.ModuleType("googleapiclient"),
            "googleapiclient.discovery": types.ModuleType("googleapiclient.discovery"),
            "googleapiclient.errors": types.ModuleType("googleapiclient.errors"),
            "googleapiclient.http": types.ModuleType("googleapiclient.http"),
        }
        modules["google"].__path__ = []
        modules["google.oauth2"].__path__ = []
        modules["googleapiclient"].__path__ = []
        modules["google.oauth2.credentials"].Credentials = FakeCredentials
        modules["googleapiclient.discovery"].build = fake_build
        modules["googleapiclient.errors"].HttpError = RuntimeError
        modules["googleapiclient.http"].MediaFileUpload = FakeMediaFileUpload
        previous = {name: sys.modules.get(name) for name in modules}
        try:
            sys.modules.update(modules)
            outcome = GoogleYouTubeUploader(build_upload_settings(user_data_dir=user_data_dir)).upload(
                self._queue_item(video_path),
                {
                    "resolved_video_path": str(video_path),
                    "upload_metadata": {"title": "Title", "description": "Description"},
                },
            )
        finally:
            for name, value in previous.items():
                if value is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = value

        self.assertEqual(outcome.outcome, "success")
        self.assertEqual(outcome.youtube_video_id, "prod123")
        self.assertEqual(calls["build"], ("youtube", "v3", "credentials"))
        self.assertEqual(calls["insert"]["part"], "snippet,status")
        self.assertEqual(calls["insert"]["body"]["snippet"]["title"], "Title")
        self.assertEqual(calls["media_path"], str(video_path))

    def test_upload_settings_expose_oauth_contract_without_secret_values(self) -> None:
        client, runtime_dirs = self._client()
        secrets_dir = runtime_dirs.config_dir / "secrets"
        secrets_dir.mkdir(parents=True, exist_ok=True)
        (secrets_dir / "youtube-token.json").write_text(
            '{"token":"super-sensitive-token","refresh_token":"super-sensitive-refresh"}',
            encoding="utf-8",
        )
        (secrets_dir / "youtube-client-secret.json").write_text(
            '{"client_secret":"super-sensitive-client-secret"}',
            encoding="utf-8",
        )
        client, _ = self._client_for_runtime(runtime_dirs)

        status = client.get("/upload/status")

        self.assertEqual(status.status_code, 200)
        settings = status.json()["settings"]
        self.assertEqual(settings["oauth_scope"], "https://www.googleapis.com/auth/youtube.upload")
        self.assertEqual(settings["privacy_status"], "private")
        self.assertTrue(settings["client_secret_configured"])
        self.assertTrue(settings["token_configured"])
        self.assertTrue(status.json()["auth"]["auth_ready"])
        self.assertEqual(status.json()["auth"]["token_state"], "valid")
        self.assertNotIn("super-sensitive-token", str(settings))
        self.assertNotIn("super-sensitive-refresh", str(status.json()["auth"]))
        self.assertNotIn("super-sensitive-client-secret", str(settings))

    def test_oauth_status_reports_expired_refreshable_token_without_secret_values(self) -> None:
        client, runtime_dirs = self._client()
        secrets_dir = runtime_dirs.config_dir / "secrets"
        secrets_dir.mkdir(parents=True, exist_ok=True)
        (secrets_dir / "youtube-client-secret.json").write_text('{"client_secret":"secret"}', encoding="utf-8")
        (secrets_dir / "youtube-token.json").write_text(
            '{"token":"old-token","refresh_token":"refresh-secret","expiry":"2000-01-01T00:00:00Z"}',
            encoding="utf-8",
        )
        client, _ = self._client_for_runtime(runtime_dirs)

        status = client.get("/upload/status")

        self.assertEqual(status.status_code, 200)
        self.assertEqual(status.json()["auth"]["token_state"], "expired_refreshable")
        self.assertTrue(status.json()["auth"]["token_expired"])
        self.assertTrue(status.json()["auth"]["token_refresh_configured"])
        self.assertNotIn("old-token", str(status.json()))
        self.assertNotIn("refresh-secret", str(status.json()))

    def test_oauth_authorization_and_code_exchange_store_token_under_secrets(self) -> None:
        client, runtime_dirs = self._client()
        secrets_dir = runtime_dirs.config_dir / "secrets"
        secrets_dir.mkdir(parents=True, exist_ok=True)
        (secrets_dir / "youtube-client-secret.json").write_text('{"client_secret":"secret"}', encoding="utf-8")
        calls: dict[str, object] = {}

        class FakeCredentials:
            def to_json(self):
                return '{"token":"stored-token","refresh_token":"stored-refresh","expiry":"2999-01-01T00:00:00Z"}'

        class FakeFlow:
            credentials = FakeCredentials()

            @classmethod
            def from_client_secrets_file(cls, path, scopes, redirect_uri):
                calls["flow"] = (path, scopes, redirect_uri)
                return cls()

            def authorization_url(self, **kwargs):
                calls["authorization_kwargs"] = kwargs
                return "http://auth.local/authorize", "state123"

            def fetch_token(self, code):
                calls["code"] = code

        modules = {
            "google_auth_oauthlib": types.ModuleType("google_auth_oauthlib"),
            "google_auth_oauthlib.flow": types.ModuleType("google_auth_oauthlib.flow"),
        }
        modules["google_auth_oauthlib"].__path__ = []
        modules["google_auth_oauthlib.flow"].Flow = FakeFlow
        previous = {name: sys.modules.get(name) for name in modules}
        try:
            sys.modules.update(modules)
            auth = client.post(
                "/upload/oauth/authorization-url",
                json={"redirect_uri": "http://127.0.0.1:8787/upload/oauth/callback"},
            )
            exchanged = client.post(
                "/upload/oauth/exchange-code",
                json={"code": "code-secret", "redirect_uri": "http://127.0.0.1:8787/upload/oauth/callback"},
            )
            status = client.get("/upload/status")
        finally:
            for name, value in previous.items():
                if value is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = value

        self.assertEqual(auth.status_code, 200)
        self.assertEqual(auth.json()["authorization_url"], "http://auth.local/authorize")
        self.assertEqual(auth.json()["state"], "state123")
        self.assertEqual(exchanged.status_code, 200)
        self.assertTrue(exchanged.json()["auth_ready"])
        self.assertTrue(status.json()["auth"]["auth_ready"])
        self.assertEqual(exchanged.json()["token_state"], "valid")
        self.assertEqual(calls["code"], "code-secret")
        self.assertTrue((secrets_dir / "youtube-token.json").exists())
        self.assertNotIn("stored-token", str(exchanged.json()))
        self.assertNotIn("stored-refresh", str(exchanged.json()))

    def test_oauth_refresh_updates_token_with_fake_credentials(self) -> None:
        client, runtime_dirs = self._client()
        secrets_dir = runtime_dirs.config_dir / "secrets"
        secrets_dir.mkdir(parents=True, exist_ok=True)
        (secrets_dir / "youtube-client-secret.json").write_text('{"client_secret":"secret"}', encoding="utf-8")
        (secrets_dir / "youtube-token.json").write_text(
            '{"token":"old-token","refresh_token":"refresh-secret","expiry":"2000-01-01T00:00:00Z"}',
            encoding="utf-8",
        )
        calls: dict[str, object] = {}

        class FakeRequest:
            pass

        class FakeCredentials:
            refresh_token = "refresh-secret"

            @staticmethod
            def from_authorized_user_file(path, scopes):
                calls["token_path"] = path
                calls["scopes"] = scopes
                return FakeCredentials()

            def refresh(self, request):
                calls["refresh_request"] = type(request).__name__

            def to_json(self):
                return '{"token":"new-token","refresh_token":"refresh-secret","expiry":"2999-01-01T00:00:00Z"}'

        modules = {
            "google": types.ModuleType("google"),
            "google.auth": types.ModuleType("google.auth"),
            "google.auth.transport": types.ModuleType("google.auth.transport"),
            "google.auth.transport.requests": types.ModuleType("google.auth.transport.requests"),
            "google.oauth2": types.ModuleType("google.oauth2"),
            "google.oauth2.credentials": types.ModuleType("google.oauth2.credentials"),
        }
        for package in ("google", "google.auth", "google.auth.transport", "google.oauth2"):
            modules[package].__path__ = []
        modules["google.auth.transport.requests"].Request = FakeRequest
        modules["google.oauth2.credentials"].Credentials = FakeCredentials
        previous = {name: sys.modules.get(name) for name in modules}
        try:
            sys.modules.update(modules)
            refreshed = client.post("/upload/oauth/refresh")
            status = client.get("/upload/status")
        finally:
            for name, value in previous.items():
                if value is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = value

        self.assertEqual(refreshed.status_code, 200)
        self.assertTrue(refreshed.json()["auth_ready"])
        self.assertTrue(status.json()["auth"]["auth_ready"])
        self.assertEqual(calls["refresh_request"], "FakeRequest")
        self.assertNotIn("new-token", str(refreshed.json()))
        self.assertIn("new-token", (secrets_dir / "youtube-token.json").read_text(encoding="utf-8"))

    def test_redaction_helper_redacts_nested_secret_values(self) -> None:
        from odr_worker.upload import redact_upload_diagnostics

        redacted = redact_upload_diagnostics(
            {
                "access_token": "secret",
                "error_code": "quota_exceeded",
                "nested": {"client_secret": "secret", "message": "Bearer abc"},
            }
        )

        self.assertEqual(redacted["access_token"], "[REDACTED]")
        self.assertEqual(redacted["error_code"], "quota_exceeded")
        self.assertEqual(redacted["nested"]["client_secret"], "[REDACTED]")
        self.assertEqual(redacted["nested"]["message"], "[REDACTED]")

    def _create_queue_item(self, client, video_path: str, *, max_retries: int = 3) -> dict[str, object]:
        resp = client.post("/queue/items", json={"video_path": video_path, "max_retries": max_retries})
        self.assertEqual(resp.status_code, 200)
        return resp.json()

    def _write_video(self, runtime_dirs, name: str) -> None:
        (runtime_dirs.videos_dir / name).write_bytes(b"fake video")

    def _queue_item(self, video_path: Path):
        from odr_worker.queue import QueueItem

        return QueueItem(
            id=1,
            match_id=None,
            state="uploading",
            video_path=str(video_path),
            youtube_video_id="",
            youtube_url="",
            retry_count=0,
            max_retries=3,
            next_attempt_at="",
            last_error_code="",
            last_error_message="",
            manual_review_reason="",
            manual_review_evidence={},
            created_at="",
            updated_at="",
        )

    def _client_for_runtime(self, runtime_dirs):
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig
        from odr_worker.db import init_db

        db_info = init_db(runtime_dirs=runtime_dirs)
        loaded_config = LoadedWorkerConfig(
            config=WorkerConfig(),
            config_path=runtime_dirs.config_dir / "worker.toml",
            config_loaded=False,
        )
        client = TestClient(create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config, db_info=db_info))
        self.addCleanup(client.close)
        return client, runtime_dirs


if __name__ == "__main__":
    unittest.main()
