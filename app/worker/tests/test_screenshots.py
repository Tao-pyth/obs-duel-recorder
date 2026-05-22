from __future__ import annotations

import base64
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class ScreenshotApiTests(unittest.TestCase):
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

    def test_capture_uses_deterministic_name_and_db_linkage(self) -> None:
        client, runtime_dirs = self._client()
        match_id = self._create_match(runtime_dirs)
        queue = self._create_queue_item(client, match_id=match_id)

        captured = client.post(
            "/screenshots/capture",
            json={
                "match_id": match_id,
                "queue_item_id": queue["id"],
                "kind": "duel-start",
                "captured_at": "2026-05-23T04:00:00Z",
                "content_type": "image/png",
                "content_base64": base64.b64encode(b"fake-png").decode("ascii"),
            },
        )

        self.assertEqual(captured.status_code, 200)
        body = captured.json()
        self.assertEqual(body["match_id"], match_id)
        self.assertEqual(body["queue_item_id"], queue["id"])
        self.assertEqual(
            body["relative_path"],
            f"match-{match_id}/20260523T040000Z-duel-start-queue-{queue['id']}.png",
        )
        self.assertTrue((runtime_dirs.screenshots_dir / body["relative_path"]).exists())

        listed = client.get("/screenshots", params={"match_id": match_id})
        self.assertEqual(len(listed.json()["items"]), 1)

    def test_preview_returns_content_and_missing_preview_is_diagnosable(self) -> None:
        client, runtime_dirs = self._client()
        captured = client.post(
            "/screenshots/capture",
            json={
                "kind": "preview",
                "captured_at": "2026-05-23T04:01:00Z",
                "content_text": "preview-bytes",
            },
        ).json()

        preview = client.get(f"/screenshots/{captured['id']}/preview")
        self.assertEqual(preview.status_code, 200)
        self.assertTrue(preview.json()["available"])
        self.assertEqual(base64.b64decode(preview.json()["content_base64"]), b"preview-bytes")

        (runtime_dirs.screenshots_dir / captured["relative_path"]).unlink()
        missing = client.get(f"/screenshots/{captured['id']}/preview")
        self.assertEqual(missing.status_code, 200)
        self.assertFalse(missing.json()["available"])
        self.assertEqual(missing.json()["record"]["status"], "missing")

    def test_cleanup_deletes_only_uploaded_or_discarded_queue_screenshots(self) -> None:
        client, runtime_dirs = self._client()
        match_id = self._create_match(runtime_dirs)
        queue = self._create_queue_item(client, match_id=match_id)
        captured = self._capture_for_queue(client, match_id=match_id, queue_item_id=queue["id"])

        preserved = client.post("/screenshots/cleanup", json={"queue_item_id": queue["id"]})
        self.assertEqual(preserved.status_code, 200)
        self.assertTrue(preserved.json()["preserved"])
        self.assertTrue((runtime_dirs.screenshots_dir / captured["relative_path"]).exists())

        client.post(f"/queue/items/{queue['id']}/command", json={"action": "start_upload"})
        client.post(
            f"/queue/items/{queue['id']}/command",
            json={"action": "mark_uploaded", "youtube_video_id": "abc123"},
        )
        cleaned = client.post("/screenshots/cleanup", json={"queue_item_id": queue["id"]})
        self.assertEqual(cleaned.status_code, 200)
        self.assertFalse(cleaned.json()["preserved"])
        self.assertEqual(cleaned.json()["cleaned"][0]["status"], "deleted")
        self.assertFalse((runtime_dirs.screenshots_dir / captured["relative_path"]).exists())

    def test_cleanup_preserves_manual_review_evidence(self) -> None:
        client, runtime_dirs = self._client()
        match_id = self._create_match(runtime_dirs)
        queue = self._create_queue_item(client, match_id=match_id, max_retries=0)
        captured = self._capture_for_queue(client, match_id=match_id, queue_item_id=queue["id"])

        client.post(f"/queue/items/{queue['id']}/command", json={"action": "start_upload"})
        client.post(
            f"/queue/items/{queue['id']}/command",
            json={"action": "mark_upload_failed", "error_code": "network_timeout"},
        )
        cleanup = client.post("/screenshots/cleanup", json={"queue_item_id": queue["id"]})

        self.assertEqual(cleanup.status_code, 200)
        self.assertTrue(cleanup.json()["preserved"])
        self.assertEqual(cleanup.json()["reason"], "queue_state_need_manual_review")
        self.assertTrue((runtime_dirs.screenshots_dir / captured["relative_path"]).exists())

    def test_capture_validation_is_stable(self) -> None:
        client, _ = self._client()

        resp = client.post("/screenshots/capture", json={})

        self.assertEqual(resp.status_code, 400)
        self.assertEqual(resp.json()["code"], "screenshot_payload_invalid")
        self.assertEqual(resp.json()["details"]["content"], "content_base64_or_content_text_required")

    def test_invalid_db_link_does_not_leave_orphan_file(self) -> None:
        client, runtime_dirs = self._client()

        resp = client.post(
            "/screenshots/capture",
            json={
                "match_id": 999,
                "kind": "orphan-check",
                "captured_at": "2026-05-23T04:03:00Z",
                "content_text": "not linked",
            },
        )

        self.assertEqual(resp.status_code, 400)
        self.assertEqual(resp.json()["code"], "screenshot_db_link_invalid")
        self.assertEqual(list((runtime_dirs.screenshots_dir / "match-999").glob("*")), [])

    def _create_queue_item(self, client, *, match_id: int, max_retries: int = 3) -> dict[str, object]:
        return client.post(
            "/queue/items",
            json={"match_id": match_id, "video_path": f"duel-{match_id}.mp4", "max_retries": max_retries},
        ).json()

    def _create_match(self, runtime_dirs) -> int:
        conn = sqlite3.connect(runtime_dirs.db_dir / "odr.sqlite3")
        try:
            cursor = conn.execute("INSERT INTO matches DEFAULT VALUES;")
            conn.commit()
            return int(cursor.lastrowid)
        finally:
            conn.close()

    def _capture_for_queue(self, client, *, match_id: int, queue_item_id: int) -> dict[str, object]:
        return client.post(
            "/screenshots/capture",
            json={
                "match_id": match_id,
                "queue_item_id": queue_item_id,
                "kind": "duel-end",
                "captured_at": "2026-05-23T04:02:00Z",
                "content_text": "screenshot bytes",
            },
        ).json()


if __name__ == "__main__":
    unittest.main()
