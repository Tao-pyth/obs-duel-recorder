from __future__ import annotations

import datetime as _dt
import json
import re
import sqlite3
from dataclasses import dataclass
from pathlib import Path
from typing import Any


QUEUE_STATES = {
    "ready_upload",
    "uploading",
    "uploaded",
    "upload_failed",
    "quota_waiting",
    "need_manual_review",
    "discarded",
}

TERMINAL_STATES = {"uploaded", "discarded"}
RESUMABLE_STATES = {"ready_upload", "upload_failed"}
YOUTUBE_VIDEO_ID_RE = re.compile(r"^[A-Za-z0-9_-]{3,64}$")


class QueueCommandError(ValueError):
    def __init__(self, code: str, details: dict[str, object]):
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class QueueItem:
    id: int
    match_id: int | None
    state: str
    video_path: str
    youtube_video_id: str
    youtube_url: str
    retry_count: int
    max_retries: int
    next_attempt_at: str
    last_error_code: str
    last_error_message: str
    manual_review_reason: str
    manual_review_evidence: dict[str, object]
    created_at: str
    updated_at: str

    def as_payload(self) -> dict[str, object]:
        return {
            "id": self.id,
            "match_id": self.match_id,
            "state": self.state,
            "video_path": self.video_path,
            "youtube_video_id": self.youtube_video_id,
            "youtube_url": self.youtube_url,
            "retry_count": self.retry_count,
            "max_retries": self.max_retries,
            "next_attempt_at": self.next_attempt_at,
            "last_error_code": self.last_error_code,
            "last_error_message": self.last_error_message,
            "manual_review_reason": self.manual_review_reason,
            "manual_review_evidence": self.manual_review_evidence,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
        }


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")


def _parse_json_object(value: str) -> dict[str, object]:
    try:
        parsed = json.loads(value or "{}")
    except json.JSONDecodeError:
        return {"raw": value}
    if isinstance(parsed, dict):
        return parsed
    return {"value": parsed}


def _compact_evidence(value: Any) -> str:
    if value is None:
        return "{}"
    if not isinstance(value, dict):
        raise QueueCommandError("queue_payload_invalid", {"manual_review_evidence": "must_be_object"})
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def _string_field(payload: dict[str, Any], key: str, default: str = "") -> str:
    value = payload.get(key, default)
    if value is None:
        return default
    if not isinstance(value, str):
        raise QueueCommandError("queue_payload_invalid", {key: "must_be_string"})
    return value


def _int_field(payload: dict[str, Any], key: str, default: int) -> int:
    value = payload.get(key, default)
    if value is None:
        return default
    if not isinstance(value, int):
        raise QueueCommandError("queue_payload_invalid", {key: "must_be_integer"})
    return value


def _optional_int_field(payload: dict[str, Any], key: str) -> int | None:
    value = payload.get(key)
    if value is None:
        return None
    if not isinstance(value, int):
        raise QueueCommandError("queue_payload_invalid", {key: "must_be_integer_or_null"})
    return value


