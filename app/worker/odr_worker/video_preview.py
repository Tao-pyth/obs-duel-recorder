from __future__ import annotations

import base64
import shutil
import subprocess
from pathlib import Path


FRAME_COUNT = 3
FRAME_FRACTIONS = {
    1: 0.25,
    2: 0.50,
    3: 0.75,
}


def build_video_preview_payload(*, video_path: str, videos_dir: Path, frame: int = 1) -> dict[str, object]:
    """Return a representative video frame as PNG base64 when local ffmpeg is available."""

    frame_index = _frame_index(frame)
    resolved = _resolve_video_path(video_path=video_path, videos_dir=videos_dir)
    if resolved is None:
        return _unavailable(frame_index=frame_index, reason="video_not_linked", video_path="")
    if not resolved.exists() or not resolved.is_file():
        return _unavailable(frame_index=frame_index, reason="video_missing", video_path=str(resolved))

    ffmpeg = shutil.which("ffmpeg")
    ffprobe = shutil.which("ffprobe")
    if not ffmpeg:
        return _unavailable(frame_index=frame_index, reason="ffmpeg_unavailable", video_path=str(resolved))

    duration = _probe_duration(ffprobe=ffprobe, video_path=resolved)
    seek_seconds = _seek_seconds(duration=duration, frame_index=frame_index)
    frame_png = _extract_frame_png(ffmpeg=ffmpeg, video_path=resolved, seek_seconds=seek_seconds)
    if frame_png is None:
        return _unavailable(frame_index=frame_index, reason="frame_extract_failed", video_path=str(resolved))

    return {
        "available": True,
        "reason": "",
        "frame_index": frame_index,
        "frame_count": FRAME_COUNT,
        "video_path": str(resolved),
        "content_type": "image/png",
        "content_base64": base64.b64encode(frame_png).decode("ascii"),
    }


def _unavailable(*, frame_index: int, reason: str, video_path: str) -> dict[str, object]:
    return {
        "available": False,
        "reason": reason,
        "frame_index": frame_index,
        "frame_count": FRAME_COUNT,
        "video_path": video_path,
        "content_type": "",
        "content_base64": "",
    }


def _frame_index(frame: int) -> int:
    if frame < 1:
        return 1
    if frame > FRAME_COUNT:
        return FRAME_COUNT
    return frame


def _resolve_video_path(*, video_path: str, videos_dir: Path) -> Path | None:
    value = video_path.strip()
    if not value:
        return None
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = videos_dir / path
    return path.resolve()


def _probe_duration(*, ffprobe: str | None, video_path: Path) -> float:
    if not ffprobe:
        return 0.0
    try:
        completed = subprocess.run(
            [
                ffprobe,
                "-v",
                "error",
                "-show_entries",
                "format=duration",
                "-of",
                "default=noprint_wrappers=1:nokey=1",
                str(video_path),
            ],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return 0.0
    if completed.returncode != 0:
        return 0.0
    try:
        return max(0.0, float(completed.stdout.strip()))
    except ValueError:
        return 0.0


def _seek_seconds(*, duration: float, frame_index: int) -> float:
    if duration <= 0:
        return float(frame_index)
    return max(0.0, duration * FRAME_FRACTIONS.get(frame_index, 0.5))


def _extract_frame_png(*, ffmpeg: str, video_path: Path, seek_seconds: float) -> bytes | None:
    try:
        completed = subprocess.run(
            [
                ffmpeg,
                "-hide_banner",
                "-loglevel",
                "error",
                "-ss",
                f"{seek_seconds:.3f}",
                "-i",
                str(video_path),
                "-frames:v",
                "1",
                "-vf",
                "scale=480:-1",
                "-f",
                "image2pipe",
                "-vcodec",
                "png",
                "pipe:1",
            ],
            capture_output=True,
            timeout=10,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if completed.returncode != 0 or not completed.stdout:
        return None
    return completed.stdout
