from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class ImageRecognitionApiTests(unittest.TestCase):
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
        return client

    def test_fixture_provider_returns_candidates_without_mutating_metadata(self) -> None:
        client = self._client()
        match_id = client.post("/matches", json={"result": "unknown"}).json()["id"]

        analyzed = client.post(
            "/recognition/analyze",
            json={
                "provider": "fixture",
                "match_id": match_id,
                "content_text": "result: Victory\nrank: Diamond I\ndp: 12,345\nconfidence: 0.92",
            },
        )

        self.assertEqual(analyzed.status_code, 200)
        body = analyzed.json()
        self.assertEqual(body["status"], "candidates_available")
        self.assertFalse(body["mutated"])
        self.assertEqual(body["metadata_patch"], {"result": "win", "rank": "Diamond I", "dp": "12345"})
        self.assertEqual(body["manual_correction"]["endpoint"], f"/matches/{match_id}/metadata")
        self.assertEqual([item["field"] for item in body["candidate_records"]], ["result", "rank", "dp"])

        unchanged = client.get(f"/matches/{match_id}")
        self.assertEqual(unchanged.json()["result"], "unknown")
        self.assertEqual(unchanged.json()["rank"], "")
        self.assertEqual(unchanged.json()["dp"], "")

    def test_manual_correction_can_persist_recognized_metadata(self) -> None:
        client = self._client()
        match_id = client.post("/matches", json={}).json()["id"]
        patch = client.post(
            "/recognition/analyze",
            json={"match_id": match_id, "fixture": {"result": "loss", "rank": "Gold V", "dp": "8500"}},
        ).json()["metadata_patch"]

        corrected = client.put(f"/matches/{match_id}/metadata", json=patch)

        self.assertEqual(corrected.status_code, 200)
        self.assertEqual(corrected.json()["result"], "loss")
        self.assertEqual(corrected.json()["rank"], "Gold V")
        self.assertEqual(corrected.json()["dp"], "8500")

    def test_candidate_confirm_correct_and_reject_are_auditable(self) -> None:
        client = self._client()
        match_id = client.post("/matches", json={}).json()["id"]
        records = client.post(
            "/recognition/analyze",
            json={"match_id": match_id, "content_text": "result: win\nrank: Bronze III\ndp: 1000"},
        ).json()["candidate_records"]

        confirmed = client.post(f"/recognition/candidates/{records[0]['id']}/command", json={"action": "confirm"})
        corrected = client.post(
            f"/recognition/candidates/{records[1]['id']}/command",
            json={"action": "correct", "value": "Silver I"},
        )
        rejected = client.post(f"/recognition/candidates/{records[2]['id']}/command", json={"action": "reject"})

        self.assertEqual(confirmed.status_code, 200)
        self.assertEqual(confirmed.json()["status"], "confirmed")
        self.assertEqual(corrected.status_code, 200)
        self.assertEqual(corrected.json()["status"], "corrected")
        self.assertEqual(corrected.json()["corrected_value"], "Silver I")
        self.assertEqual(rejected.status_code, 200)
        self.assertEqual(rejected.json()["status"], "rejected")

        match = client.get(f"/matches/{match_id}").json()
        self.assertEqual(match["result"], "win")
        self.assertEqual(match["rank"], "Silver I")
        self.assertEqual(match["dp"], "")

        listed = client.get("/recognition/candidates", params={"match_id": match_id})
        self.assertEqual([item["status"] for item in listed.json()["items"]], ["confirmed", "corrected", "rejected"])

    def test_low_confidence_requires_manual_review_and_no_patch(self) -> None:
        client = self._client()

        analyzed = client.post(
            "/recognition/analyze",
            json={"content_text": "result: win\nrank: Platinum\ndp: 4000\nconfidence: 0.50"},
        )

        self.assertEqual(analyzed.status_code, 200)
        self.assertEqual(analyzed.json()["status"], "manual_review_required")
        self.assertEqual(analyzed.json()["metadata_patch"], {})
        self.assertEqual(analyzed.json()["diagnostics"][0]["code"], "low_confidence")
        self.assertEqual(len(analyzed.json()["candidate_records"]), 3)

    def test_invalid_provider_and_missing_match_are_diagnosable(self) -> None:
        client = self._client()

        unsupported = client.post("/recognition/analyze", json={"provider": "real_ocr", "content_text": "result: win"})
        self.assertEqual(unsupported.status_code, 400)
        self.assertEqual(unsupported.json()["code"], "recognition_provider_unsupported")

        missing = client.post("/recognition/analyze", json={"match_id": 999, "content_text": "result: win"})
        self.assertEqual(missing.status_code, 404)
        self.assertEqual(missing.json()["code"], "match_not_found")


if __name__ == "__main__":
    unittest.main()
