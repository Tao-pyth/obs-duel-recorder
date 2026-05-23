from __future__ import annotations

import sqlite3
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .queue import QUEUE_STATES


KNOWN_RESULTS = ("win", "loss", "draw")
MAX_LIMIT = 200
DEFAULT_LIMIT = 50


class StatisticsError(ValueError):
    def __init__(self, code: str, details: dict[str, object]):
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class StatisticsFilters:
    from_date: str = ""
    to_date: str = ""
    deck: str = ""
    opponent_deck: str = ""
    result: str = ""

    def as_payload(self) -> dict[str, str]:
        return {
            "from_date": self.from_date,
            "to_date": self.to_date,
            "deck": self.deck,
            "opponent_deck": self.opponent_deck,
            "result": self.result,
        }


class StatisticsStore:
    def __init__(self, db_path: Path):
        self.db_path = db_path

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA foreign_keys = ON;")
        return conn

    def summary(self, filters: StatisticsFilters) -> dict[str, object]:
        rows = self._match_rows(filters)
        counts = _result_counts(rows)
        known_count = sum(counts[result] for result in KNOWN_RESULTS)
        win_rate = 0.0 if known_count == 0 else counts["win"] / known_count
        return {
            "filters": filters.as_payload(),
            "total_matches": len(rows),
            "known_result_count": known_count,
            "unknown_result_count": counts["unknown"],
            "result_counts": counts,
            "win_rate": round(win_rate, 4),
        }

    def decks(self, filters: StatisticsFilters, *, limit: int = DEFAULT_LIMIT) -> dict[str, object]:
        return {
            "filters": filters.as_payload(),
            "items": _group_match_rows(self._match_rows(filters), "deck_name", limit=_limit(limit)),
        }

    def opponents(self, filters: StatisticsFilters, *, limit: int = DEFAULT_LIMIT) -> dict[str, object]:
        return {
            "filters": filters.as_payload(),
            "items": _group_match_rows(self._match_rows(filters), "opponent_deck", limit=_limit(limit)),
        }

    def uploads(self, filters: StatisticsFilters) -> dict[str, object]:
        clauses: list[str] = []
        params: list[object] = []
        if filters.from_date:
            clauses.append("created_at >= ?")
            params.append(filters.from_date)
        if filters.to_date:
            clauses.append("created_at <= ?")
            params.append(filters.to_date)
        where = f" WHERE {' AND '.join(clauses)}" if clauses else ""
        conn = self._connect()
        try:
            rows = conn.execute(
                f"SELECT state, COUNT(*) AS count FROM upload_queue{where} GROUP BY state ORDER BY state;",
                params,
            ).fetchall()
        finally:
            conn.close()
        counts = {state: 0 for state in sorted(QUEUE_STATES)}
        for row in rows:
            state = str(row["state"])
            counts[state] = int(row["count"])
        return {
            "filters": {"from_date": filters.from_date, "to_date": filters.to_date},
            "total_items": sum(counts.values()),
            "state_counts": counts,
        }

    def memos(self, filters: StatisticsFilters, *, query: str, limit: int = DEFAULT_LIMIT) -> dict[str, object]:
        query = query.strip()
        if not query:
            raise StatisticsError("statistics_query_invalid", {"query": "non_empty_required"})
        items: list[dict[str, object]] = []
        query_key = query.casefold()
        for row in self._match_rows(filters):
            memo = str(row["memo"])
            if query_key not in memo.casefold():
                continue
            items.append(
                {
                    "match_id": int(row["id"]),
                    "created_at": str(row["created_at"]),
                    "started_at": str(row["started_at"]),
                    "deck_name": _display_value(row["deck_name"]),
                    "opponent_deck": _display_value(row["opponent_deck"]),
                    "result": _normalize_result(row["result"]),
                    "memo_excerpt": _excerpt(memo, query),
                }
            )
            if len(items) >= _limit(limit):
                break
        return {"filters": filters.as_payload(), "query": query, "items": items}

    def _match_rows(self, filters: StatisticsFilters) -> list[sqlite3.Row]:
        clauses: list[str] = []
        params: list[object] = []
        if filters.from_date:
            clauses.append("COALESCE(NULLIF(started_at, ''), created_at) >= ?")
            params.append(filters.from_date)
        if filters.to_date:
            clauses.append("COALESCE(NULLIF(started_at, ''), created_at) <= ?")
            params.append(filters.to_date)
        if filters.deck:
            clauses.append("LOWER(TRIM(deck_name)) = ?")
            params.append(_key(filters.deck))
        if filters.opponent_deck:
            clauses.append("LOWER(TRIM(opponent_deck)) = ?")
            params.append(_key(filters.opponent_deck))
        if filters.result:
            clauses.append("LOWER(TRIM(result)) = ?")
            params.append(_normalize_result(filters.result))
        where = f" WHERE {' AND '.join(clauses)}" if clauses else ""
        conn = self._connect()
        try:
            return conn.execute(f"SELECT * FROM matches{where} ORDER BY id;", params).fetchall()
        finally:
            conn.close()


