from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class OverlayStateApiTests(unittest.TestCase):
    def _client(self):
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        user_data_dir = Path(tmp.name)
        runtime_dirs = ensure_runtime_dirs(user_data_dir=user_data_dir)
        loaded_config = LoadedWorkerConfig(
            config=WorkerConfig(),
            config_path=runtime_dirs.config_dir / "worker.toml",
            config_loaded=False,
        )
        return TestClient(create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config))

    def test_overlay_state_defaults_are_available(self) -> None:
        client = self._client()

        resp = client.get("/overlay/state")
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(
            resp.json(),
            {
                "deck_name": "",
                "sequence_number": "",
                "result": "unknown",
                "opponent_deck": "unknown",
                "recording_state": "idle",
            },
        )

    def test_overlay_state_update_preserves_missing_fields(self) -> None:
        client = self._client()

        resp = client.put(
            "/overlay/state",
            json={
                "deck_name": "Sample Deck",
                "sequence_number": "001",
                "recording_state": "recording",
            },
        )
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.json()["deck_name"], "Sample Deck")
        self.assertEqual(resp.json()["sequence_number"], "001")
        self.assertEqual(resp.json()["result"], "unknown")
        self.assertEqual(resp.json()["opponent_deck"], "unknown")
        self.assertEqual(resp.json()["recording_state"], "recording")

        second = client.get("/overlay/state")
        self.assertEqual(second.json(), resp.json())

    def test_overlay_state_rejects_invalid_payload(self) -> None:
        client = self._client()

        resp = client.put(
            "/overlay/state",
            json={"recording_state": "starting", "deck_name": 123, "extra": "value"},
        )
        self.assertEqual(resp.status_code, 400)
        body = resp.json()
        self.assertEqual(body["code"], "overlay_payload_invalid")
        self.assertEqual(body["details"]["recording_state"], "unknown_recording_state")
        self.assertEqual(body["details"]["deck_name"], "must_be_string")
        self.assertEqual(body["details"]["extra"], "unknown_field")

    def test_overlay_state_rejects_overlong_value(self) -> None:
        client = self._client()

        resp = client.put("/overlay/state", json={"deck_name": "x" * 257})
        self.assertEqual(resp.status_code, 400)
        self.assertEqual(resp.json()["details"]["deck_name"], "too_long")


if __name__ == "__main__":
    unittest.main()
