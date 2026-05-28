from __future__ import annotations

import base64
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class TemplateDetectionApiTests(unittest.TestCase):
    def _client(self, *, config_text: str | None = None, templates: dict[str, str | bytes] | None = None):
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
                target = templates_dir / name
                if isinstance(content, bytes):
                    target.write_bytes(content)
                else:
                    target.write_text(content, encoding="utf-8")

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

    def test_detection_reports_recording_transition_rejection(self) -> None:
        client, _ = self._configured_client()
        client.post("/recording/command", json={"action": "start", "source": "manual"})
        client.post("/recording/command", json={"action": "confirm_started", "source": "manual"})

        client.post("/detection/frame", json={"frame_text": "DUEL_START_MARKER"})
        result = client.post("/detection/frame", json={"frame_text": "DUEL_START_MARKER"})

        self.assertEqual(result.status_code, 200)
        self.assertIn("duel_started", result.json()["events"])
        self.assertIn("recording_start_skipped:active_or_unrecoverable_session_exists", result.json()["events"])
        self.assertEqual(result.json()["recording_state"]["state"], "recording")
        self.assertEqual(result.json()["recording_state"]["command_source"], "manual")

    def test_detection_payload_validation_is_stable(self) -> None:
        client, _ = self._configured_client()

        resp = client.post("/detection/frame", json={"frame_hex": "not hex"})

        self.assertEqual(resp.status_code, 400)
        self.assertEqual(resp.json()["code"], "detection_payload_invalid")
        self.assertEqual(resp.json()["details"]["frame_hex"], "must_be_hex")

        base64_resp = client.post("/detection/frame", json={"frame_base64": "not base64"})

        self.assertEqual(base64_resp.status_code, 400)
        self.assertEqual(base64_resp.json()["code"], "detection_payload_invalid")
        self.assertEqual(base64_resp.json()["details"]["frame_base64"], "must_be_base64")

    def test_detection_test_reports_matches_without_recording_transition(self) -> None:
        client, _ = self._configured_client()

        result = client.post("/detection/test", json={"kind": "start", "frame_text": "DUEL_START_MARKER"})
        state = client.get("/detection/state")
        recording = client.get("/recording/state")

        self.assertEqual(result.status_code, 200)
        self.assertTrue(result.json()["matched"])
        self.assertFalse(result.json()["state_changed"])
        self.assertFalse(result.json()["recording_command_sent"])
        self.assertEqual(result.json()["matches"][0]["name"], "start")
        self.assertEqual(result.json()["matches"][0]["kind"], "duel_start")
        self.assertEqual(result.json()["matches"][0]["score"], 1.0)
        self.assertEqual(state.json()["lifecycle_state"], "no_duel")
        self.assertEqual(recording.json()["state"], "idle")

    def test_detection_test_reports_missing_or_low_confidence_matches(self) -> None:
        client, _ = self._configured_client()

        result = client.post("/detection/test", json={"kind": "end", "frame_text": "no marker here"})

        self.assertEqual(result.status_code, 200)
        self.assertFalse(result.json()["matched"])
        self.assertEqual(result.json()["matches"][0]["score"], 0.0)
        self.assertIn(
            {"code": "template_match_missing", "kind": "duel_end", "best_score": 0.0},
            result.json()["diagnostics"],
        )

    def test_detection_test_reports_missing_templates_without_starting_recording(self) -> None:
        client, _ = self._client()

        result = client.post("/detection/test", json={"kind": "start", "frame_text": "DUEL_START_MARKER"})

        self.assertEqual(result.status_code, 200)
        self.assertFalse(result.json()["matched"])
        self.assertEqual(result.json()["matches"], [])
        self.assertIn("template_config_missing", {item["code"] for item in result.json()["diagnostics"]})
        self.assertIn("templates_not_configured", {item["code"] for item in result.json()["diagnostics"]})

    def test_png_template_test_matches_embedded_current_screen_capture(self) -> None:
        template_png = _png_bytes(width=2, height=2, rows=[[(255, 0, 0)] * 2, [(255, 0, 0)] * 2])
        frame_png = _png_bytes(
            width=4,
            height=4,
            rows=[
                [(0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0)],
                [(0, 0, 0), (255, 0, 0), (255, 0, 0), (0, 0, 0)],
                [(0, 0, 0), (255, 0, 0), (255, 0, 0), (0, 0, 0)],
                [(0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0)],
            ],
        )
        client, _ = self._client(
            config_text="""
[detection]
start_confirmations = 2
end_confirmations = 2

[[templates]]
name = "start"
kind = "duel_start"
path = "start.png"
threshold = 1.0
""".strip(),
            templates={"start.png": template_png},
        )

        result = client.post(
            "/detection/test",
            json={"kind": "start", "frame_base64": base64.b64encode(frame_png).decode("ascii")},
        )

        self.assertEqual(result.status_code, 200)
        self.assertTrue(result.json()["matched"])
        self.assertEqual(result.json()["matches"][0]["score"], 1.0)
        self.assertFalse(result.json()["state_changed"])

    def test_png_template_frames_drive_detection_lifecycle(self) -> None:
        template_png = _png_bytes(width=1, height=1, rows=[[(255, 255, 255)]])
        frame_png = _png_bytes(width=2, height=1, rows=[[(0, 0, 0), (255, 255, 255)]])
        client, _ = self._client(
            config_text="""
[detection]
start_confirmations = 2
end_confirmations = 2

[[templates]]
name = "start"
kind = "duel_start"
path = "start.png"
threshold = 1.0
""".strip(),
            templates={"start.png": template_png},
        )

        payload = {"frame_base64": base64.b64encode(frame_png).decode("ascii")}
        first = client.post("/detection/frame", json=payload)
        second = client.post("/detection/frame", json=payload)

        self.assertEqual(first.status_code, 200)
        self.assertEqual(first.json()["lifecycle_state"], "potential_duel")
        self.assertEqual(second.status_code, 200)
        self.assertEqual(second.json()["lifecycle_state"], "active_duel")
        self.assertIn("duel_started", second.json()["events"])

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


def _png_bytes(*, width: int, height: int, rows: list[list[tuple[int, int, int]]]) -> bytes:
    raw_rows = bytearray()
    for row in rows:
        raw_rows.append(0)
        for red, green, blue in row:
            raw_rows.extend((red, green, blue))

    def chunk(kind: bytes, data: bytes) -> bytes:
        checksum = zlib.crc32(kind)
        checksum = zlib.crc32(data, checksum) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", checksum)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw_rows))) + chunk(b"IEND", b"")


if __name__ == "__main__":
    unittest.main()
