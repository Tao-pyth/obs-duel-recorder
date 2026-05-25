from __future__ import annotations

import dataclasses
import datetime as _dt
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .recording import RecordingCommandError, RecordingState, apply_recording_command


TEMPLATE_KINDS = {"duel_start", "duel_end"}
LIFECYCLE_STATES = {"no_duel", "potential_duel", "active_duel", "ended_duel"}


class TemplateConfigError(ValueError):
    def __init__(self, details: dict[str, object]):
        super().__init__("template_config_invalid")
        self.details = details


@dataclass(frozen=True)
class TemplateSpec:
    name: str
    kind: str
    path: Path
    threshold: float = 1.0

    def as_payload(self) -> dict[str, object]:
        return {
            "name": self.name,
            "kind": self.kind,
            "path": self.path.as_posix(),
            "threshold": self.threshold,
        }


@dataclass(frozen=True)
class LoadedTemplate:
    spec: TemplateSpec
    content: bytes


@dataclass(frozen=True)
class TemplateConfig:
    config_path: Path
    templates_dir: Path
    config_loaded: bool
    start_confirmations: int
    end_confirmations: int
    templates: tuple[TemplateSpec, ...]
    errors: tuple[dict[str, object], ...] = ()

    def as_payload(self) -> dict[str, object]:
        return {
            "config_path": self.config_path.as_posix(),
            "templates_dir": self.templates_dir.as_posix(),
            "config_loaded": self.config_loaded,
            "start_confirmations": self.start_confirmations,
            "end_confirmations": self.end_confirmations,
            "templates": [template.as_payload() for template in self.templates],
            "errors": list(self.errors),
        }


@dataclass(frozen=True)
class TemplateMatch:
    name: str
    kind: str
    score: float
    matched: bool

    def as_payload(self) -> dict[str, object]:
        return dataclasses.asdict(self)


@dataclass(frozen=True)
class DetectionState:
    lifecycle_state: str = "no_duel"
    start_count: int = 0
    end_count: int = 0
    last_event: str = ""
    updated_at: str = ""

    def normalized(self) -> "DetectionState":
        if self.updated_at:
            return self
        return dataclasses.replace(self, updated_at=_utc_now_iso())

    def as_payload(self) -> dict[str, object]:
        return dataclasses.asdict(self.normalized())


def default_template_config_path(user_data_dir: Path) -> Path:
    return user_data_dir / "config" / "templates.toml"


def default_templates_dir(user_data_dir: Path) -> Path:
    return user_data_dir / "templates"


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")


def load_template_config(user_data_dir: Path) -> TemplateConfig:
    config_path = default_template_config_path(user_data_dir)
    templates_dir = default_templates_dir(user_data_dir)
    if not config_path.exists():
        return TemplateConfig(
            config_path=config_path,
            templates_dir=templates_dir,
            config_loaded=False,
            start_confirmations=2,
            end_confirmations=2,
            templates=(),
        )

    try:
        raw = tomllib.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError) as exc:
        raise TemplateConfigError({"config": f"failed_to_load: {exc}"}) from exc

    detection = raw.get("detection", {})
    if not isinstance(detection, dict):
        raise TemplateConfigError({"detection": "must_be_table"})

    start_confirmations = _positive_int(detection.get("start_confirmations", 2), "start_confirmations")
    end_confirmations = _positive_int(detection.get("end_confirmations", 2), "end_confirmations")

    raw_templates = raw.get("templates", [])
    if not isinstance(raw_templates, list):
        raise TemplateConfigError({"templates": "must_be_array_of_tables"})

    templates: list[TemplateSpec] = []
    errors: list[dict[str, object]] = []
    for index, item in enumerate(raw_templates):
        try:
            templates.append(_parse_template(item, index=index, config_path=config_path, templates_dir=templates_dir))
        except TemplateConfigError as exc:
            errors.append({"index": index, **exc.details})

    return TemplateConfig(
        config_path=config_path,
        templates_dir=templates_dir,
        config_loaded=True,
        start_confirmations=start_confirmations,
        end_confirmations=end_confirmations,
        templates=tuple(templates),
        errors=tuple(errors),
    )


