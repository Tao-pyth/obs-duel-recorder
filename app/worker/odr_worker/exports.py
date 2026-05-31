from __future__ import annotations

import datetime as _dt
import csv
import json
import re
import sqlite3
import tempfile
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .health import API_VERSION
from .runtime_dirs import RuntimeDirs
from .version import __version__


EXPORT_VERSION = "1.2"


class ExportError(ValueError):
    def __init__(self, code: str, details: dict[str, object]):
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class ExportRecord:
    file_name: str
    path: str
    size_bytes: int
    updated_at: str

    def as_payload(self) -> dict[str, object]:
        return {
            "file_name": self.file_name,
            "path": self.path,
            "size_bytes": self.size_bytes,
            "updated_at": self.updated_at,
        }


class ExportStore:
    def __init__(self, *, db_path: Path, runtime_dirs: RuntimeDirs):
        self.db_path = db_path
        self.runtime_dirs = runtime_dirs
        self.exports_dir = runtime_dirs.exports_dir.resolve()
        self.screenshots_dir = runtime_dirs.screenshots_dir.resolve()
        self.videos_dir = runtime_dirs.videos_dir.resolve()

    def list_exports(self) -> list[ExportRecord]:
        records: list[ExportRecord] = []
        for path in sorted(self.exports_dir.glob("*.zip")):
            stat = path.stat()
            records.append(
                ExportRecord(
                    file_name=path.name,
                    path=path.resolve().as_posix(),
                    size_bytes=stat.st_size,
                    updated_at=_timestamp_from_stat(stat.st_mtime),
                )
            )
        return records

    def create_export(self, payload: dict[str, Any]) -> dict[str, object]:
        if not isinstance(payload, dict):
            raise ExportError("export_payload_invalid", {"payload": "must_be_object"})

        created_at = _string(payload, "created_at", _utc_now_iso())
        include_videos = _bool(payload, "include_videos", False)
        file_name = _export_file_name(payload.get("name"), created_at)
        target = self._resolve_export_path(file_name)
        if target.exists():
            raise ExportError("export_path_conflict", {"file_name": file_name})

        tmp_zip = target.with_name(f".{target.name}.tmp")
        if tmp_zip.exists():
            tmp_zip.unlink()

        try:
            with tempfile.TemporaryDirectory(dir=self.exports_dir) as tmp_dir_name:
                tmp_dir = Path(tmp_dir_name)
                snapshot_path = tmp_dir / "odr.sqlite3"
                self._snapshot_db(snapshot_path)
                metadata = self._metadata(snapshot_path)
                manifest = self._manifest(
                    file_name=file_name,
                    created_at=created_at,
                    include_videos=include_videos,
                    metadata=metadata,
                )
                self._write_zip(
                    zip_path=tmp_zip,
                    snapshot_path=snapshot_path,
                    metadata=metadata,
                    manifest=manifest,
                    include_videos=include_videos,
                )
            tmp_zip.replace(target)
        except Exception as exc:
            if tmp_zip.exists():
                tmp_zip.unlink()
            if isinstance(exc, ExportError):
                raise
            raise ExportError("export_failed", {"reason": exc.__class__.__name__}) from exc

        stat = target.stat()
        return {
            "status": "completed",
            "file_name": file_name,
            "path": target.resolve().as_posix(),
            "size_bytes": stat.st_size,
            "manifest": manifest,
        }

    def create_registration_csv(self, payload: dict[str, Any]) -> dict[str, object]:
        if not isinstance(payload, dict):
            raise ExportError("export_payload_invalid", {"payload": "must_be_object"})

        save_dir = _string(payload, "save_dir", str(self.exports_dir))
        target_dir = Path(save_dir).expanduser().resolve()
        if target_dir.exists() and not target_dir.is_dir():
            raise ExportError("csv_export_dir_invalid", {"save_dir": "must_be_directory"})
        target_dir.mkdir(parents=True, exist_ok=True)

        created_at = _local_now_compact()
        file_name = f"odr-registration-{created_at}.csv"
        target = (target_dir / file_name).resolve()
        if target.parent != target_dir:
            raise ExportError("csv_export_path_invalid", {"file_name": file_name})

        rows = self._registration_rows()
        fieldnames = [
            "match_id",
            "recording_session_id",
            "created_at",
            "updated_at",
            "started_at",
            "ended_at",
            "deck_name",
            "deck_sequence_number",
            "opponent_deck",
            "result",
            "rank",
            "dp",
            "memo",
            "queue_item_id",
            "queue_status",
            "video_path",
            "youtube_video_id",
            "youtube_url",
            "last_error_code",
            "last_error_message",
            "manual_review_reason",
        ]
        with target.open("w", encoding="utf-8-sig", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
            writer.writeheader()
            writer.writerows(rows)

        stat = target.stat()
        return {
            "status": "completed",
            "file_name": file_name,
            "path": target.as_posix(),
            "row_count": len(rows),
            "size_bytes": stat.st_size,
        }

    def _snapshot_db(self, snapshot_path: Path) -> None:
        source = sqlite3.connect(self.db_path)
        try:
            target = sqlite3.connect(snapshot_path)
            try:
                source.backup(target)
            finally:
                target.close()
        finally:
            source.close()

    def _metadata(self, snapshot_path: Path) -> dict[str, object]:
        conn = sqlite3.connect(snapshot_path)
        conn.row_factory = sqlite3.Row
        try:
            return {
                "schema_version": _schema_version(conn),
                "matches": _table_rows(conn, "matches"),
                "upload_queue": _table_rows(conn, "upload_queue"),
                "screenshots": _table_rows(conn, "screenshots"),
            }
        finally:
            conn.close()

    def _manifest(
        self,
        *,
        file_name: str,
        created_at: str,
        include_videos: bool,
        metadata: dict[str, object],
    ) -> dict[str, object]:
        screenshots = list(metadata["screenshots"])
        queue_items = list(metadata["upload_queue"])
        included_artifacts: list[dict[str, object]] = [
            {"kind": "database", "path": "database/odr.sqlite3"},
            {"kind": "metadata", "path": "metadata/matches.json"},
            {"kind": "metadata", "path": "metadata/upload_queue.json"},
            {"kind": "metadata", "path": "metadata/screenshots.json"},
            {"kind": "metadata", "path": "metadata/video_linkages.json"},
        ]
        missing_files: list[dict[str, object]] = []

        screenshot_count = 0
        for row in screenshots:
            relative_path = str(row.get("relative_path", ""))
            if row.get("status") == "available" and relative_path:
                source = self._resolve_screenshot_path(relative_path)
                if source.exists():
                    screenshot_count += 1
                    included_artifacts.append({"kind": "screenshot", "path": f"screenshots/{relative_path}"})
                else:
                    missing_files.append(
                        {"kind": "screenshot", "id": row.get("id"), "relative_path": relative_path}
                    )

        video_count = 0
        videos_included = 0
        for row in queue_items:
            video_path = str(row.get("video_path", ""))
            if not video_path:
                continue
            video_count += 1
            source = self._resolve_video_path(video_path)
            if not source.exists():
                missing_files.append({"kind": "video", "queue_item_id": row.get("id"), "path": video_path})
            elif include_videos:
                videos_included += 1
                included_artifacts.append({"kind": "video", "path": _video_archive_path(row, source)})

        return {
            "export_version": EXPORT_VERSION,
            "created_at": created_at,
            "file_name": file_name,
            "app_version": __version__,
            "api_version": API_VERSION,
            "schema_version": metadata["schema_version"],
            "include_videos": include_videos,
            "included_artifacts": included_artifacts,
            "missing_files": missing_files,
            "counts": {
                "matches": len(metadata["matches"]),
                "upload_queue": len(queue_items),
                "screenshots": len(screenshots),
                "screenshots_included": screenshot_count,
                "video_linkages": video_count,
                "videos_included": videos_included,
            },
            "exclusions": {
                "config": "excluded",
                "logs": "excluded",
                "oauth_tokens": "excluded",
                "client_secrets": "excluded",
                "temporary_files": "excluded",
                "videos_default": "linkage_only",
            },
        }

    def _write_zip(
        self,
        *,
        zip_path: Path,
        snapshot_path: Path,
        metadata: dict[str, object],
        manifest: dict[str, object],
        include_videos: bool,
    ) -> None:
        queue_items = list(metadata["upload_queue"])
        with zipfile.ZipFile(zip_path, mode="w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.write(snapshot_path, "database/odr.sqlite3")
            archive.writestr("manifest.json", _json_bytes(manifest))
            archive.writestr("metadata/matches.json", _json_bytes(metadata["matches"]))
            archive.writestr("metadata/upload_queue.json", _json_bytes(queue_items))
            archive.writestr("metadata/screenshots.json", _json_bytes(metadata["screenshots"]))
            archive.writestr("metadata/video_linkages.json", _json_bytes(_video_linkages(queue_items)))

            for row in metadata["screenshots"]:
                relative_path = str(row.get("relative_path", ""))
                if row.get("status") != "available" or not relative_path:
                    continue
                source = self._resolve_screenshot_path(relative_path)
                if source.exists():
                    archive.write(source, f"screenshots/{relative_path}")

            if include_videos:
                for row in queue_items:
                    video_path = str(row.get("video_path", ""))
                    if not video_path:
                        continue
                    source = self._resolve_video_path(video_path)
                    if source.exists():
                        archive.write(source, _video_archive_path(row, source))

    def _resolve_export_path(self, file_name: str) -> Path:
        path = (self.exports_dir / file_name).resolve()
        if self.exports_dir != path.parent:
            raise ExportError("export_path_invalid", {"file_name": file_name})
        return path

    def _resolve_screenshot_path(self, relative_path: str) -> Path:
        path = (self.screenshots_dir / relative_path).resolve()
        if self.screenshots_dir != path and self.screenshots_dir not in path.parents:
            raise ExportError("export_screenshot_path_invalid", {"relative_path": relative_path})
        return path

    def _resolve_video_path(self, video_path: str) -> Path:
        path = Path(video_path)
        if not path.is_absolute():
            path = self.videos_dir / path
        return path.resolve()

    def _registration_rows(self) -> list[dict[str, object]]:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        try:
            rows = conn.execute(
                """
                SELECT
                  matches.id AS match_id,
                  matches.recording_session_id AS recording_session_id,
                  matches.created_at AS created_at,
                  matches.updated_at AS updated_at,
                  matches.started_at AS started_at,
                  matches.ended_at AS ended_at,
                  matches.deck_name AS deck_name,
                  matches.deck_sequence_number AS deck_sequence_number,
                  matches.opponent_deck AS opponent_deck,
                  matches.result AS result,
                  matches.rank AS rank,
                  matches.dp AS dp,
                  matches.memo AS memo,
                  upload_queue.id AS queue_item_id,
                  upload_queue.state AS queue_status,
                  upload_queue.video_path AS video_path,
                  upload_queue.youtube_video_id AS youtube_video_id,
                  upload_queue.youtube_url AS youtube_url,
                  upload_queue.last_error_code AS last_error_code,
                  upload_queue.last_error_message AS last_error_message,
                  upload_queue.manual_review_reason AS manual_review_reason
                FROM matches
                LEFT JOIN upload_queue ON upload_queue.match_id = matches.id
                ORDER BY matches.id, upload_queue.id;
                """
            ).fetchall()
            return [{key: row[key] if row[key] is not None else "" for key in row.keys()} for row in rows]
        finally:
            conn.close()


def _table_rows(conn: sqlite3.Connection, table: str) -> list[dict[str, object]]:
    rows = conn.execute(f"SELECT * FROM {table} ORDER BY id;").fetchall()
    return [{key: row[key] for key in row.keys()} for row in rows]


def _schema_version(conn: sqlite3.Connection) -> int:
    row = conn.execute("SELECT value FROM odr_meta WHERE key = 'schema_version';").fetchone()
    if row is None:
        return 0
    try:
        return int(row["value"])
    except (TypeError, ValueError):
        return 0


def _video_linkages(queue_items: list[dict[str, object]]) -> list[dict[str, object]]:
    return [
        {
            "queue_item_id": row.get("id"),
            "match_id": row.get("match_id"),
            "state": row.get("state"),
            "video_path": row.get("video_path"),
            "youtube_video_id": row.get("youtube_video_id"),
            "youtube_url": row.get("youtube_url"),
        }
        for row in queue_items
    ]


def _video_archive_path(row: dict[str, object], source: Path) -> str:
    queue_id = row.get("id") or "unknown"
    safe_name = re.sub(r"[^A-Za-z0-9._-]+", "-", source.name).strip("-") or "video.bin"
    return f"videos/queue-{queue_id}-{safe_name}"


def _export_file_name(value: object, created_at: str) -> str:
    if value is None:
        stem = f"odr-export-{_safe_timestamp(created_at)}"
    else:
        if not isinstance(value, str):
            raise ExportError("export_payload_invalid", {"name": "must_be_string"})
        stem = value.strip()
        if not stem:
            raise ExportError("export_payload_invalid", {"name": "must_not_be_empty"})
        stem = stem[:-4] if stem.lower().endswith(".zip") else stem
        stem = re.sub(r"[^A-Za-z0-9._-]+", "-", stem).strip(".-")
        if not stem:
            raise ExportError("export_payload_invalid", {"name": "invalid"})
    if len(stem) > 120:
        raise ExportError("export_payload_invalid", {"name": "max_length_120"})
    return f"{stem}.zip"


def _safe_timestamp(value: str) -> str:
    safe = re.sub(r"[^0-9A-Za-z]+", "", value)[:20]
    return safe or "unknown-time"


def _string(payload: dict[str, Any], key: str, default: str) -> str:
    value = payload.get(key, default)
    if not isinstance(value, str):
        raise ExportError("export_payload_invalid", {key: "must_be_string"})
    return value


def _bool(payload: dict[str, Any], key: str, default: bool) -> bool:
    value = payload.get(key, default)
    if not isinstance(value, bool):
        raise ExportError("export_payload_invalid", {key: "must_be_boolean"})
    return value


def _json_bytes(value: object) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2).encode("utf-8")


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")


def _local_now_compact() -> str:
    return _dt.datetime.now().strftime("%Y%m%d-%H%M%S")


def _timestamp_from_stat(timestamp: float) -> str:
    return _dt.datetime.fromtimestamp(timestamp, tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")
