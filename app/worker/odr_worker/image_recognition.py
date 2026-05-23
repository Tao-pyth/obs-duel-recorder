from __future__ import annotations

import re
import sqlite3
import datetime as _dt
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .metadata import MatchMetadataStore, MetadataError


SUPPORTED_FIELDS = ("result", "rank", "dp")
MIN_ACCEPT_CONFIDENCE = 0.80
MAX_EVIDENCE_LENGTH = 160
CANDIDATE_STATUSES = {"candidate", "confirmed", "corrected", "rejected"}


class ImageRecognitionError(ValueError):
    def __init__(self, code: str, details: dict[str, object]):
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class RecognitionCandidate:
    field: str
    value: str
    confidence: float
    evidence: str

    def as_payload(self) -> dict[str, object]:
        return {
            "field": self.field,
            "value": self.value,
            "confidence": self.confidence,
            "evidence": self.evidence,
        }


@dataclass(frozen=True)
class RecognitionCandidateRecord:
    id: int
    match_id: int | None
    provider: str
    field: str
    value: str
    confidence: float
    evidence: str
    status: str
    corrected_value: str
    created_at: str
    updated_at: str

    def as_payload(self) -> dict[str, object]:
        return {
            "id": self.id,
            "match_id": self.match_id,
            "provider": self.provider,
            "field": self.field,
            "value": self.value,
            "confidence": self.confidence,
            "evidence": self.evidence,
            "status": self.status,
            "corrected_value": self.corrected_value,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
        }


@dataclass(frozen=True)
class RecognitionResult:
    provider: str
    status: str
    candidates: tuple[RecognitionCandidate, ...]
    diagnostics: tuple[dict[str, object], ...]

    def metadata_patch(self) -> dict[str, str]:
        return {
            candidate.field: candidate.value
            for candidate in self.candidates
            if candidate.confidence >= MIN_ACCEPT_CONFIDENCE
        }

    def as_payload(self, *, match_id: int | None = None) -> dict[str, object]:
        metadata_patch = self.metadata_patch()
        payload: dict[str, object] = {
            "provider": self.provider,
            "status": self.status,
            "match_id": match_id,
            "minimum_confidence": MIN_ACCEPT_CONFIDENCE,
            "candidates": [candidate.as_payload() for candidate in self.candidates],
            "metadata_patch": metadata_patch,
            "diagnostics": list(self.diagnostics),
            "mutated": False,
        }
        if match_id is not None:
            payload["manual_correction"] = {
                "method": "PUT",
                "endpoint": f"/matches/{match_id}/metadata",
                "body": metadata_patch,
            }
        return payload