def load_templates(config: TemplateConfig) -> tuple[LoadedTemplate, ...]:
    loaded: list[LoadedTemplate] = []
    errors = list(config.errors)
    for spec in config.templates:
        try:
            content = spec.path.read_bytes()
        except OSError as exc:
            errors.append({"template": spec.name, "path": spec.path.as_posix(), "error": f"failed_to_read: {exc}"})
            continue
        if not content:
            errors.append({"template": spec.name, "path": spec.path.as_posix(), "error": "empty_template"})
            continue
        loaded.append(LoadedTemplate(spec=spec, content=content))
    if errors:
        object.__setattr__(config, "errors", tuple(errors))
    return tuple(loaded)


def match_templates(templates: tuple[LoadedTemplate, ...], frame: bytes) -> list[TemplateMatch]:
    matches: list[TemplateMatch] = []
    for template in templates:
        score = 1.0 if template.content in frame else 0.0
        matches.append(
            TemplateMatch(
                name=template.spec.name,
                kind=template.spec.kind,
                score=score,
                matched=score >= template.spec.threshold,
            )
        )
    return matches


class DetectionRuntime:
    def __init__(self, config: TemplateConfig, templates: tuple[LoadedTemplate, ...]):
        self.config = config
        self.templates = templates
        self.state = DetectionState().normalized()

    def evaluate(self, payload: Any, recording_state: RecordingState) -> tuple[dict[str, object], RecordingState]:
        frame = _frame_bytes(payload)
        matches = match_templates(self.templates, frame)
        has_start = any(match.matched and match.kind == "duel_start" for match in matches)
        has_end = any(match.matched and match.kind == "duel_end" for match in matches)
        events: list[str] = []

        state = self.state.normalized()
        lifecycle_state = state.lifecycle_state
        start_count = state.start_count
        end_count = state.end_count
        next_recording = recording_state

        if lifecycle_state == "ended_duel":
            lifecycle_state = "no_duel"

        if lifecycle_state in {"no_duel", "potential_duel"}:
            if has_start:
                start_count += 1
                lifecycle_state = "potential_duel"
                if start_count >= self.config.start_confirmations:
                    lifecycle_state = "active_duel"
                    start_count = 0
                    end_count = 0
                    events.append("duel_started")
                    next_recording = _try_recording_command(next_recording, "start", events)
            else:
                start_count = 0
                lifecycle_state = "no_duel"
        elif lifecycle_state == "active_duel":
            if has_end:
                end_count += 1
                if end_count >= self.config.end_confirmations:
                    lifecycle_state = "ended_duel"
                    start_count = 0
                    end_count = 0
                    events.append("duel_ended")
                    next_recording = _try_recording_command(next_recording, "stop", events)
            else:
                end_count = 0

        last_event = events[-1] if events else state.last_event
        self.state = DetectionState(
            lifecycle_state=lifecycle_state,
            start_count=start_count,
            end_count=end_count,
            last_event=last_event,
            updated_at=_utc_now_iso(),
        )
        return (
            {
                **self.state.as_payload(),
                "events": events,
                "matches": [match.as_payload() for match in matches],
            },
            next_recording,
        )

    def test(self, payload: Any) -> dict[str, object]:
        frame = _frame_bytes(payload)
        selected_kind = _optional_template_kind(payload)
        templates = self.templates
        if selected_kind:
            templates = tuple(template for template in templates if template.spec.kind == selected_kind)

        matches = match_templates(templates, frame)
        matched = any(match.matched for match in matches)
        diagnostics = _template_test_diagnostics(
            config=self.config,
            loaded=templates,
            matches=matches,
            selected_kind=selected_kind,
        )
        return {
            "config_loaded": self.config.config_loaded,
            "kind": selected_kind or "any",
            "matched": matched,
            "matches": [match.as_payload() for match in matches],
            "diagnostics": diagnostics,
            "state_changed": False,
            "recording_command_sent": False,
        }


def _try_recording_command(state: RecordingState, action: str, events: list[str]) -> RecordingState:
    try:
        return apply_recording_command(
            state,
            {"action": action, "source": "automatic", "reason": f"detection_{action}"},
        )
    except RecordingCommandError as exc:
        events.append(f"recording_{action}_skipped:{exc.details.get('reason', exc.code)}")
        return state


