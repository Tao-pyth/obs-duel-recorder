from __future__ import annotations

from dataclasses import asdict, dataclass, replace
from typing import Any


ALLOWED_RECORDING_STATES = {"idle", "recording", "paused", "unknown"}
OVERLAY_FIELDS = ("deck_name", "sequence_number", "result", "opponent_deck", "recording_state")
MAX_OVERLAY_VALUE_LENGTH = 256


class OverlayPayloadError(ValueError):
    def __init__(self, details: dict[str, str]) -> None:
        super().__init__("Invalid overlay payload")
        self.details = details


@dataclass(frozen=True)
class OverlayState:
    deck_name: str = ""
    sequence_number: str = ""
    result: str = "unknown"
    opponent_deck: str = "unknown"
    recording_state: str = "idle"

    def as_payload(self) -> dict[str, str]:
        return asdict(self)


def apply_overlay_update(current: OverlayState, payload: Any) -> OverlayState:
    if not isinstance(payload, dict):
        raise OverlayPayloadError({"payload": "must_be_object"})

    updates: dict[str, str] = {}
    errors: dict[str, str] = {}
    for key, value in payload.items():
        if key not in OVERLAY_FIELDS:
            errors[str(key)] = "unknown_field"
            continue
        if not isinstance(value, str):
            errors[key] = "must_be_string"
            continue
        if len(value) > MAX_OVERLAY_VALUE_LENGTH:
            errors[key] = "too_long"
            continue
        if key == "recording_state" and value not in ALLOWED_RECORDING_STATES:
            errors[key] = "unknown_recording_state"
            continue
        updates[key] = value

    if errors:
        raise OverlayPayloadError(errors)
    return replace(current, **updates)
