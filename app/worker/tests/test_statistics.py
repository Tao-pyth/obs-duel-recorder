from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class StatisticsApiTests(unittest.TestCase):
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

    def test_empty_statistics_are_stable_zero_counts(self) -> None:
        client, _ = self._client()

        summary = client.get("/statistics/summary")
        decks = client.get("/statistics/decks")
        opponents = client.get("/statistics/opponents")
        uploads = client.get("/statistics/uploads")

        self.assertEqual(summary.status_code, 200)
        self.assertEqual(summary.json()["total_matches"], 0)
        self.assertEqual(summary.json()["win_rate"], 0.0)
        self.assertEqual(decks.json()["items"], [])
        self.assertEqual(opponents.json()["items"], [])
        self.assertEqual(sum(uploads.json()["state_counts"].values()), 0)

    def test_summary_deck_opponent_and_filters_are_deterministic(self) -> None:
        client, _ = self._client()
        self._create_match(client, deck_name="Labrynth", opponent_deck="Branded", result="win", memo="Long game")
        self._create_match(client, deck_name="Labrynth", opponent_deck="Branded", result="loss")
        self._create_match(client, deck_name="Swordsoul", opponent_deck="Purrely", result="draw")
        self._create_match(client, deck_name="", opponent_deck="", result="")

        summary = client.get("/statistics/summary")
        self.assertEqual(summary.status_code, 200)
        self.assertEqual(summary.json()["result_counts"], {"win": 1, "loss": 1, "draw": 1, "unknown": 1})
        self.assertEqual(summary.json()["known_result_count"], 3)
        self.assertEqual(summary.json()["win_rate"], 0.3333)

        decks = client.get("/statistics/decks")
        self.assertEqual(decks.status_code, 200)
        self.assertEqual(decks.json()["items"][0]["name"], "Labrynth")
        self.assertEqual(decks.json()["items"][0]["total_matches"], 2)

        opponents = client.get("/statistics/opponents", params={"result": "win"})
        self.assertEqual(opponents.status_code, 200)
        self.assertEqual(opponents.json()["items"][0]["name"], "Branded")
        self.assertEqual(opponents.json()["items"][0]["result_counts"]["win"], 1)

    def test_memo_search_is_case_insensitive_and_does_not_mutate(self) -> None:
        client, _ = self._client()
        match_id = self._create_match(
            client,
            deck_name="Vanquish Soul",
            opponent_deck="Tearlaments",
            result="win",
            memo="Played around Nibiru in game two.",
        )["id"]

        found = client.get("/statistics/memos", params={"query": "nibiru"})

        self.assertEqual(found.status_code, 200)
        self.assertEqual(found.json()["items"][0]["match_id"], match_id)
        self.assertIn("Nibiru", found.json()["items"][0]["memo_excerpt"])
        self.assertEqual(client.get(f"/matches/{match_id}").json()["memo"], "Played around Nibiru in game two.")

    def test_upload_statistics_count_states_without_paths(self) -> None:
        client, runtime_dirs = self._client()
        match = self._create_match(client, result="win")
        client.post("/queue/items", json={"match_id": match["id"], "video_path": "duel.mp4"}).json()
        uploaded = client.post("/queue/items", json={"match_id": match["id"], "video_path": "duel2.mp4"}).json()
        failed = client.post(
            "/queue/items",
            json={"match_id": match["id"], "video_path": "duel3.mp4", "max_retries": 0},
        ).json()

        (runtime_dirs.videos_dir / "duel2.mp4").write_bytes(b"video")
        client.post(f"/queue/items/{uploaded['id']}/command", json={"action": "start_upload"})
        client.post(f"/queue/items/{uploaded['id']}/command", json={"action": "mark_uploaded", "youtube_video_id": "abc"})
        client.post(f"/queue/items/{failed['id']}/command", json={"action": "start_upload"})
        client.post(f"/queue/items/{failed['id']}/command", json={"action": "mark_upload_failed", "error_code": "network"})

        uploads = client.get("/statistics/uploads")

        self.assertEqual(uploads.status_code, 200)
        self.assertEqual(uploads.json()["state_counts"]["ready_upload"], 1)
        self.assertEqual(uploads.json()["state_counts"]["uploaded"], 1)
        self.assertEqual(uploads.json()["state_counts"]["need_manual_review"], 1)
        self.assertNotIn("duel", str(uploads.json()))

    def test_invalid_statistics_requests_are_diagnosable(self) -> None:
        client, _ = self._client()

        bad_limit = client.get("/statistics/decks", params={"limit": 0})
        bad_query = client.get("/statistics/memos", params={"query": " "})

        self.assertEqual(bad_limit.status_code, 400)
        self.assertEqual(bad_limit.json()["code"], "statistics_limit_invalid")
        self.assertEqual(bad_query.status_code, 400)
        self.assertEqual(bad_query.json()["code"], "statistics_query_invalid")

    def _create_match(self, client, **payload):
        response = client.post("/matches", json=payload)
        self.assertEqual(response.status_code, 200)
        return response.json()


if __name__ == "__main__":
    unittest.main()