def filters_from_query(
    *,
    from_date: str | None = None,
    to_date: str | None = None,
    deck: str | None = None,
    opponent_deck: str | None = None,
    result: str | None = None,
) -> StatisticsFilters:
    return StatisticsFilters(
        from_date=(from_date or "").strip(),
        to_date=(to_date or "").strip(),
        deck=(deck or "").strip(),
        opponent_deck=(opponent_deck or "").strip(),
        result=(result or "").strip(),
    )


def _result_counts(rows: list[sqlite3.Row]) -> dict[str, int]:
    counts = {"win": 0, "loss": 0, "draw": 0, "unknown": 0}
    for row in rows:
        counts[_normalize_result(row["result"])] += 1
    return counts


def _group_match_rows(rows: list[sqlite3.Row], field: str, *, limit: int) -> list[dict[str, object]]:
    grouped: dict[str, dict[str, Any]] = {}
    labels: dict[str, str] = {}
    for row in rows:
        raw = str(row[field])
        key = _key(raw) or "unknown"
        labels.setdefault(key, _display_value(raw))
        item = grouped.setdefault(
            key,
            {
                "name": labels[key],
                "total_matches": 0,
                "result_counts": {"win": 0, "loss": 0, "draw": 0, "unknown": 0},
                "win_rate": 0.0,
            },
        )
        item["total_matches"] += 1
        item["result_counts"][_normalize_result(row["result"])] += 1

    items = list(grouped.values())
    for item in items:
        result_counts = item["result_counts"]
        known_count = sum(result_counts[result] for result in KNOWN_RESULTS)
        item["known_result_count"] = known_count
        item["win_rate"] = 0.0 if known_count == 0 else round(result_counts["win"] / known_count, 4)
    items.sort(key=lambda item: (-int(item["total_matches"]), str(item["name"]).casefold()))
    return items[:limit]


def _normalize_result(value: object) -> str:
    normalized = str(value or "").strip().casefold()
    return normalized if normalized in KNOWN_RESULTS else "unknown"


def _display_value(value: object) -> str:
    normalized = " ".join(str(value or "").strip().split())
    return normalized or "unknown"


def _key(value: object) -> str:
    return " ".join(str(value or "").strip().casefold().split())


def _limit(value: int) -> int:
    if value < 1:
        raise StatisticsError("statistics_limit_invalid", {"limit": "must_be_positive"})
    return min(value, MAX_LIMIT)


def _excerpt(memo: str, query: str) -> str:
    index = memo.casefold().find(query.casefold())
    if index < 0:
        return memo[:120]
    start = max(0, index - 40)
    end = min(len(memo), index + len(query) + 40)
    prefix = "..." if start > 0 else ""
    suffix = "..." if end < len(memo) else ""
    return prefix + memo[start:end] + suffix