def _frame_bytes(payload: Any) -> bytes:
    if not isinstance(payload, dict):
        raise TemplateConfigError({"payload": "must_be_object"})
    if "frame_text" in payload:
        value = payload["frame_text"]
        if not isinstance(value, str):
            raise TemplateConfigError({"frame_text": "must_be_string"})
        return value.encode("utf-8")
    if "frame_hex" in payload:
        value = payload["frame_hex"]
        if not isinstance(value, str):
            raise TemplateConfigError({"frame_hex": "must_be_string"})
        try:
            return bytes.fromhex(value)
        except ValueError as exc:
            raise TemplateConfigError({"frame_hex": "must_be_hex"}) from exc
    raise TemplateConfigError({"frame": "frame_text_or_frame_hex_required"})


def _optional_template_kind(payload: Any) -> str | None:
    if not isinstance(payload, dict):
        raise TemplateConfigError({"payload": "must_be_object"})
    if "kind" not in payload or payload["kind"] in (None, ""):
        return None
    value = payload["kind"]
    aliases = {
        "start": "duel_start",
        "duel_start": "duel_start",
        "end": "duel_end",
        "duel_end": "duel_end",
    }
    if not isinstance(value, str) or value.strip() not in aliases:
        raise TemplateConfigError({"kind": "must_be_start_or_end"})
    return aliases[value.strip()]


def _template_test_diagnostics(
    *,
    config: TemplateConfig,
    loaded: tuple[LoadedTemplate, ...],
    matches: list[TemplateMatch],
    selected_kind: str | None,
) -> list[dict[str, object]]:
    diagnostics = list(config.errors)
    configured = config.templates
    if selected_kind:
        configured = tuple(template for template in configured if template.kind == selected_kind)

    if not config.config_loaded:
        diagnostics.append({"code": "template_config_missing", "path": config.config_path.as_posix()})
    if not config.templates:
        diagnostics.append({"code": "templates_not_configured", "templates_dir": config.templates_dir.as_posix()})
    elif not configured:
        diagnostics.append({"code": "templates_not_configured_for_kind", "kind": selected_kind or "any"})
    elif not loaded:
        diagnostics.append({"code": "templates_not_loaded_for_kind", "kind": selected_kind or "any"})
    elif not any(match.matched for match in matches):
        best_score = max((match.score for match in matches), default=0.0)
        diagnostics.append(
            {
                "code": "template_match_missing",
                "kind": selected_kind or "any",
                "best_score": best_score,
            }
        )
    return diagnostics


def _positive_int(value: object, label: str) -> int:
    try:
        result = int(value)
    except (TypeError, ValueError) as exc:
        raise TemplateConfigError({label: "must_be_integer"}) from exc
    if result < 1:
        raise TemplateConfigError({label: "must_be_positive"})
    return result


def _parse_template(item: object, *, index: int, config_path: Path, templates_dir: Path) -> TemplateSpec:
    if not isinstance(item, dict):
        raise TemplateConfigError({"template": "must_be_table"})

    name = item.get("name")
    kind = item.get("kind")
    rel_path = item.get("path")
    threshold = item.get("threshold", 1.0)

    errors: dict[str, object] = {}
    if not isinstance(name, str) or not name:
        errors["name"] = "required_string"
    if not isinstance(kind, str) or kind not in TEMPLATE_KINDS:
        errors["kind"] = "unknown_kind"
    if not isinstance(rel_path, str) or not rel_path:
        errors["path"] = "required_string"
    try:
        threshold_value = float(threshold)
    except (TypeError, ValueError):
        errors["threshold"] = "must_be_number"
        threshold_value = 1.0
    if threshold_value < 0.0 or threshold_value > 1.0:
        errors["threshold"] = "must_be_between_0_and_1"

    if errors:
        raise TemplateConfigError(errors)

    assert isinstance(name, str)
    assert isinstance(kind, str)
    assert isinstance(rel_path, str)
    path = Path(rel_path)
    if not path.is_absolute():
        candidate = (config_path.parent / path).resolve()
        if not candidate.exists():
            candidate = (templates_dir / path).resolve()
        path = candidate
    return TemplateSpec(name=name, kind=kind, path=path.resolve(), threshold=threshold_value)
