from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class TemplateDetectionApiTests(unittest.TestCase):
    def _client(self, *, config_text: str | None = None, templates: dict[str, str] | None = None):
        from fastapi.testclient import TestClient

        from odr_worker.api import create_app
        from odr_worker.config import LoadedWorkerConfig, WorkerConfig
        from odr_worker.runtime_dirs import ensure_runtime_dirs

        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        runtime_dirs = ensure_runtime_dirs(user_data_dir=Path(tmp.name))

        if templates:
            templates_dir = runtime_dirs.user_data_dir / "templates"
            templates_dir.mkdir(parents=True, exist_ok=True)
            for name, content in templates.items():
                (templates_dir / name).write_text(content, encoding="utf-8")

        if config_text is not None:
            config_path = runtime_dirs.config_dir / "templates.toml"
            config_path.write_text(config_text, encoding="utf-8")

        loaded_config = LoadedWorkerConfig(
            config=WorkerConfig(),
            config_path=runtime_dirs.config_dir / "worker.toml",
            config_loaded=False,
        )
        client = TestClient(create_app(runtime_dirs=runtime_dirs, loaded_config=loaded_config))
        self.addCleanup(client.close)
        return client, runtime_dirs

    def test_missing_template_config_is_empty_and_diagnosable(self) -> None:
        client, _ = self._client()

        templates = client.get("/detection/templates")
        state = client.get("/detection/state")

        self.assertEqual(templates.status_code, 200)
        self.assertFalse(templates.json()["config_loaded"])
        self.assertEqual(templates.json()["templates"], [])
        self.assertEqual(state.status_code, 200)
        self.assertEqual(state.json()["lifecycle_state"], "no_duel")

    def test_template_config_loads_local_templates(self) -> None:
        client, _ = self._configured_client()

        templates = client.get("/detection/templates")

        self.assertEqual(templates.status_code, 200)
        self.assertTrue(templates.json()["config_loaded"])
        self.assertEqual(len(templates.json()["templates"]), 2)
        self.assertEqual(templates.json()["errors"], [])

    def test_invalid_template_config_is_diagnosable(self) -> None:
        client, _ = self._client(config_text="[detection]\nstart_confirmations = 0\n")

        templates = client.get("/detection/templates")
        state = client.get("/detection/state")

        self.assertEqual(templates.status_code, 200)
        self.assertFalse(templates.json()["config_loaded"])
        self.assertEqual(state.status_code, 503)
        self.assertEqual(state.json()["code"], "detection_unavailable")
        self.assertEqual(state.json()["details"]["start_confirmations"], "must_be_positive")

    def test_duel_start_and_end_drive_recording_boundary(self) -> None:
        client, _ = self._configured_client()

        first = client.post("/detection/frame", json={"frame_text": "noise DUEL_START_MARKER"})
        second = client.post("/detection/frame", json={"frame_text": "again DUEL_START_MARKER"})

        self.assertEqual(first.status_code, 200)
        self.assertEqual(first.json()["lifecycle_state"], "potential_duel")
        self.assertEqual(second.status_code, 200)
        self.assertEqual(second.json()["lifecycle_state"], "active_duel")
        self.assertIn("duel_started", second.json()["events"])
        self.assertEqual(second.json()["recording_state"]["state"], "starting")

        confirmed = client.post("/recording/command", json={"action": "confirm_started", "source": "automatic"})
        self.assertEqual(confirmed.status_code, 200)
        self.assertEqual(confirmed.json()["state"], "recording")

        first_end = client.post("/detection/frame", json={"frame_text": "noise DUEL_END_MARKER"})
        second_end = client.post("/detection/frame", json={"frame_text": "again DUEL_END_MARKER"})

        self.assertEqual(first_end.status_code, 200)
        self.assertEqual(first_end.json()["lifecycle_state"], "active_duel")
        self.assertEqual(second_end.status_code, 200)
        self.assertEqual(second_end.json()["lifecycle_state"], "ended_duel")
        self.assertIn("duel_ended", second_end.json()["events"])
        self.assertEqual(second_end.json()["recording_state"]["state"], "stopping")

    def test_detection_payload_validation_is_stable(self) -> None:
        client, _ = self._configured_client()

        resp = client.post("/detection/frame", json={"frame_hex": "not hex"})

        self.assertEqual(resp.status_code, 400)
        self.assertEqual(resp.json()["code"], "detection_payload_invalid")
        self.assertEqual(resp.json()["details"]["frame_hex"], "must_be_hex")

    def _configured_client(self):
        return self._client(
            config_text="""
[detection]
start_confirmations = 2
end_confirmations = 2

[[templates]]
name = "start"
kind = "duel_start"
path = "start.tpl"

[[templates]]
name = "end"
kind = "duel_end"
path = "end.tpl"
""".strip(),
            templates={
                "start.tpl": "DUEL_START_MARKER",
                "end.tpl": "DUEL_END_MARKER",
            },
        )


if __name__ == "__main__":
    unittest.main()
