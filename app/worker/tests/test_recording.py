from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class RecordingStateApiTests(unittest.TestCase):
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
        return TestClient(create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config)), runtime_dirs

    def test_recording_state_defaults_to_idle(self) -> None:
        client, _ = self._client()

        resp = client.get("/recording/state")

        self.assertEqual(resp.status_code, 200)
        body = resp.json()
        self.assertEqual(body["state"], "idle")
        self.assertEqual(body["session_id"], "")
        self.assertEqual(body["command_source"], "recovery")
        self.assertEqual(body["last_action"], "init")
        self.assertTrue(body["updated_at"].endswith("Z"))

    def test_manual_start_confirm_stop_flow(self) -> None:
        client, _ = self._client()

        start = client.post("/recording/command", json={"action": "start", "source": "manual"})
        self.assertEqual(start.status_code, 200)
        start_body = start.json()
        self.assertEqual(start_body["state"], "starting")
        self.assertEqual(start_body["command_source"], "manual")
        self.assertEqual(start_body["last_action"], "start")
        self.assertNotEqual(start_body["session_id"], "")

        overlay_start = client.get("/overlay/state")
        self.assertEqual(overlay_start.json()["recording_state"], "recording")

        confirmed = client.post("/recording/command", json={"action": "confirm_started", "source": "manual"})
        self.assertEqual(confirmed.status_code, 200)
        self.assertEqual(confirmed.json()["state"], "recording")
        self.assertEqual(confirmed.json()["session_id"], start_body["session_id"])

        stopping = client.post("/recording/command", json={"action": "stop", "source": "manual"})
        self.assertEqual(stopping.status_code, 200)
        self.assertEqual(stopping.json()["state"], "stopping")

        completed = client.post("/recording/command", json={"action": "confirm_stopped", "source": "manual"})
        self.assertEqual(completed.status_code, 200)
        self.assertEqual(completed.json()["state"], "completed")
        self.assertEqual(completed.json()["session_id"], start_body["session_id"])

        overlay_completed = client.get("/overlay/state")
        self.assertEqual(overlay_completed.json()["recording_state"], "idle")

    def test_conflicting_commands_return_stable_diagnostics(self) -> None:
        client, _ = self._client()

        stop = client.post("/recording/command", json={"action": "stop", "source": "manual"})

        self.assertEqual(stop.status_code, 409)
        body = stop.json()
        self.assertEqual(body["code"], "recording_transition_invalid")
        self.assertEqual(body["details"]["state"], "idle")
        self.assertEqual(body["details"]["action"], "stop")

    def test_payload_validation_returns_stable_diagnostics(self) -> None:
        client, _ = self._client()

        resp = client.post(
            "/recording/command",
            json={"action": "bad", "source": "operator", "reason": 123},
        )

        self.assertEqual(resp.status_code, 400)
        body = resp.json()
        self.assertEqual(body["code"], "recording_command_invalid")
        self.assertEqual(body["details"]["action"], "unknown_action")
        self.assertEqual(body["details"]["source"], "unknown_source")
        self.assertEqual(body["details"]["reason"], "must_be_string")

    def test_automatic_commands_use_same_boundary(self) -> None:
        client, _ = self._client()

        resp = client.post("/recording/command", json={"action": "start", "source": "automatic"})

        self.assertEqual(resp.status_code, 200)
        body = resp.json()
        self.assertEqual(body["state"], "starting")
        self.assertEqual(body["command_source"], "automatic")

    def test_interrupted_state_recovers_on_startup(self) -> None:
        client, runtime_dirs = self._client()

        started = client.post("/recording/command", json={"action": "start", "source": "manual"})
        self.assertEqual(started.status_code, 200)
        state_path = runtime_dirs.data_dir / "recording-state.json"
        self.assertTrue(state_path.exists())

        second_client, _ = self._client_for_runtime(runtime_dirs)
        recovered = second_client.get("/recording/state")

        self.assertEqual(recovered.status_code, 200)
        body = recovered.json()
        self.assertEqual(body["state"], "interrupted")
        self.assertEqual(body["command_source"], "recovery")
        self.assertEqual(body["last_action"], "startup_recovery")
        self.assertEqual(body["session_id"], started.json()["session_id"])

        overlay = second_client.get("/overlay/state")
        self.assertEqual(overlay.json()["recording_state"], "unknown")

    def test_discard_interrupted_returns_to_idle(self) -> None:
        client, runtime_dirs = self._client()
        state_path = runtime_dirs.data_dir / "recording-state.json"
        state_path.write_text(
            json.dumps(
                {
                    "state": "recording",
                    "session_id": "session-1",
                    "command_source": "manual",
                    "last_action": "confirm_started",
                    "reason": "",
                    "updated_at": "2026-05-23T00:00:00Z",
                }
            ),
            encoding="utf-8",
        )
        recovered_client, _ = self._client_for_runtime(runtime_dirs)

        discarded = recovered_client.post(
            "/recording/command",
            json={"action": "discard_interrupted", "source": "recovery"},
        )

        self.assertEqual(discarded.status_code, 200)
        self.assertEqual(discarded.json()["state"], "idle")
        self.assertEqual(discarded.json()["session_id"], "")

    def _client_for_runtime(self, runtime_dirs):
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig

        loaded_config = LoadedWorkerConfig(
            config=WorkerConfig(),
            config_path=runtime_dirs.config_dir / "worker.toml",
            config_loaded=False,
        )
        return TestClient(create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config)), runtime_dirs


if __name__ == "__main__":
    unittest.main()
