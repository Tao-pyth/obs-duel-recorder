from __future__ import annotations

import sys
import tempfile
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

    def test_upload_settings_expose_oauth_contract_without_secret_values(self) -> None:
        client, runtime_dirs = self._client()
        secrets_dir = runtime_dirs.config_dir / "secrets"
        secrets_dir.mkdir(parents=True, exist_ok=True)
        (secrets_dir / "youtube-token.json").write_text('{"refresh_token":"super-sensitive-token"}', encoding="utf-8")
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
        self.assertNotIn("super-sensitive-token", str(settings))
        self.assertNotIn("super-sensitive-client-secret", str(settings))

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