class RecognitionCandidateStore:
    def __init__(self, db_path: Path):
        self.db_path = db_path

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA foreign_keys = ON;")
        return conn

    def save_result(self, *, match_id: int | None, result: RecognitionResult) -> list[RecognitionCandidateRecord]:
        conn = self._connect()
        try:
            records: list[RecognitionCandidateRecord] = []
            now = _utc_now_iso()
            for candidate in result.candidates:
                cursor = conn.execute(
                    """
                    INSERT INTO recognition_candidates(
                      match_id, provider, field, value, confidence, evidence,
                      status, corrected_value, created_at, updated_at
                    ) VALUES(?, ?, ?, ?, ?, ?, 'candidate', '', ?, ?);
                    """,
                    (
                        match_id,
                        result.provider,
                        candidate.field,
                        candidate.value,
                        candidate.confidence,
                        candidate.evidence,
                        now,
                        now,
                    ),
                )
                records.append(self._get(conn, int(cursor.lastrowid)))
            conn.commit()
            return records
        finally:
            conn.close()

    def list_candidates(self, *, match_id: int | None = None, status: str | None = None) -> list[RecognitionCandidateRecord]:
        if status is not None and status not in CANDIDATE_STATUSES:
            raise ImageRecognitionError("recognition_status_invalid", {"status": status})
        clauses: list[str] = []
        params: list[object] = []
        if match_id is not None:
            clauses.append("match_id = ?")
            params.append(match_id)
        if status is not None:
            clauses.append("status = ?")
            params.append(status)
        where = f" WHERE {' AND '.join(clauses)}" if clauses else ""
        conn = self._connect()
        try:
            rows = conn.execute(f"SELECT * FROM recognition_candidates{where} ORDER BY id;", params).fetchall()
            return [_record_from_row(row) for row in rows]
        finally:
            conn.close()

    def apply_command(
        self,
        candidate_id: int,
        payload: dict[str, Any],
        *,
        metadata_store: MatchMetadataStore | None,
    ) -> RecognitionCandidateRecord:
        action = payload.get("action")
        if action not in {"confirm", "correct", "reject"}:
            raise ImageRecognitionError("recognition_command_invalid", {"action": "confirm_correct_or_reject_required"})

        conn = self._connect()
        try:
            record = self._get(conn, candidate_id)
            if record.status != "candidate":
                raise ImageRecognitionError(
                    "recognition_candidate_already_resolved",
                    {"id": candidate_id, "status": record.status},
                )

            corrected_value = ""
            metadata_value = record.value
            status = "confirmed"
            if action == "reject":
                status = "rejected"
                metadata_value = ""
            elif action == "correct":
                value = payload.get("value")
                if not isinstance(value, str) or not value.strip():
                    raise ImageRecognitionError("recognition_command_invalid", {"value": "non_empty_string_required"})
                corrected_value = _normalize_candidate_value(record.field, value)
                metadata_value = corrected_value
                status = "corrected"

            if action in {"confirm", "correct"}:
                if record.match_id is None:
                    raise ImageRecognitionError("recognition_match_required", {"id": candidate_id})
                if metadata_store is None:
                    raise ImageRecognitionError("recognition_metadata_unavailable", {"id": candidate_id})
                metadata_store.update_match(record.match_id, {record.field: metadata_value})

            conn.execute(
                """
                UPDATE recognition_candidates
                SET status = ?, corrected_value = ?, updated_at = ?
                WHERE id = ?;
                """,
                (status, corrected_value, _utc_now_iso(), candidate_id),
            )
            conn.commit()
            return self._get(conn, candidate_id)
        except MetadataError as exc:
            raise ImageRecognitionError(exc.code, exc.details) from exc
        finally:
            conn.close()

    def _get(self, conn: sqlite3.Connection, candidate_id: int) -> RecognitionCandidateRecord:
        row = conn.execute("SELECT * FROM recognition_candidates WHERE id = ?;", (candidate_id,)).fetchone()
        if row is None:
            raise ImageRecognitionError("recognition_candidate_not_found", {"id": candidate_id})
        return _record_from_row(row)


class FixtureImageRecognitionProvider:
    name = "fixture"

    def analyze(self, payload: dict[str, Any]) -> RecognitionResult:
        if not isinstance(payload, dict):
            raise ImageRecognitionError("recognition_payload_invalid", {"payload": "must_be_object"})

        source_text = _source_text(payload)
        candidates = tuple(_candidate(field, source_text) for field in SUPPORTED_FIELDS if _field_value(field, source_text))
        diagnostics: list[dict[str, object]] = []
        if not candidates:
            diagnostics.append({"code": "no_candidates", "message": "No fixture recognition fields were found"})
        low_confidence = [candidate.field for candidate in candidates if candidate.confidence < MIN_ACCEPT_CONFIDENCE]
        if low_confidence:
            diagnostics.append({"code": "low_confidence", "fields": low_confidence})

        status = "candidates_available" if candidates and not low_confidence else "manual_review_required"
        return RecognitionResult(
            provider=self.name,
            status=status,
            candidates=candidates,
            diagnostics=tuple(diagnostics),
        )


