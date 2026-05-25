from __future__ import annotations

import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class MatchMetadataApiTests(unittest.TestCase):
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

    def test_create_edit_read_and_search_metadata(self) -> None:
        client, _ = self._client()

        created = client.post(
            "/matches",
            json={
                "deck_name": "Labrynth",
                "opponent_deck": "Branded",
                "result": "win",
                "rank": "Diamond I",
                "dp": "12000",
                "memo": "Game 2 went long.",
                "started_at": "2026-05-23T05:00:00Z",
            },
        )

        self.assertEqual(created.status_code, 200)
        match_id = created.json()["id"]
        self.assertEqual(created.json()["opponent_deck"], "Branded")
        self.assertEqual(created.json()["rank"], "Diamond I")
        self.assertEqual(created.json()["dp"], "12000")

        edited = client.put(
            f"/matches/{match_id}/metadata",
            json={"opponent_deck": "Branded Despia", "memo": "Played around Nibiru."},
        )
        self.assertEqual(edited.status_code, 200)
        self.assertEqual(edited.json()["opponent_deck"], "Branded Despia")
        self.assertEqual(edited.json()["memo"], "Played around Nibiru.")

        fetched = client.get(f"/matches/{match_id}")
        self.assertEqual(fetched.status_code, 200)
        self.assertEqual(fetched.json()["deck_name"], "Labrynth")

        searched = client.get("/matches", params={"query": "Despia"})
        self.assertEqual(searched.status_code, 200)
        self.assertEqual([item["id"] for item in searched.json()["items"]], [match_id])

    def test_metadata_validation_is_non_destructive(self) -> None:
        client, _ = self._client()
        match_id = client.post("/matches", json={"opponent_deck": "Kashtira"}).json()["id"]

        invalid = client.put(f"/matches/{match_id}/metadata", json={"opponent_deck": "x" * 121})

        self.assertEqual(invalid.status_code, 400)
        self.assertEqual(invalid.json()["code"], "metadata_payload_invalid")
        fetched = client.get(f"/matches/{match_id}")
        self.assertEqual(fetched.json()["opponent_deck"], "Kashtira")

    def test_upload_metadata_generation_is_deterministic(self) -> None:
        client, _ = self._client()
        match_id = client.post(
            "/matches",
            json={
                "deck_name": "Swordsoul",
                "opponent_deck": "Purrely",
                "result": "loss",
                "memo": "Misplayed turn three.",
                "title_template": "{deck_name} vs {opponent_deck} [{result}]",
            },
        ).json()["id"]

        metadata = client.get(f"/matches/{match_id}/upload-metadata")

        self.assertEqual(metadata.status_code, 200)
        self.assertEqual(metadata.json()["title"], "Swordsoul vs Purrely [loss]")
        self.assertIn("Notes:", metadata.json()["description"])
        self.assertIn("Rank: unknown", metadata.json()["description"])
        self.assertIn("DP: unknown", metadata.json()["description"])
        self.assertIn("Misplayed turn three.", metadata.json()["description"])
        self.assertIn("opponent_deck", metadata.json()["variables"])
        self.assertEqual(metadata.json()["missing_fields"], ["rank", "dp"])
        self.assertEqual(metadata.json()["warning"], "Missing metadata fields: rank, dp")

    def test_title_generation_uses_fallbacks_and_length_limit(self) -> None:
        client, _ = self._client()
        match_id = client.post(
            "/matches",
            json={"title_template": "Replay {unknown_variable} " + ("x" * 120)},
        ).json()["id"]

        metadata = client.get(f"/matches/{match_id}/upload-metadata")

        self.assertEqual(metadata.status_code, 200)
        self.assertLessEqual(len(metadata.json()["title"]), 100)
        self.assertTrue(metadata.json()["title"].startswith("Replay unknown"))
        self.assertIn("deck_name", metadata.json()["missing_fields"])
        self.assertIn("opponent_deck", metadata.json()["missing_fields"])

    def test_metadata_persists_across_restart(self) -> None:
        client, runtime_dirs = self._client()
        match_id = client.post("/matches", json={"opponent_deck": "Runick", "memo": "Keep this."}).json()["id"]

        second_client = self._client_for_runtime(runtime_dirs)
        fetched = second_client.get(f"/matches/{match_id}")

        self.assertEqual(fetched.status_code, 200)
        self.assertEqual(fetched.json()["opponent_deck"], "Runick")
        self.assertEqual(fetched.json()["memo"], "Keep this.")

    def test_latest_match_returns_most_recent_for_ui(self) -> None:
        client, _ = self._client()
        client.post("/matches", json={"deck_name": "First"})
        second = client.post("/matches", json={"deck_name": "Second"}).json()

        latest = client.get("/matches/latest")

        self.assertEqual(latest.status_code, 200)
        self.assertEqual(latest.json()["id"], second["id"])
        self.assertEqual(latest.json()["deck_name"], "Second")

    def test_latest_match_reports_missing_metadata_for_ui(self) -> None:
        client, _ = self._client()

        latest = client.get("/matches/latest")

        self.assertEqual(latest.status_code, 404)
        self.assertEqual(latest.json()["code"], "match_not_found")

    def test_migration_preserves_v1_upload_records(self) -> None:
        from odr_worker.db import init_db
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        with tempfile.TemporaryDirectory() as tmp:
            runtime_dirs = ensure_runtime_dirs(user_data_dir=Path(tmp))
            db_info = init_db(runtime_dirs=runtime_dirs)
            conn = sqlite3.connect(db_info.db_path)
            try:
                conn.execute(
                    """
                    INSERT INTO matches(opponent_deck, memo)
                    VALUES('Mathmech', 'existing metadata');
                    """
                )
                match_id = conn.execute("SELECT id FROM matches ORDER BY id DESC LIMIT 1;").fetchone()[0]
                conn.execute(
                    """
                    INSERT INTO upload_queue(match_id, state, video_path, youtube_video_id, youtube_url)
                    VALUES(?, 'uploaded', 'duel.mp4', 'abc123', 'https://youtu.be/abc123');
                    """,
                    (match_id,),
                )
                conn.commit()
                row = conn.execute(
                    """
                    SELECT m.opponent_deck, q.youtube_video_id
                    FROM matches m JOIN upload_queue q ON q.match_id = m.id
                    WHERE m.id = ?;
                    """,
                    (match_id,),
                ).fetchone()
            finally:
                conn.close()

        self.assertEqual(row[0], "Mathmech")
        self.assertEqual(row[1], "abc123")

    def test_upload_process_returns_generated_metadata_when_match_is_linked(self) -> None:
        client, runtime_dirs = self._client()
        video_path = runtime_dirs.videos_dir / "duel.mp4"
        video_path.write_bytes(b"fake video")
        match_id = client.post(
            "/matches",
            json={"deck_name": "Vanquish Soul", "opponent_deck": "Tearlaments", "result": "win"},
        ).json()["id"]
        queue = client.post(
            "/queue/items",
            json={"match_id": match_id, "video_path": "duel.mp4"},
        )
        self.assertEqual(queue.status_code, 200)

        uploaded = client.post(
            "/upload/process-next",
            json={"mock_result": "success", "youtube_video_id": "meta123"},
        )

        self.assertEqual(uploaded.status_code, 200)
        self.assertEqual(uploaded.json()["item"]["state"], "uploaded")
        self.assertEqual(uploaded.json()["upload_metadata"]["match_id"], match_id)
        self.assertIn("Tearlaments", uploaded.json()["upload_metadata"]["title"])

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
