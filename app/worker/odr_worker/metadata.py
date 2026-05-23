from __future__ import annotations

import datetime as _dt
import sqlite3
from dataclasses import dataclass
from pathlib import Path
from typing import Any


DEFAULT_TITLE_TEMPLATE = "Duel {match_id} vs {opponent_deck} - {result}"
MAX_TITLE_LENGTH = 100
MAX_DESCRIPTION_LENGTH = 5000
FIELD_LIMITS = {
    "deck_name": 120,
    "opponent_deck": 120,
    "result": 40,
    "rank": 80,
    "dp": 40,
    "memo": 4000,
    "started_at": 64,
    "ended_at": 64,
    "title_template": 200,
}


class MetadataError(ValueError):
    def __init__(self, code: str, details: dict[str, object]):
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class MatchMetadataRecord:
    id: int
    created_at: str
    updated_at: str
    deck_name: str
    opponent_deck: str
    result: str
    rank: str
    dp: str
    memo: str
    started_at: str
    ended_at: str
    title_template: str

    def as_payload(self) -> dict[str, object]:
        return {
            "id": self.id,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "deck_name": self.deck_name,
            "opponent_deck": self.opponent_deck,
            "result": self.result,
            "rank": self.rank,
            "dp": self.dp,
            "memo": self.memo,
            "started_at": self.started_at,
            "ended_at": self.ended_at,
            "title_template": self.title_template,
        }


class MatchMetadataStore:
    def __init__(self, db_path: Path):
        self.db_path = db_path

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA foreign_keys = ON;")
        return conn

    def create_match(self, payload: dict[str, Any]) -> MatchMetadataRecord:
        values = _metadata_values(payload, partial=True)
        now = _utc_now_iso()
        conn = self._connect()
        try:
            columns = ["updated_at", *values.keys()]
            params: dict[str, object] = {"updated_at": now, **values}
            placeholders = ", ".join(f":{column}" for column in columns)
            cursor = conn.execute(
                f"INSERT INTO matches({', '.join(columns)}) VALUES({placeholders});",
                params,
            )
            conn.commit()
            return self._get(conn, int(cursor.lastrowid))
        finally:
            conn.close()

    def get_match(self, match_id: int) -> MatchMetadataRecord:
        conn = self._connect()
        try:
            return self._get(conn, match_id)
        finally:
            conn.close()

    def list_matches(self, *, query: str | None = None) -> list[MatchMetadataRecord]:
        conn = self._connect()
        try:
            if query:
                like = f"%{query}%"
                rows = conn.execute(
                    """
                    SELECT * FROM matches
                    WHERE opponent_deck LIKE ? OR memo LIKE ? OR deck_name LIKE ? OR result LIKE ?
                    ORDER BY id;
                    """,
                    (like, like, like, like),
                ).fetchall()
            else:
                rows = conn.execute("SELECT * FROM matches ORDER BY id;").fetchall()
            return [_record_from_row(row) for row in rows]
        finally:
            conn.close()

    def update_match(self, match_id: int, payload: dict[str, Any]) -> MatchMetadataRecord:
        values = _metadata_values(payload, partial=True)
        if not values:
            raise MetadataError("metadata_payload_invalid", {"fields": "no_editable_fields"})
        conn = self._connect()
        try:
            self._get(conn, match_id)
            values["updated_at"] = _utc_now_iso()
            values["id"] = match_id
            assignments = ", ".join(f"{key} = :{key}" for key in values if key != "id")
            conn.execute(f"UPDATE matches SET {assignments} WHERE id = :id;", values)
            conn.commit()
            return self._get(conn, match_id)
        finally:
            conn.close()

    def render_upload_metadata(self, match_id: int) -> dict[str, object]:
        record = self.get_match(match_id)
        values = {
            "match_id": str(record.id),
            "deck_name": record.deck_name or "Unknown Deck",
            "opponent_deck": record.opponent_deck or "Unknown Opponent",
            "result": record.result or "unknown",
            "rank": record.rank or "unknown",
            "dp": record.dp or "unknown",
            "started_at": record.started_at or record.created_at,
            "ended_at": record.ended_at or "unknown",
            "created_at": record.created_at,
        }
        template = record.title_template or DEFAULT_TITLE_TEMPLATE
        title = _truncate(template.format_map(_SafeDict(values)), MAX_TITLE_LENGTH)
        description = _description(record, values)
        return {
            "match_id": record.id,
            "title": title,
            "description": description,
            "notes": record.memo,
            "variables": sorted(values.keys()),
        }

    def _get(self, conn: sqlite3.Connection, match_id: int) -> MatchMetadataRecord:
        row = conn.execute("SELECT * FROM matches WHERE id = ?;", (match_id,)).fetchone()
        if row is None:
            raise MetadataError("match_not_found", {"id": match_id})
        return _record_from_row(row)


class _SafeDict(dict[str, str]):
    def __missing__(self, key: str) -> str:
        return "unknown"


def _metadata_values(payload: dict[str, Any], *, partial: bool) -> dict[str, str]:
    if not isinstance(payload, dict):
        raise MetadataError("metadata_payload_invalid", {"payload": "must_be_object"})
    values: dict[str, str] = {}
    for key, limit in FIELD_LIMITS.items():
        if key not in payload:
            continue
        value = payload[key]
        if value is None:
            value = ""
        if not isinstance(value, str):
            raise MetadataError("metadata_payload_invalid", {key: "must_be_string_or_null"})
        normalized = value.strip()
        if len(normalized) > limit:
            raise MetadataError("metadata_payload_invalid", {key: f"max_length_{limit}"})
        values[key] = normalized
    if not partial:
        for key in FIELD_LIMITS:
            values.setdefault(key, "")
    return values


def _record_from_row(row: sqlite3.Row) -> MatchMetadataRecord:
    return MatchMetadataRecord(
        id=int(row["id"]),
        created_at=str(row["created_at"]),
        updated_at=str(row["updated_at"]),
        deck_name=str(row["deck_name"]),
        opponent_deck=str(row["opponent_deck"]),
        result=str(row["result"]),
        rank=str(row["rank"]),
        dp=str(row["dp"]),
        memo=str(row["memo"]),
        started_at=str(row["started_at"]),
        ended_at=str(row["ended_at"]),
        title_template=str(row["title_template"]),
    )


def _description(record: MatchMetadataRecord, values: dict[str, str]) -> str:
    lines = [
        "OBS Duel Recorder Archive",
        "",
        f"Match ID: {record.id}",
        f"Deck: {values['deck_name']}",
        f"Opponent: {values['opponent_deck']}",
        f"Result: {values['result']}",
        f"Rank: {values['rank']}",
        f"DP: {values['dp']}",
        f"Started: {values['started_at']}",
        f"Ended: {values['ended_at']}",
    ]
    if record.memo:
        lines.extend(["", "Notes:", record.memo])
    return _truncate("\n".join(lines), MAX_DESCRIPTION_LENGTH)


def _truncate(value: str, limit: int) -> str:
    if len(value) <= limit:
        return value
    if limit <= 3:
        return value[:limit]
    return value[: limit - 3] + "..."


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")