class QueueStore:
    def __init__(self, db_path: Path):
        self.db_path = db_path

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA foreign_keys = ON;")
        conn.execute("PRAGMA busy_timeout = 5000;")
        return conn

    def create_item(self, payload: dict[str, Any]) -> QueueItem:
        match_id = _optional_int_field(payload, "match_id")
        video_path = _string_field(payload, "video_path")
        max_retries = _int_field(payload, "max_retries", 3)
        if max_retries < 0:
            raise QueueCommandError("queue_payload_invalid", {"max_retries": "must_be_non_negative"})

        now = _utc_now_iso()
        conn = self._connect()
        try:
            cursor = conn.execute(
                """
                INSERT INTO upload_queue(
                  match_id, state, video_path, max_retries, updated_at
                ) VALUES(?, 'ready_upload', ?, ?, ?);
                """,
                (match_id, video_path, max_retries, now),
            )
            item_id = int(cursor.lastrowid)
            conn.commit()
            return self._get_item(conn, item_id)
        finally:
            conn.close()

    def get_or_create_ready_item_for_match(self, *, match_id: int, video_path: str) -> QueueItem:
        if not video_path:
            raise QueueCommandError("queue_payload_invalid", {"video_path": "required"})

        now = _utc_now_iso()
        conn = self._connect()
        try:
            existing = conn.execute(
                "SELECT * FROM upload_queue WHERE match_id = ? ORDER BY id LIMIT 1;",
                (match_id,),
            ).fetchone()
            if existing is not None:
                return _item_from_row(existing)

            cursor = conn.execute(
                """
                INSERT INTO upload_queue(
                  match_id, state, video_path, updated_at
                ) VALUES(?, 'ready_upload', ?, ?);
                """,
                (match_id, video_path, now),
            )
            conn.commit()
            return self._get_item(conn, int(cursor.lastrowid))
        finally:
            conn.close()

    def get_item_for_match(self, match_id: int) -> QueueItem | None:
        conn = self._connect()
        try:
            row = conn.execute(
                "SELECT * FROM upload_queue WHERE match_id = ? ORDER BY id LIMIT 1;",
                (match_id,),
            ).fetchone()
            return _item_from_row(row) if row is not None else None
        finally:
            conn.close()

    def list_items(self, state: str | None = None) -> list[QueueItem]:
        conn = self._connect()
        try:
            if state:
                if state not in QUEUE_STATES:
                    raise QueueCommandError("queue_payload_invalid", {"state": "unknown_state"})
                rows = conn.execute(
                    "SELECT * FROM upload_queue WHERE state = ? ORDER BY id;",
                    (state,),
                ).fetchall()
            else:
                rows = conn.execute("SELECT * FROM upload_queue ORDER BY id;").fetchall()
            return [_item_from_row(row) for row in rows]
        finally:
            conn.close()

    def count_by_state(self) -> dict[str, int]:
        counts = {state: 0 for state in sorted(QUEUE_STATES)}
        conn = self._connect()
        try:
            rows = conn.execute(
                "SELECT state, COUNT(*) AS count FROM upload_queue GROUP BY state ORDER BY state;"
            ).fetchall()
            for row in rows:
                state = str(row["state"])
                if state in counts:
                    counts[state] = int(row["count"])
            return counts
        finally:
            conn.close()

    def next_ready_item(self) -> QueueItem | None:
        conn = self._connect()
        try:
            row = conn.execute(
                "SELECT * FROM upload_queue WHERE state = 'ready_upload' ORDER BY id LIMIT 1;"
            ).fetchone()
            return _item_from_row(row) if row is not None else None
        finally:
            conn.close()

    def get_item(self, item_id: int) -> QueueItem:
        conn = self._connect()
        try:
            return self._get_item(conn, item_id)
        finally:
            conn.close()

    def apply_command(self, item_id: int, payload: dict[str, Any]) -> QueueItem:
        action = _string_field(payload, "action")
        now = _utc_now_iso()

        conn = self._connect()
        try:
            item = self._get_item(conn, item_id)
            params: dict[str, object] = {"updated_at": now}

            if action == "start_upload":
                self._require_state(item, RESUMABLE_STATES, action)
                state = "uploading"
            elif action == "mark_uploaded":
                self._require_state(item, {"uploading", "need_manual_review"}, action)
                state = "uploaded"
                params["youtube_video_id"] = _string_field(payload, "youtube_video_id")
                if not params["youtube_video_id"]:
                    raise QueueCommandError("queue_payload_invalid", {"youtube_video_id": "required"})
                if not _is_valid_youtube_video_id(params["youtube_video_id"]):
                    raise QueueCommandError(
                        "queue_payload_invalid",
                        {"youtube_video_id": "must_be_plain_video_id"},
                    )
                params["youtube_url"] = _string_field(
                    payload,
                    "youtube_url",
                    f"https://youtu.be/{params['youtube_video_id']}",
                )
            elif action == "mark_upload_failed":
                self._require_state(item, {"uploading", "ready_upload", "upload_failed"}, action)
                state, params = self._failed_state(item, payload, now)
            elif action == "mark_quota_waiting":
                self._require_state(item, {"uploading", "ready_upload", "upload_failed"}, action)
                state = "quota_waiting"
                params["next_attempt_at"] = _string_field(payload, "next_attempt_at")
                params["last_error_code"] = _string_field(payload, "error_code", "quota_exceeded")
                params["last_error_message"] = _string_field(payload, "error_message")
            elif action == "mark_need_manual_review":
                self._require_state(item, QUEUE_STATES - TERMINAL_STATES, action)
                state = "need_manual_review"
                params["manual_review_reason"] = _string_field(payload, "reason", "manual_review_required")
                params["manual_review_evidence_json"] = _compact_evidence(payload.get("manual_review_evidence"))
            elif action == "retry":
                self._require_state(item, {"upload_failed", "quota_waiting", "need_manual_review"}, action)
                state = "ready_upload"
                params["next_attempt_at"] = ""
                params["manual_review_reason"] = _string_field(payload, "reason", item.manual_review_reason)
                if "manual_review_evidence" in payload:
                    params["manual_review_evidence_json"] = _compact_evidence(payload.get("manual_review_evidence"))
            elif action == "discard":
                self._require_state(item, QUEUE_STATES - TERMINAL_STATES, action)
                state = "discarded"
                params["manual_review_reason"] = _string_field(payload, "reason", item.manual_review_reason)
                if "manual_review_evidence" in payload:
                    params["manual_review_evidence_json"] = _compact_evidence(payload.get("manual_review_evidence"))
            else:
                raise QueueCommandError("queue_payload_invalid", {"action": "unknown_action"})

            params["state"] = state
            self._update_item(conn, item.id, params)
            conn.commit()
            return self._get_item(conn, item.id)
        finally:
            conn.close()

    def recover_startup(self) -> dict[str, object]:
        started_at = _dt.datetime.now(tz=_dt.timezone.utc)
        recovered: list[dict[str, object]] = []
        conn = self._connect()
        try:
            rows = conn.execute("SELECT * FROM upload_queue WHERE state = 'uploading' ORDER BY id;").fetchall()
            scanned_count = len(rows)
            for row in rows:
                item = _item_from_row(row)
                now = _utc_now_iso()
                if item.youtube_video_id:
                    state = "uploaded"
                    reason = "success_marker_present"
                    params = {"state": state, "updated_at": now}
                elif item.video_path and Path(item.video_path).is_absolute() and not Path(item.video_path).exists():
                    state = "discarded"
                    reason = "local_video_missing"
                    params = {
                        "state": state,
                        "manual_review_reason": reason,
                        "manual_review_evidence_json": _compact_evidence({"video_path": item.video_path}),
                        "updated_at": now,
                    }
                else:
                    state = "need_manual_review"
                    reason = "interrupted_upload_requires_manual_review"
                    params = {
                        "state": state,
                        "manual_review_reason": reason,
                        "manual_review_evidence_json": _compact_evidence(
                            {
                                "queue_item_id": item.id,
                                "previous_state": "uploading",
                                "video_path": item.video_path,
                                "retry_count": item.retry_count,
                                "last_error_code": item.last_error_code,
                            }
                        ),
                        "updated_at": now,
                    }
                self._update_item(conn, item.id, params)
                recovered.append({"id": item.id, "from": "uploading", "to": state, "reason": reason})
            conn.commit()
        finally:
            conn.close()
        elapsed = _dt.datetime.now(tz=_dt.timezone.utc) - started_at
        return {
            "recovered": recovered,
            "scanned_count": scanned_count,
            "recovered_count": len(recovered),
            "duration_ms": round(elapsed.total_seconds() * 1000, 3),
        }

    def _failed_state(self, item: QueueItem, payload: dict[str, Any], now: str) -> tuple[str, dict[str, object]]:
        retry_count = item.retry_count + 1
        max_retries = _int_field(payload, "max_retries", item.max_retries)
        error_code = _string_field(payload, "error_code", "upload_failed")
        error_message = _string_field(payload, "error_message")
        next_attempt_at = _string_field(payload, "next_attempt_at")
        params: dict[str, object] = {
            "retry_count": retry_count,
            "max_retries": max_retries,
            "last_error_code": error_code,
            "last_error_message": error_message,
            "next_attempt_at": next_attempt_at,
            "updated_at": now,
        }
        if retry_count > max_retries:
            params["manual_review_reason"] = "retry_limit_exceeded"
            params["manual_review_evidence_json"] = _compact_evidence(
                {
                    "retry_count": retry_count,
                    "max_retries": max_retries,
                    "last_error_code": error_code,
                    "last_error_message": error_message,
                }
            )
            return "need_manual_review", params
        return "upload_failed", params

    def _get_item(self, conn: sqlite3.Connection, item_id: int) -> QueueItem:
        row = conn.execute("SELECT * FROM upload_queue WHERE id = ?;", (item_id,)).fetchone()
        if row is None:
            raise QueueCommandError("queue_item_not_found", {"id": item_id})
        return _item_from_row(row)

    def _require_state(self, item: QueueItem, allowed: set[str], action: str) -> None:
        if item.state not in allowed:
            raise QueueCommandError(
                "queue_transition_invalid",
                {"id": item.id, "state": item.state, "action": action},
            )

    def _update_item(self, conn: sqlite3.Connection, item_id: int, params: dict[str, object]) -> None:
        assignments = ", ".join(f"{key} = :{key}" for key in params)
        params["id"] = item_id
        conn.execute(f"UPDATE upload_queue SET {assignments} WHERE id = :id;", params)


def _item_from_row(row: sqlite3.Row) -> QueueItem:
    return QueueItem(
        id=int(row["id"]),
        match_id=row["match_id"],
        state=str(row["state"]),
        video_path=str(row["video_path"]),
        youtube_video_id=str(row["youtube_video_id"]),
        youtube_url=str(row["youtube_url"]),
        retry_count=int(row["retry_count"]),
        max_retries=int(row["max_retries"]),
        next_attempt_at=str(row["next_attempt_at"]),
        last_error_code=str(row["last_error_code"]),
        last_error_message=str(row["last_error_message"]),
        manual_review_reason=str(row["manual_review_reason"]),
        manual_review_evidence=_parse_json_object(str(row["manual_review_evidence_json"])),
        created_at=str(row["created_at"]),
        updated_at=str(row["updated_at"]),
    )


def _is_valid_youtube_video_id(value: str) -> bool:
    return bool(YOUTUBE_VIDEO_ID_RE.fullmatch(value.strip()))
