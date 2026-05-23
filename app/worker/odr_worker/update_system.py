from __future__ import annotations

import argparse
import datetime as _dt
import json
import re
import sqlite3
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .db import DbInitError, init_db
from .health import API_VERSION
from .runtime_dirs import RuntimeDirs, ensure_runtime_dirs
from .version import __version__


UPDATE_STATE_FILE = "update-state.json"
INSTALLED_VERSION_FILE = "installed-version.json"


class UpdateError(RuntimeError):
    def __init__(self, code: str, details: dict[str, object]):
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class UpdateRequest:
    current_version: str
    target_version: str
    expected_api_version: str
    created_at: str


class UpdateManager:
    def __init__(self, *, runtime_dirs: RuntimeDirs):
        self.runtime_dirs = runtime_dirs
        self.state_path = runtime_dirs.data_dir / UPDATE_STATE_FILE
        self.installed_version_path = runtime_dirs.data_dir / INSTALLED_VERSION_FILE
        self.backups_dir = runtime_dirs.db_dir / "backups"

    def status(self) -> dict[str, object]:
        state = self._read_state()
        installed = self._read_installed_version()
        return {
            "status": state.get("status", "idle"),
            "partial_update_detected": state.get("status") in {"in_progress", "failed"},
            "current_version": installed.get("version", "unknown"),
            "target_version": __version__,
            "api_version": API_VERSION,
            "state_path": self.state_path.resolve().as_posix(),
            "backup_dir": self.backups_dir.resolve().as_posix(),
            "last_update": state,
        }

    def validate(self, payload: dict[str, Any] | None = None) -> dict[str, object]:
        request = self._request(payload or {})
        errors = self._validation_errors(request)
        return {
            "valid": not errors,
            "errors": errors,
            "current_version": request.current_version,
            "target_version": request.target_version,
            "api_version": API_VERSION,
            "expected_api_version": request.expected_api_version,
            "db_exists": self._db_path().exists(),
            "runtime_preserved": True,
        }

    def apply_update(self, payload: dict[str, Any] | None = None) -> dict[str, object]:
        request = self._request(payload or {})
        validation = self.validate(
            {
                "current_version": request.current_version,
                "target_version": request.target_version,
                "expected_api_version": request.expected_api_version,
                "created_at": request.created_at,
            }
        )
        if not validation["valid"]:
            raise UpdateError("update_validation_failed", {"errors": validation["errors"]})

        self._write_state(
            {
                "status": "in_progress",
                "started_at": request.created_at,
                "current_version": request.current_version,
                "target_version": request.target_version,
                "api_version": API_VERSION,
            }
        )

        backup_path = None
        try:
            backup_path = self._backup_db(request.created_at)
            db_info = init_db(runtime_dirs=self.runtime_dirs)
        except DbInitError as exc:
            failed = {
                "status": "failed",
                "failed_at": _utc_now_iso(),
                "current_version": request.current_version,
                "target_version": request.target_version,
                "api_version": API_VERSION,
                "backup_path": backup_path.resolve().as_posix() if backup_path else "",
                "error": "migration_failed",
                "recovery": "Stop OBS, keep user_data in place, and restore the DB backup before retrying.",
            }
            self._write_state(failed)
            raise UpdateError("update_migration_failed", {"reason": str(exc), "state": failed}) from exc

        completed = {
            "status": "completed",
            "started_at": request.created_at,
            "completed_at": _utc_now_iso(),
            "current_version": request.current_version,
            "target_version": request.target_version,
            "api_version": API_VERSION,
            "backup_path": backup_path.resolve().as_posix() if backup_path else "",
            "schema_version": db_info.schema_version,
            "applied_migrations": list(db_info.applied_migrations),
            "runtime_preserved": True,
        }
        self._write_state(completed)
        self._write_installed_version(request.target_version)
        return completed

    def _request(self, payload: dict[str, Any]) -> UpdateRequest:
        installed = self._read_installed_version()
        return UpdateRequest(
            current_version=_string(payload, "current_version", str(installed.get("version", "unknown"))),
            target_version=_string(payload, "target_version", __version__),
            expected_api_version=_string(payload, "expected_api_version", API_VERSION),
            created_at=_string(payload, "created_at", _utc_now_iso()),
        )

    def _validation_errors(self, request: UpdateRequest) -> list[dict[str, object]]:
        errors: list[dict[str, object]] = []
        if request.expected_api_version != API_VERSION:
            errors.append(
                {
                    "code": "api_version_mismatch",
                    "expected_api_version": request.expected_api_version,
                    "api_version": API_VERSION,
                }
            )

        current = _parse_semver(request.current_version)
        target = _parse_semver(request.target_version)
        if current is not None and target is not None and target < current:
            errors.append(
                {
                    "code": "downgrade_unsupported",
                    "current_version": request.current_version,
                    "target_version": request.target_version,
                }
            )

        if not self.runtime_dirs.user_data_dir.exists():
            errors.append({"code": "runtime_root_missing", "path": self.runtime_dirs.user_data_dir.as_posix()})

        return errors

    def _backup_db(self, created_at: str) -> Path | None:
        db_path = self._db_path()
        if not db_path.exists():
            return None

        self.backups_dir.mkdir(parents=True, exist_ok=True)
        backup_path = self.backups_dir / f"odr-{_safe_timestamp(created_at)}-{__version__}.sqlite3"
        if backup_path.exists():
            raise UpdateError("update_backup_conflict", {"path": backup_path.resolve().as_posix()})

        source = sqlite3.connect(db_path)
        try:
            target = sqlite3.connect(backup_path)
            try:
                source.backup(target)
            finally:
                target.close()
        finally:
            source.close()
        return backup_path

    def _db_path(self) -> Path:
        return self.runtime_dirs.db_dir / "odr.sqlite3"

    def _read_state(self) -> dict[str, object]:
        return _read_json_object(self.state_path)

    def _write_state(self, state: dict[str, object]) -> None:
        _write_json_object(self.state_path, state)

    def _read_installed_version(self) -> dict[str, object]:
        return _read_json_object(self.installed_version_path)

    def _write_installed_version(self, version: str) -> None:
        _write_json_object(
            self.installed_version_path,
            {"version": version, "api_version": API_VERSION, "updated_at": _utc_now_iso()},
        )


