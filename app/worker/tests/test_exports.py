from __future__ import annotations

import json
import csv
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class ExportApiTests(unittest.TestCase):
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

    def test_export_creates_manifest_db_metadata_and_screenshot_zip(self) -> None:
        client, runtime_dirs = self._client()
        self._write_secret_files(runtime_dirs)
        (runtime_dirs.videos_dir / "duel.mp4").write_bytes(b"fake video")
        match_id = client.post(
            "/matches",
            json={"deck_name": "Labrynth", "opponent_deck": "Branded", "memo": "Export this."},
        ).json()["id"]
        queue = client.post(
            "/queue/items",
            json={"match_id": match_id, "video_path": "duel.mp4"},
        ).json()
        screenshot = client.post(
            "/screenshots/capture",
            json={
                "match_id": match_id,
                "queue_item_id": queue["id"],
                "kind": "export",
                "captured_at": "2026-05-23T12:00:00Z",
                "content_text": "screenshot bytes",
            },
        ).json()

        exported = client.post("/exports", json={"created_at": "2026-05-23T12:01:00Z"})

        self.assertEqual(exported.status_code, 200)
        body = exported.json()
        self.assertEqual(body["status"], "completed")
        self.assertEqual(body["file_name"], "odr-export-20260523T120100Z.zip")
        self.assertEqual(body["manifest"]["app_version"], "2.3.0")
        self.assertEqual(body["manifest"]["api_version"], "2.3")
        self.assertFalse(body["manifest"]["include_videos"])
        archive_path = Path(body["path"])
        self.assertTrue(archive_path.exists())

        with zipfile.ZipFile(archive_path) as archive:
            names = set(archive.namelist())
            self.assertIn("manifest.json", names)
            self.assertIn("database/odr.sqlite3", names)
            self.assertIn("metadata/matches.json", names)
            self.assertIn("metadata/upload_queue.json", names)
            self.assertIn("metadata/screenshots.json", names)
            self.assertIn("metadata/video_linkages.json", names)
            self.assertIn(f"screenshots/{screenshot['relative_path']}", names)
            self.assertNotIn("config/secrets/youtube-token.json", names)
            self.assertNotIn("logs/worker.log", names)
            self.assertNotIn("videos/queue-1-duel.mp4", names)
            manifest = json.loads(archive.read("manifest.json"))
            matches = json.loads(archive.read("metadata/matches.json"))
            linkages = json.loads(archive.read("metadata/video_linkages.json"))

        self.assertEqual(manifest["counts"]["matches"], 1)
        self.assertEqual(manifest["counts"]["screenshots_included"], 1)
        self.assertEqual(manifest["counts"]["video_linkages"], 1)
        self.assertEqual(manifest["counts"]["videos_included"], 0)
        self.assertEqual(matches[0]["opponent_deck"], "Branded")
        self.assertEqual(linkages[0]["youtube_video_id"], "")

        listed = client.get("/exports")
        self.assertEqual(listed.status_code, 200)
        self.assertEqual(listed.json()["items"][0]["file_name"], body["file_name"])

    def test_export_records_missing_screenshot_and_video_files(self) -> None:
        client, runtime_dirs = self._client()
        match_id = client.post("/matches", json={"opponent_deck": "Runick"}).json()["id"]
        queue = client.post(
            "/queue/items",
            json={"match_id": match_id, "video_path": "missing.mp4"},
        ).json()
        screenshot = client.post(
            "/screenshots/capture",
            json={
                "match_id": match_id,
                "queue_item_id": queue["id"],
                "kind": "missing",
                "captured_at": "2026-05-23T12:02:00Z",
                "content_text": "gone",
            },
        ).json()
        (runtime_dirs.screenshots_dir / screenshot["relative_path"]).unlink()

        exported = client.post("/exports", json={"name": "missing-check", "created_at": "2026-05-23T12:03:00Z"})

        self.assertEqual(exported.status_code, 200)
        missing = exported.json()["manifest"]["missing_files"]
        self.assertIn({"kind": "screenshot", "id": screenshot["id"], "relative_path": screenshot["relative_path"]}, missing)
        self.assertIn({"kind": "video", "queue_item_id": queue["id"], "path": "missing.mp4"}, missing)

    def test_export_conflict_does_not_replace_existing_archive(self) -> None:
        client, _ = self._client()
        payload = {"created_at": "2026-05-23T12:04:00Z"}
        first = client.post("/exports", json=payload)
        self.assertEqual(first.status_code, 200)
        archive_path = Path(first.json()["path"])
        original_size = archive_path.stat().st_size

        second = client.post("/exports", json=payload)

        self.assertEqual(second.status_code, 400)
        self.assertEqual(second.json()["code"], "export_path_conflict")
        self.assertTrue(archive_path.exists())
        self.assertEqual(archive_path.stat().st_size, original_size)
        self.assertFalse(list(archive_path.parent.glob("*.tmp")))

    def test_include_videos_is_explicit_and_validated(self) -> None:
        client, runtime_dirs = self._client()
        (runtime_dirs.videos_dir / "duel.mp4").write_bytes(b"fake video")
        client.post("/queue/items", json={"video_path": "duel.mp4"})

        invalid = client.post("/exports", json={"include_videos": "yes"})
        self.assertEqual(invalid.status_code, 400)
        self.assertEqual(invalid.json()["code"], "export_payload_invalid")

        exported = client.post(
            "/exports",
            json={"name": "with-video", "created_at": "2026-05-23T12:05:00Z", "include_videos": True},
        )
        self.assertEqual(exported.status_code, 200)
        with zipfile.ZipFile(Path(exported.json()["path"])) as archive:
            self.assertIn("videos/queue-1-duel.mp4", set(archive.namelist()))

    def test_registration_csv_exports_match_and_queue_status(self) -> None:
        client, runtime_dirs = self._client()
        match_id = client.post(
            "/matches",
            json={
                "deck_name": "Labrynth",
                "opponent_deck": "Branded",
                "result": "win",
                "rank": "Master",
                "dp": "12000",
                "memo": "CSV check",
            },
        ).json()["id"]
        queue = client.post("/queue/items", json={"match_id": match_id, "video_path": "duel.mp4"}).json()
        save_dir = runtime_dirs.exports_dir / "csv"

        exported = client.post("/exports/registration-csv", json={"save_dir": str(save_dir)})

        self.assertEqual(exported.status_code, 200)
        body = exported.json()
        self.assertEqual(body["status"], "completed")
        self.assertTrue(body["file_name"].startswith("odr-registration-"))
        self.assertTrue(body["file_name"].endswith(".csv"))
        csv_path = Path(body["path"])
        self.assertTrue(csv_path.exists())
        with csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
            rows = list(csv.DictReader(handle))

        self.assertEqual(body["row_count"], 1)
        self.assertEqual(rows[0]["match_id"], str(match_id))
        self.assertEqual(rows[0]["queue_item_id"], str(queue["id"]))
        self.assertEqual(rows[0]["deck_name"], "Labrynth")
        self.assertEqual(rows[0]["deck_sequence_number"], "1")
        self.assertEqual(rows[0]["queue_status"], "ready_upload")
        self.assertNotIn("refresh_token", csv_path.read_text(encoding="utf-8-sig"))

    def _write_secret_files(self, runtime_dirs) -> None:
        secrets_dir = runtime_dirs.config_dir / "secrets"
        secrets_dir.mkdir(parents=True, exist_ok=True)
        (secrets_dir / "youtube-token.json").write_text('{"refresh_token":"secret"}', encoding="utf-8")
        (secrets_dir / "youtube-client-secret.json").write_text('{"client_secret":"secret"}', encoding="utf-8")
        (runtime_dirs.logs_dir / "worker.log").write_text("secret log", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