def analyze_fixture_payload(payload: dict[str, Any]) -> RecognitionResult:
    provider = payload.get("provider", "fixture")
    if provider != "fixture":
        raise ImageRecognitionError("recognition_provider_unsupported", {"provider": provider})
    return FixtureImageRecognitionProvider().analyze(payload)


def _source_text(payload: dict[str, Any]) -> str:
    value = payload.get("content_text")
    if isinstance(value, str):
        return value
    fixture = payload.get("fixture")
    if isinstance(fixture, dict):
        lines: list[str] = []
        for field in SUPPORTED_FIELDS:
            field_value = fixture.get(field)
            if field_value is not None:
                lines.append(f"{field}: {field_value}")
        confidence = fixture.get("confidence")
        if confidence is not None:
            lines.append(f"confidence: {confidence}")
        return "\n".join(lines)
    raise ImageRecognitionError(
        "recognition_payload_invalid",
        {"content": "content_text_or_fixture_required"},
    )


def _field_value(field: str, source_text: str) -> str:
    pattern = re.compile(rf"^\s*{re.escape(field)}\s*[:=]\s*(.+?)\s*$", re.IGNORECASE | re.MULTILINE)
    match = pattern.search(source_text)
    if match is None:
        return ""
    value = match.group(1).strip()
    if field == "result":
        return _normalize_result(value)
    if field == "dp":
        return _normalize_dp(value)
    return _normalize_text(value)


def _candidate(field: str, source_text: str) -> RecognitionCandidate:
    value = _field_value(field, source_text)
    return RecognitionCandidate(
        field=field,
        value=value,
        confidence=_confidence(source_text),
        evidence=_evidence(field, source_text),
    )


def _normalize_result(value: str) -> str:
    normalized = _normalize_text(value).lower()
    aliases = {
        "w": "win",
        "won": "win",
        "victory": "win",
        "l": "loss",
        "lost": "loss",
        "defeat": "loss",
        "d": "draw",
        "tie": "draw",
    }
    return aliases.get(normalized, normalized)


def _normalize_dp(value: str) -> str:
    return re.sub(r"[^0-9]", "", value)


def _normalize_text(value: str) -> str:
    return re.sub(r"\s+", " ", value.strip())


def _confidence(source_text: str) -> float:
    match = re.search(r"^\s*confidence\s*[:=]\s*([0-9]+(?:\.[0-9]+)?)\s*$", source_text, re.IGNORECASE | re.MULTILINE)
    if match is None:
        return 0.95
    try:
        value = float(match.group(1))
    except ValueError:
        return 0.0
    if value > 1.0:
        value = value / 100.0
    return max(0.0, min(1.0, value))


def _evidence(field: str, source_text: str) -> str:
    pattern = re.compile(rf"^\s*{re.escape(field)}\s*[:=]\s*(.+?)\s*$", re.IGNORECASE | re.MULTILINE)
    match = pattern.search(source_text)
    if match is None:
        return ""
    evidence = _normalize_text(match.group(0))
    if len(evidence) <= MAX_EVIDENCE_LENGTH:
        return evidence
    return evidence[: MAX_EVIDENCE_LENGTH - 3] + "..."


def _normalize_candidate_value(field: str, value: str) -> str:
    if field == "result":
        return _normalize_result(value)
    if field == "dp":
        return _normalize_dp(value)
    return _normalize_text(value)


def _record_from_row(row: sqlite3.Row) -> RecognitionCandidateRecord:
    return RecognitionCandidateRecord(
        id=int(row["id"]),
        match_id=row["match_id"],
        provider=str(row["provider"]),
        field=str(row["field"]),
        value=str(row["value"]),
        confidence=float(row["confidence"]),
        evidence=str(row["evidence"]),
        status=str(row["status"]),
        corrected_value=str(row["corrected_value"]),
        created_at=str(row["created_at"]),
        updated_at=str(row["updated_at"]),
    )


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")
