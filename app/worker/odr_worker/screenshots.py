from __future__ import annotations

import base64
import datetime as _dt
import re
import sqlite3
from dataclasses import dataclass
from pathlib import Path
from typing import Any


SCREENSHOT_STATUSES = {"available", "missing", "deleted"}
CLEANUP_ALLOWED_QUEUE_STATES = {"uploaded", "discarded"}
CLEANUP_PRESERVE_QUEUE_STATES = {
    "ready_upload",
    "uploading",
    "upload_failed",
    "quota_waiting",
    "need_manual_review",
}


class ScreenshotError(ValueError):
    def __init__(self, code: str, details: dict[str, object]):
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class ScreenshotRecord:
    id: int
    match_id: int | None
    queue_item_id: int | None
    kind: str
    relative_path: str
    content_type: str
    size_bytes: int
    status: str
    created_at: str
    updated_at: str

    def as_payload(self) -> dict[str, object]:
        return {
            "id": self.id,
            "match_id": self.match_id,
            "queue_item_id": self.queue_item_id,
            "kind": self.kind,
            "relative_path": self.relative_path,
            "content_type": self.content_type,
            "size_bytes": self.size_bytes,
            "status": self.status,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
        }


class ScreenshotStore:
    def __init__(self, *, db_path: Path, screenshots_dir: Path):
        self.db_path = db_path
        self.screenshots_dir = screenshots_dir.resolve()

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA foreign_keys = ON;")
        return conn

    def capture(self, payload: dict[str, Any]) -> ScreenshotRecord:
        match_id = _optional_int(payload, "match_id")
        queue_item_id = _optional_int(payload, "queue_item_id")
        kind = _string(payload, "kind", "duel")
        content_type = _string(payload, "content_type", "application/octet-stream")
        captured_at = _string(payload, "captured_at", _utc_now_iso())
        extension = _extension(payload.get("extension"), content_type)
        content = _content_bytes(payload)
        if not content:
            raise ScreenshotError("screenshot_payload_invalid", {"content": "empty"})

        relative_path = _screenshot_name(
            match_id=match_id,
            queue_item_id=queue_item_id,
            kind=kind,
            captured_at=captured_at,
            extension=extension,
        )
        target = self._resolve_relative_path(relative_path)
        if target.exists():
            raise ScreenshotError("screenshot_path_conflict", {"relative_path": relative_path})
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)

        now = _utc_now_iso()
        conn = self._connect()
        try:
            cursor = conn.execute(
                """
                INSERT INTO screenshots(
                  match_id, queue_item_id, kind, relative_path, content_type,
                  size_bytes, status, created_at, updated_at
                ) VALUES(?, ?, ?, ?, ?, ?, 'available', ?, ?);
                """,
                (match_id, queue_item_id, kind, relative_path, content_type, len(content), now, now),
            )
            conn.commit()
            return self._get_record(conn, int(cursor.lastrowid))
        except sqlite3.IntegrityError as exc:
            try:
                target.unlink()
            except FileNotFoundError:
                pass
            raise ScreenshotError("screenshot_db_link_invalid", {"relative_path": relative_path}) from exc
        finally:
            conn.close()

    def list_records(self, *, match_id: int | None = None, queue_item_id: int | None = None) -> list[ScreenshotRecord]:
        conn = self._connect()
        try:
            if match_id is not None:
                rows = conn.execute(
                    "SELECT * FROM screenshots WHERE match_id = ? ORDER BY id;",
                    (match_id,),
                ).fetchall()
            elif queue_item_id is not None:
                rows = conn.execute(
                    "SELECT * FROM screenshots WHERE queue_item_id = ? ORDER BY id;",
                    (queue_item_id,),
                ).fetchall()
            else:
                rows = conn.execute("SELECT * FROM screenshots ORDER BY id;").fetchall()
            return [_record_from_row(row) for row in rows]
        finally:
            conn.close()

    def get_record(self, screenshot_id: int) -> ScreenshotRecord:
        conn = self._connect()
        try:
            return self._get_record(conn, screenshot_id)
        finally:
            conn.close()

    def preview(self, screenshot_id: int) -> dict[str, object]:
        conn = self._connect()
        try:
            record = self._get_record(conn, screenshot_id)
            path = self._resolve_relative_path(record.relative_path)
            if record.status != "available" or not path.exists():
                self._mark_status(conn, record.id, "missing" if record.status == "available" else record.status)
                conn.commit()
                return {"available": False, "record": self._get_record(conn, record.id).as_payload()}
            content = path.read_bytes()
            return {
                "available": True,
                "record": record.as_payload(),
                "content_base64": base64.b64encode(content).decode("ascii"),
                "content_type": record.content_type,
            }
        finally:
            conn.close()

    def cleanup_for_queue_item(self, queue_item_id: int) -> dict[str, object]:
        conn = self._connect()
        try:
            queue = conn.execute(
                "SELECT id, match_id, state FROM upload_queue WHERE id = ?;",
                (queue_item_id,),
            ).fetchone()
            if queue is None:
                raise ScreenshotError("queue_item_not_found", {"queue_item_id": queue_item_id})

            state = str(queue["state"])
            if state in CLEANUP_PRESERVE_QUEUE_STATES:
                return {"cleaned": [], "preserved": True, "reason": f"queue_state_{state}"}
            if state not in CLEANUP_ALLOWED_QUEUE_STATES:
                return {"cleaned": [], "preserved": True, "reason": f"queue_state_{state}_not_cleanup_safe"}

            rows = conn.execute(
                """
                SELECT * FROM screenshots
                WHERE (queue_item_id = ? OR (match_id IS NOT NULL AND match_id = ?))
                  AND status = 'available'
                ORDER BY id;
                """,
                (queue_item_id, queue["match_id"]),
            ).fetchall()
            cleaned: list[dict[str, object]] = []
            for row in rows:
                record = _record_from_row(row)
                path = self._resolve_relative_path(record.relative_path)
                if path.exists():
                    path.unlink()
                self._mark_status(conn, record.id, "deleted")
                cleaned.append({"id": record.id, "relative_path": record.relative_path, "status": "deleted"})
            conn.commit()
            return {"cleaned": cleaned, "preserved": False, "reason": f"queue_state_{state}"}
        finally:
            conn.close()

    def _get_record(self, conn: sqlite3.Connection, screenshot_id: int) -> ScreenshotRecord:
        row = conn.execute("SELECT * FROM screenshots WHERE id = ?;", (screenshot_id,)).fetchone()
        if row is None:
            raise ScreenshotError("screenshot_not_found", {"id": screenshot_id})
        return _record_from_row(row)

    def _mark_status(self, conn: sqlite3.Connection, screenshot_id: int, status: str) -> None:
        if status not in SCREENSHOT_STATUSES:
            raise ScreenshotError("screenshot_status_invalid", {"status": status})
        conn.execute(
            "UPDATE screenshots SET status = ?, updated_at = ? WHERE id = ?;",
            (status, _utc_now_iso(), screenshot_id),
        )

    def _resolve_relative_path(self, relative_path: str) -> Path:
        path = (self.screenshots_dir / relative_path).resolve()
        if self.screenshots_dir != path and self.screenshots_dir not in path.parents:
            raise ScreenshotError("screenshot_path_invalid", {"relative_path": relative_path})
        return path


