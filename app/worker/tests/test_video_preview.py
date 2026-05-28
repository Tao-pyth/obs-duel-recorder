from __future__ import annotations

import base64
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


WORKER_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WORKER_ROOT))


class VideoPreviewApiTests(unittest.TestCase):
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

    @mock.patch("odr_worker.video_preview.subprocess.run")
    @mock.patch("odr_worker.video_preview.shutil.which")
    def test_match_video_preview_extracts_representative_png(self, mock_which, mock_run) -> None:
        client, runtime_dirs = self._client()
        video_path = runtime_dirs.videos_dir / "duel.mp4"
        video_path.write_bytes(b"fake video")
        match_id = client.post("/matches", json={"opponent_deck": "Branded"}).json()["id"]
        client.post("/queue/items", json={"match_id": match_id, "video_path": "duel.mp4"})

        mock_which.side_effect = lambda name: f"/tools/{name}"
        mock_run.side_effect = [
            subprocess.CompletedProcess(args=["ffprobe"], returncode=0, stdout="12.0\n", stderr=""),
            subprocess.CompletedProcess(args=["ffmpeg"], returncode=0, stdout=b"\x89PNG\r\n\x1a\npreview", stderr=b""),
        ]

        response = client.get(f"/matches/{match_id}/video-preview", params={"frame": 2})

        self.assertEqual(response.status_code, 200)
        body = response.json()
        self.assertTrue(body["available"])
        self.assertEqual(body["frame_index"], 2)
        self.assertEqual(body["frame_count"], 3)
        self.assertEqual(body["content_type"], "image/png")
        self.assertEqual(base64.b64decode(body["content_base64"]), b"\x89PNG\r\n\x1a\npreview")
        self.assertEqual(mock_run.call_args_list[1].args[0][5], "6.000")

    def test_match_video_preview_reports_missing_video_link(self) -> None:
        client, _runtime_dirs = self._client()
        match_id = client.post("/matches", json={}).json()["id"]

        response = client.get(f"/matches/{match_id}/video-preview")

        self.assertEqual(response.status_code, 200)
        body = response.json()
        self.assertFalse(body["available"])
        self.assertEqual(body["reason"], "video_not_linked")
        self.assertEqual(body["frame_index"], 1)

    @mock.patch("odr_worker.video_preview.shutil.which", return_value=None)
    def test_match_video_preview_reports_missing_ffmpeg_without_mutation(self, _mock_which) -> None:
        client, runtime_dirs = self._client()
        video_path = runtime_dirs.videos_dir / "duel.mp4"
        video_path.write_bytes(b"fake video")
        match_id = client.post("/matches", json={}).json()["id"]
        client.post("/queue/items", json={"match_id": match_id, "video_path": "duel.mp4"})

        response = client.get(f"/matches/{match_id}/video-preview", params={"frame": 99})

        self.assertEqual(response.status_code, 200)
        body = response.json()
        self.assertFalse(body["available"])
        self.assertEqual(body["reason"], "ffmpeg_unavailable")
        self.assertEqual(body["frame_index"], 3)

    def test_match_video_preview_missing_match_is_404(self) -> None:
        client, _runtime_dirs = self._client()

        response = client.get("/matches/999/video-preview")

        self.assertEqual(response.status_code, 404)
        self.assertEqual(response.json()["code"], "match_not_found")


if __name__ == "__main__":
    unittest.main()
