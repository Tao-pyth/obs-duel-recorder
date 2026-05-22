from __future__ import annotations

import datetime as _dt
import json
import uuid
from dataclasses import asdict, dataclass, replace
from pathlib import Path
from typing import Any


RECORDING_STATES = {
    "idle",
    "starting",
    "recording",
    "stopping",
    "completed",
    "interrupted",
    "error",
}
ACTIVE_STATES = {"starting", "recording", "stopping"}
COMMAND_ACTIONS = {
    "start",
    "confirm_started",
    "stop",
    "confirm_stopped",
    "mark_interrupted",
    "discard_interrupted",
    "reset",
}
COMMAND_SOURCES = {"manual", "automatic", "recovery"}
MAX_REASON_LENGTH = 256


class RecordingCommandError(ValueError):
    def __init__(self, *, code: str, details: dict[str, str]) -> None:
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class RecordingState:
    state: str = "idle"
    session_id: str = ""
    command_source: str = "recovery"
    last_action: str = "init"
    reason: str = ""
    updated_at: str = ""

    def normalized(self) -> "RecordingState":
        if self.updated_at:
            return self
        return replace(self, updated_at=_utc_now_iso())

    def as_payload(self) -> dict[str, str]:
        return asdict(self.normalized())


def recording_state_path(data_dir: Path) -> Path:
    return (data_dir / "recording-state.json").resolve()


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")


def _new_session_id() -> str:
    return str(uuid.uuid4())


def _coerce_state(payload: Any) -> RecordingState:
    if not isinstance(payload, dict):
        return RecordingState(state="idle").normalized()

    state = str(payload.get("state", "idle"))
    if state not in RECORDING_STATES:
        state = "error"

    return RecordingState(
        state=state,
        session_id=str(payload.get("session_id", "")),
        command_source=str(payload.get("command_source", "recovery")),
        last_action=str(payload.get("last_action", "loaded")),
        reason=str(payload.get("reason", "")),
        updated_at=str(payload.get("updated_at", "")),
    ).normalized()


def load_recording_state(path: Path) -> RecordingState:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return RecordingState().normalized()
    except (OSError, json.JSONDecodeError):
        return RecordingState(state="error", last_action="load_failed", reason="recording_state_load_failed").normalized()
    return _coerce_state(payload)


def save_recording_state(path: Path, state: RecordingState) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = path.with_suffix(path.suffix + ".tmp")
    tmp_path.write_text(json.dumps(state.as_payload(), indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tmp_path.replace(path)


def recover_recording_state(state: RecordingState) -> RecordingState:
    if state.state not in ACTIVE_STATES:
        return state.normalized()
    return replace(
        state,
        state="interrupted",
        command_source="recovery",
        last_action="startup_recovery",
        reason="interrupted_active_recording_discard_required",
        updated_at=_utc_now_iso(),
    )


def initialize_recording_state(path: Path) -> RecordingState:
    state = recover_recording_state(load_recording_state(path))
    save_recording_state(path, state)
    return state


def overlay_recording_state(state: RecordingState) -> str:
    if state.state in {"starting", "recording", "stopping"}:
        return "recording"
    if state.state in {"idle", "completed"}:
        return "idle"
    return "unknown"


def apply_recording_command(current: RecordingState, payload: Any) -> RecordingState:
    if not isinstance(payload, dict):
        raise RecordingCommandError(code="recording_command_invalid", details={"payload": "must_be_object"})

    action = payload.get("action")
    source = payload.get("source", "manual")
    reason = payload.get("reason", "")
    errors: dict[str, str] = {}

    if not isinstance(action, str) or action not in COMMAND_ACTIONS:
        errors["action"] = "unknown_action"
    if not isinstance(source, str) or source not in COMMAND_SOURCES:
        errors["source"] = "unknown_source"
    if not isinstance(reason, str):
        errors["reason"] = "must_be_string"
    elif len(reason) > MAX_REASON_LENGTH:
        errors["reason"] = "too_long"

    if errors:
        raise RecordingCommandError(code="recording_command_invalid", details=errors)

    assert isinstance(action, str)
    assert isinstance(source, str)
    assert isinstance(reason, str)

    next_state = _transition(current.normalized(), action=action, source=source, reason=reason)
    return next_state


def _transition(current: RecordingState, *, action: str, source: str, reason: str) -> RecordingState:
    now = _utc_now_iso()

    def changed(state: str, *, session_id: str | None = None, note: str = "") -> RecordingState:
        return RecordingState(
            state=state,
            session_id=current.session_id if session_id is None else session_id,
            command_source=source,
            last_action=action,
            reason=reason or note,
            updated_at=now,
        )

    if action == "start":
        if current.state in {"idle", "completed"}:
            return changed("starting", session_id=_new_session_id())
        raise RecordingCommandError(
            code="recording_transition_invalid",
            details={"state": current.state, "action": action, "reason": "active_or_unrecoverable_session_exists"},
        )

    if action == "confirm_started":
        if current.state == "starting":
            return changed("recording")
        if current.state == "recording":
            return changed("recording", note="already_recording")
        raise RecordingCommandError(
            code="recording_transition_invalid",
            details={"state": current.state, "action": action, "reason": "start_not_pending"},
        )

    if action == "stop":
        if current.state == "recording":
            return changed("stopping")
        raise RecordingCommandError(
            code="recording_transition_invalid",
            details={"state": current.state, "action": action, "reason": "recording_not_active"},
        )

    if action == "confirm_stopped":
        if current.state == "stopping":
            return changed("completed")
        if current.state == "completed":
            return changed("completed", note="already_completed")
        raise RecordingCommandError(
            code="recording_transition_invalid",
            details={"state": current.state, "action": action, "reason": "stop_not_pending"},
        )

    if action == "mark_interrupted":
        if current.state in ACTIVE_STATES:
            return changed("interrupted", note="recording_interrupted")
        raise RecordingCommandError(
            code="recording_transition_invalid",
            details={"state": current.state, "action": action, "reason": "no_active_recording"},
        )

    if action == "discard_interrupted":
        if current.state == "interrupted":
            return changed("idle", session_id="", note="interrupted_recording_discarded")
        raise RecordingCommandError(
            code="recording_transition_invalid",
            details={"state": current.state, "action": action, "reason": "recording_not_interrupted"},
        )

    if action == "reset":
        return changed("idle", session_id="", note="recording_state_reset")

    raise RecordingCommandError(code="recording_command_invalid", details={"action": "unknown_action"})