def _read_json_object(path: Path) -> dict[str, object]:
    if not path.exists():
        return {}
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {"status": "invalid"}
    return raw if isinstance(raw, dict) else {"status": "invalid"}


def _write_json_object(path: Path, value: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")


def _string(payload: dict[str, Any], key: str, default: str) -> str:
    value = payload.get(key, default)
    if not isinstance(value, str):
        raise UpdateError("update_payload_invalid", {key: "must_be_string"})
    value = value.strip()
    return value or default


def _parse_semver(value: str) -> tuple[int, int, int] | None:
    match = re.fullmatch(r"v?(\d+)\.(\d+)\.(\d+)", value.strip())
    if not match:
        return None
    return tuple(int(part) for part in match.groups())


def _safe_timestamp(value: str) -> str:
    safe = re.sub(r"[^0-9A-Za-z]+", "", value)[:20]
    return safe or "unknown-time"


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="odr-update")
    parser.add_argument("command", choices=("status", "validate", "apply"), nargs="?", default="apply")
    parser.add_argument("--user-data-dir", type=Path, help="Override runtime root; defaults to ODR_USER_DATA_DIR/user_data")
    parser.add_argument("--from-version", dest="current_version", help="Current installed version before update")
    parser.add_argument("--target-version", default=__version__, help="Target Worker version")
    parser.add_argument("--expected-api-version", default=API_VERSION, help="Expected Worker API version")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    runtime_dirs = ensure_runtime_dirs(user_data_dir=args.user_data_dir)
    manager = UpdateManager(runtime_dirs=runtime_dirs)
    payload = {
        "current_version": args.current_version or manager.status()["current_version"],
        "target_version": args.target_version,
        "expected_api_version": args.expected_api_version,
    }

    try:
        if args.command == "status":
            result = manager.status()
        elif args.command == "validate":
            result = manager.validate(payload)
        else:
            result = manager.apply_update(payload)
    except UpdateError as exc:
        print(json.dumps({"status": "failed", "code": exc.code, "details": exc.details}, ensure_ascii=False, indent=2))
        return 2

    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
