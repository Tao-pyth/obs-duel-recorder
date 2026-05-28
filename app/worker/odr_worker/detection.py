from __future__ import annotations

import base64
import binascii
import dataclasses
import datetime as _dt
import struct
import tomllib
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .recording import RecordingCommandError, RecordingState, apply_recording_command


TEMPLATE_KINDS = {"duel_start", "duel_end"}
LIFECYCLE_STATES = {"no_duel", "potential_duel", "active_duel", "ended_duel"}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
MAX_IMAGE_COMPARISONS = 25_000_000


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
class PngImage:
    width: int
    height: int
    pixels: tuple[int, ...]


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
    frame_png = _decode_png_or_none(frame)
    for template in templates:
        template_png = _decode_png_or_none(template.content) if frame_png is not None else None
        if frame_png is not None and template_png is not None:
            score = _image_match_score(template_png, frame_png)
        else:
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
    if "frame_base64" in payload:
        value = payload["frame_base64"]
        if not isinstance(value, str):
            raise TemplateConfigError({"frame_base64": "must_be_string"})
        try:
            return base64.b64decode(value, validate=True)
        except (binascii.Error, ValueError) as exc:
            raise TemplateConfigError({"frame_base64": "must_be_base64"}) from exc
    raise TemplateConfigError({"frame": "frame_text_frame_hex_or_frame_base64_required"})


def _decode_png_or_none(content: bytes) -> PngImage | None:
    try:
        return _decode_png(content)
    except ValueError:
        return None


def _decode_png(content: bytes) -> PngImage:
    if not content.startswith(PNG_SIGNATURE):
        raise ValueError("not_png")

    pos = len(PNG_SIGNATURE)
    width = height = bit_depth = color_type = interlace = None
    idat_parts: list[bytes] = []

    while pos + 8 <= len(content):
        length = struct.unpack(">I", content[pos : pos + 4])[0]
        chunk_type = content[pos + 4 : pos + 8]
        pos += 8
        chunk_end = pos + length
        crc_end = chunk_end + 4
        if crc_end > len(content):
            raise ValueError("truncated_png_chunk")
        chunk = content[pos:chunk_end]
        pos = crc_end

        if chunk_type == b"IHDR":
            if length != 13:
                raise ValueError("invalid_ihdr")
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
        elif chunk_type == b"IDAT":
            idat_parts.append(chunk)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None or bit_depth is None or color_type is None or interlace is None:
        raise ValueError("missing_ihdr")
    if width < 1 or height < 1:
        raise ValueError("invalid_png_size")
    if bit_depth != 8:
        raise ValueError("unsupported_png_bit_depth")
    if color_type not in {0, 2, 6}:
        raise ValueError("unsupported_png_color_type")
    if interlace != 0:
        raise ValueError("unsupported_png_interlace")
    if not idat_parts:
        raise ValueError("missing_idat")

    channels = {0: 1, 2: 3, 6: 4}[color_type]
    row_size = width * channels
    expected_size = height * (row_size + 1)
    try:
        raw = zlib.decompress(b"".join(idat_parts))
    except zlib.error as exc:
        raise ValueError("invalid_png_compression") from exc
    if len(raw) != expected_size:
        raise ValueError("invalid_png_data_size")

    pixels: list[int] = []
    previous = bytearray(row_size)
    offset = 0
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        row = bytearray(raw[offset : offset + row_size])
        offset += row_size
        _unfilter_png_row(row=row, previous=previous, filter_type=filter_type, channels=channels)
        pixels.extend(_png_row_to_luma(row=row, color_type=color_type, width=width))
        previous = row

    return PngImage(width=width, height=height, pixels=tuple(pixels))


def _unfilter_png_row(*, row: bytearray, previous: bytearray, filter_type: int, channels: int) -> None:
    if filter_type == 0:
        return
    if filter_type == 1:
        for index, value in enumerate(row):
            left = row[index - channels] if index >= channels else 0
            row[index] = (value + left) & 0xFF
        return
    if filter_type == 2:
        for index, value in enumerate(row):
            row[index] = (value + previous[index]) & 0xFF
        return
    if filter_type == 3:
        for index, value in enumerate(row):
            left = row[index - channels] if index >= channels else 0
            up = previous[index]
            row[index] = (value + ((left + up) // 2)) & 0xFF
        return
    if filter_type == 4:
        for index, value in enumerate(row):
            left = row[index - channels] if index >= channels else 0
            up = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            row[index] = (value + _paeth_predictor(left, up, upper_left)) & 0xFF
        return
    raise ValueError("unsupported_png_filter")


def _paeth_predictor(left: int, up: int, upper_left: int) -> int:
    estimate = left + up - upper_left
    left_distance = abs(estimate - left)
    up_distance = abs(estimate - up)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= up_distance and left_distance <= upper_left_distance:
        return left
    if up_distance <= upper_left_distance:
        return up
    return upper_left


def _png_row_to_luma(*, row: bytearray, color_type: int, width: int) -> list[int]:
    if color_type == 0:
        return list(row)

    channels = 3 if color_type == 2 else 4
    values: list[int] = []
    for x in range(width):
        index = x * channels
        red = row[index]
        green = row[index + 1]
        blue = row[index + 2]
        values.append((red * 299 + green * 587 + blue * 114) // 1000)
    return values


def _image_match_score(template: PngImage, frame: PngImage) -> float:
    if template.width > frame.width or template.height > frame.height:
        return 0.0

    positions_x = frame.width - template.width + 1
    positions_y = frame.height - template.height + 1
    comparisons = positions_x * positions_y * template.width * template.height
    step = 1
    if comparisons > MAX_IMAGE_COMPARISONS:
        step = max(1, int((comparisons / MAX_IMAGE_COMPARISONS) ** 0.5))

    best_diff = template.width * template.height * 255 + 1
    y_positions = _scan_positions(positions_y, step)
    x_positions = _scan_positions(positions_x, step)
    for y in y_positions:
        for x in x_positions:
            diff = _image_diff_at(template=template, frame=frame, x=x, y=y, best_diff=best_diff)
            if diff < best_diff:
                best_diff = diff
                if best_diff == 0:
                    return 1.0

    max_diff = template.width * template.height * 255
    return max(0.0, 1.0 - (best_diff / max_diff))


def _scan_positions(count: int, step: int) -> range | tuple[int, ...]:
    if step == 1 or count <= 1:
        return range(count)
    values = tuple(range(0, count, step))
    last = count - 1
    if values[-1] == last:
        return values
    return (*values, last)


def _image_diff_at(*, template: PngImage, frame: PngImage, x: int, y: int, best_diff: int) -> int:
    total = 0
    for row in range(template.height):
        template_offset = row * template.width
        frame_offset = (y + row) * frame.width + x
        for column in range(template.width):
            total += abs(template.pixels[template_offset + column] - frame.pixels[frame_offset + column])
        if total >= best_diff:
            return total
    return total


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