def _record_from_row(row: sqlite3.Row) -> ScreenshotRecord:
    return ScreenshotRecord(
        id=int(row["id"]),
        match_id=row["match_id"],
        queue_item_id=row["queue_item_id"],
        kind=str(row["kind"]),
        relative_path=str(row["relative_path"]),
        content_type=str(row["content_type"]),
        size_bytes=int(row["size_bytes"]),
        status=str(row["status"]),
        created_at=str(row["created_at"]),
        updated_at=str(row["updated_at"]),
    )


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")


def _optional_int(payload: dict[str, Any], key: str) -> int | None:
    value = payload.get(key)
    if value is None:
        return None
    if not isinstance(value, int):
        raise ScreenshotError("screenshot_payload_invalid", {key: "must_be_integer_or_null"})
    return value


def _string(payload: dict[str, Any], key: str, default: str) -> str:
    value = payload.get(key, default)
    if not isinstance(value, str):
        raise ScreenshotError("screenshot_payload_invalid", {key: "must_be_string"})
    return value


def _content_bytes(payload: dict[str, Any]) -> bytes:
    if "content_base64" in payload:
        value = payload["content_base64"]
        if not isinstance(value, str):
            raise ScreenshotError("screenshot_payload_invalid", {"content_base64": "must_be_string"})
        try:
            return base64.b64decode(value, validate=True)
        except ValueError as exc:
            raise ScreenshotError("screenshot_payload_invalid", {"content_base64": "must_be_base64"}) from exc
    if "content_text" in payload:
        value = payload["content_text"]
        if not isinstance(value, str):
            raise ScreenshotError("screenshot_payload_invalid", {"content_text": "must_be_string"})
        return value.encode("utf-8")
    raise ScreenshotError("screenshot_payload_invalid", {"content": "content_base64_or_content_text_required"})


def _extension(value: object, content_type: str) -> str:
    if value is None:
        if content_type == "image/png":
            return "png"
        if content_type in {"image/jpeg", "image/jpg"}:
            return "jpg"
        return "bin"
    if not isinstance(value, str):
        raise ScreenshotError("screenshot_payload_invalid", {"extension": "must_be_string"})
    cleaned = value.lower().lstrip(".")
    if not re.fullmatch(r"[a-z0-9]{1,8}", cleaned):
        raise ScreenshotError("screenshot_payload_invalid", {"extension": "invalid_extension"})
    return cleaned


def _screenshot_name(
    *,
    match_id: int | None,
    queue_item_id: int | None,
    kind: str,
    captured_at: str,
    extension: str,
) -> str:
    safe_kind = re.sub(r"[^a-zA-Z0-9_-]+", "-", kind).strip("-").lower() or "screenshot"
    safe_time = re.sub(r"[^0-9A-Za-z]+", "", captured_at)[:20] or "unknown-time"
    match_part = f"match-{match_id}" if match_id is not None else "match-none"
    queue_part = f"queue-{queue_item_id}" if queue_item_id is not None else "queue-none"
    return f"{match_part}/{safe_time}-{safe_kind}-{queue_part}.{extension}"
