from __future__ import annotations

import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class QueueRecoveryApiTests(unittest.TestCase):
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

    def test_queue_item_can_retry_and_upload(self) -> None:
        client, _ = self._client()

        created = client.post("/queue/items", json={"video_path": "duel.mp4", "max_retries": 2})
        self.assertEqual(created.status_code, 200)
        item_id = created.json()["id"]
        self.assertEqual(created.json()["state"], "ready_upload")

        started = client.post(f"/queue/items/{item_id}/command", json={"action": "start_upload"})
        self.assertEqual(started.status_code, 200)
        self.assertEqual(started.json()["state"], "uploading")

        failed = client.post(
            f"/queue/items/{item_id}/command",
            json={
                "action": "mark_upload_failed",
                "error_code": "network_timeout",
                "error_message": "temporary network failure",
                "next_attempt_at": "2026-05-23T12:00:00Z",
            },
        )
        self.assertEqual(failed.status_code, 200)
        self.assertEqual(failed.json()["state"], "upload_failed")
        self.assertEqual(failed.json()["retry_count"], 1)

        retried = client.post(f"/queue/items/{item_id}/command", json={"action": "retry"})
        self.assertEqual(retried.status_code, 200)
        self.assertEqual(retried.json()["state"], "ready_upload")

        client.post(f"/queue/items/{item_id}/command", json={"action": "start_upload"})
        uploaded = client.post(
            f"/queue/items/{item_id}/command",
            json={
                "action": "mark_uploaded",
                "youtube_video_id": "abc123",
                "youtube_url": "https://youtu.be/abc123",
            },
        )
        self.assertEqual(uploaded.status_code, 200)
        self.assertEqual(uploaded.json()["state"], "uploaded")
        self.assertEqual(uploaded.json()["youtube_video_id"], "abc123")

    def test_retry_limit_moves_to_manual_review(self) -> None:
        client, _ = self._client()
        item = client.post("/queue/items", json={"video_path": "duel.mp4", "max_retries": 0}).json()

        client.post(f"/queue/items/{item['id']}/command", json={"action": "start_upload"})
        failed = client.post(
            f"/queue/items/{item['id']}/command",
            json={"action": "mark_upload_failed", "error_code": "network_timeout"},
        )

        self.assertEqual(failed.status_code, 200)
        self.assertEqual(failed.json()["state"], "need_manual_review")
        self.assertEqual(failed.json()["manual_review_reason"], "retry_limit_exceeded")
        self.assertEqual(failed.json()["manual_review_evidence"]["retry_count"], 1)

    def test_quota_waiting_persists_next_attempt(self) -> None:
        client, _ = self._client()
        item = client.post("/queue/items", json={"video_path": "duel.mp4"}).json()
        client.post(f"/queue/items/{item['id']}/command", json={"action": "start_upload"})

        quota = client.post(
            f"/queue/items/{item['id']}/command",
            json={
                "action": "mark_quota_waiting",
                "next_attempt_at": "2026-05-24T00:00:00Z",
                "error_message": "quota exceeded",
            },
        )

        self.assertEqual(quota.status_code, 200)
        self.assertEqual(quota.json()["state"], "quota_waiting")
        self.assertEqual(quota.json()["next_attempt_at"], "2026-05-24T00:00:00Z")

        listed = client.get("/queue/items", params={"state": "quota_waiting"})
        self.assertEqual(listed.status_code, 200)
        self.assertEqual(len(listed.json()["items"]), 1)

    def test_queue_runtime_queries_use_counts_and_single_ready_item(self) -> None:
        from odr_worker.queue import QueueStore

        client, runtime_dirs = self._client()
        first = client.post("/queue/items", json={"video_path": "first.mp4"}).json()
        second = client.post("/queue/items", json={"video_path": "second.mp4"}).json()
        client.post(f"/queue/items/{second['id']}/command", json={"action": "start_upload"})

        store = QueueStore(runtime_dirs.db_dir / "odr.sqlite3")
        counts = store.count_by_state()
        ready = store.next_ready_item()

        self.assertEqual(counts["ready_upload"], 1)
        self.assertEqual(counts["uploading"], 1)
        self.assertEqual(ready.id, first["id"])
        self.assertEqual(ready.video_path, "first.mp4")

    def test_startup_recovery_moves_interrupted_upload_to_manual_review(self) -> None:
        client, runtime_dirs = self._client()
        item = client.post("/queue/items", json={"video_path": "duel.mp4"}).json()
        started = client.post(f"/queue/items/{item['id']}/command", json={"action": "start_upload"})
        self.assertEqual(started.json()["state"], "uploading")

        second_client = self._client_for_runtime(runtime_dirs)
        recovery = second_client.get("/queue/recovery")
        recovered = recovery.json()["recovered"]

        self.assertEqual(recovery.json()["scanned_count"], 1)
        self.assertEqual(recovery.json()["recovered_count"], 1)
        self.assertGreaterEqual(recovery.json()["duration_ms"], 0)
        self.assertEqual(recovered[0]["id"], item["id"])
        self.assertEqual(recovered[0]["to"], "need_manual_review")

        recovered_item = second_client.get(f"/queue/items/{item['id']}")
        self.assertEqual(recovered_item.json()["state"], "need_manual_review")
        self.assertEqual(
            recovered_item.json()["manual_review_reason"],
            "interrupted_upload_requires_manual_review",
        )

    def test_startup_recovery_discards_missing_video(self) -> None:
        client, runtime_dirs = self._client()
        missing_path = str(runtime_dirs.videos_dir / "missing.mp4")
        item = client.post("/queue/items", json={"video_path": missing_path}).json()
        client.post(f"/queue/items/{item['id']}/command", json={"action": "start_upload"})

        second_client = self._client_for_runtime(runtime_dirs)
        recovered_item = second_client.get(f"/queue/items/{item['id']}")

        self.assertEqual(recovered_item.json()["state"], "discarded")
        self.assertEqual(recovered_item.json()["manual_review_reason"], "local_video_missing")

    def test_startup_recovery_preserves_success_marker(self) -> None:
        client, runtime_dirs = self._client()
        item = client.post("/queue/items", json={"video_path": "duel.mp4"}).json()
        client.post(f"/queue/items/{item['id']}/command", json={"action": "start_upload"})

        conn = sqlite3.connect(runtime_dirs.db_dir / "odr.sqlite3")
        try:
            conn.execute(
                "UPDATE upload_queue SET youtube_video_id = ?, youtube_url = ? WHERE id = ?;",
                ("abc123", "https://youtu.be/abc123", item["id"]),
            )
            conn.commit()
        finally:
            conn.close()

        second_client = self._client_for_runtime(runtime_dirs)
        recovered_item = second_client.get(f"/queue/items/{item['id']}")

        self.assertEqual(recovered_item.json()["state"], "uploaded")
        self.assertEqual(recovered_item.json()["youtube_video_id"], "abc123")

    def test_invalid_transition_is_stable_conflict(self) -> None:
        client, _ = self._client()
        item = client.post("/queue/items", json={"video_path": "duel.mp4"}).json()

        uploaded = client.post(
            f"/queue/items/{item['id']}/command",
            json={"action": "mark_uploaded", "youtube_video_id": "abc123"},
        )

        self.assertEqual(uploaded.status_code, 409)
        self.assertEqual(uploaded.json()["code"], "queue_transition_invalid")
        self.assertEqual(uploaded.json()["details"]["state"], "ready_upload")

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
        return client


if __name__ == "__main__":
    unittest.main()
