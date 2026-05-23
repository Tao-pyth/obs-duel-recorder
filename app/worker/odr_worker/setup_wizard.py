from __future__ import annotations

import datetime as _dt
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .detection import TemplateConfigError, load_template_config, load_templates
from .health import API_VERSION
from .runtime_dirs import RuntimeDirs
from .upload import build_upload_settings
from .version import __version__


SETUP_VERSION = "1.3"
SETUP_STEPS = ("runtime_path", "obs_integration", "oauth", "templates")


class SetupWizardError(ValueError):
    def __init__(self, code: str, details: dict[str, object]):
        super().__init__(code)
        self.code = code
        self.details = details


@dataclass(frozen=True)
class SetupStep:
    id: str
    title: str
    required: bool
    completed: bool
    status: str
    diagnostics: tuple[dict[str, object], ...]

    def as_payload(self) -> dict[str, object]:
        return {
            "id": self.id,
            "title": self.title,
            "required": self.required,
            "completed": self.completed,
            "status": self.status,
            "diagnostics": list(self.diagnostics),
        }


class SetupWizardStore:
    def __init__(self, *, runtime_dirs: RuntimeDirs):
        self.runtime_dirs = runtime_dirs
        self.state_path = runtime_dirs.data_dir / "setup-wizard.json"

    def status(self) -> dict[str, object]:
        state = self._load_state()
        validations = self.validate()
        completed = set(state["completed_steps"])
        steps = [
            _step_payload(step_id, completed=step_id in completed, validation=validations[step_id])
            for step_id in SETUP_STEPS
        ]
        status = _status(completed)
        return {
            "setup_version": SETUP_VERSION,
            "status": status,
            "first_run": not self.state_path.exists() and not completed,
            "current_step": _current_step(completed),
            "state_path": self.state_path.resolve().as_posix(),
            "completed_steps": sorted(completed),
            "steps": steps,
            "updated_at": state["updated_at"],
            "cancelled_at": state["cancelled_at"],
            "reset_count": state["reset_count"],
        }

    def validate(self) -> dict[str, dict[str, object]]:
        return {
            "runtime_path": self._validate_runtime_path(),
            "obs_integration": self._validate_obs_integration(),
            "oauth": self._validate_oauth(),
            "templates": self._validate_templates(),
        }

    def complete_step(self, step_id: str, payload: dict[str, Any]) -> dict[str, object]:
        if step_id not in SETUP_STEPS:
            raise SetupWizardError("setup_step_unknown", {"step": step_id})
        if not isinstance(payload, dict):
            raise SetupWizardError("setup_payload_invalid", {"payload": "must_be_object"})
        completed = payload.get("completed", True)
        if not isinstance(completed, bool):
            raise SetupWizardError("setup_payload_invalid", {"completed": "must_be_boolean"})

        state = self._load_state()
        steps = set(state["completed_steps"])
        if completed:
            steps.add(step_id)
        else:
            steps.discard(step_id)
        state["completed_steps"] = sorted(steps)
        state["cancelled_at"] = ""
        state["updated_at"] = _utc_now_iso()
        self._save_state(state)
        return self.status()

    def cancel(self) -> dict[str, object]:
        state = self._load_state()
        state["cancelled_at"] = _utc_now_iso()
        state["updated_at"] = state["cancelled_at"]
        self._save_state(state)
        return self.status()

    def reset(self) -> dict[str, object]:
        state = self._load_state()
        state["completed_steps"] = []
        state["cancelled_at"] = ""
        state["reset_count"] = int(state["reset_count"]) + 1
        state["updated_at"] = _utc_now_iso()
        self._save_state(state)
        return self.status()

    def _load_state(self) -> dict[str, object]:
        if not self.state_path.exists():
            return _default_state()
        try:
            raw = json.loads(self.state_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise SetupWizardError("setup_state_invalid", {"state_path": self.state_path.as_posix()}) from exc
        if not isinstance(raw, dict):
            raise SetupWizardError("setup_state_invalid", {"state": "must_be_object"})
        state = _default_state()
        state.update(raw)
        completed = state.get("completed_steps", [])
        if not isinstance(completed, list) or any(step not in SETUP_STEPS for step in completed):
            raise SetupWizardError("setup_state_invalid", {"completed_steps": "invalid"})
        state["completed_steps"] = sorted(set(completed))
        return state

    def _save_state(self, state: dict[str, object]) -> None:
        self.state_path.parent.mkdir(parents=True, exist_ok=True)
        tmp_path = self.state_path.with_suffix(".json.tmp")
        tmp_path.write_text(json.dumps(state, sort_keys=True, indent=2), encoding="utf-8")
        tmp_path.replace(self.state_path)

    def _validate_runtime_path(self) -> dict[str, object]:
        dirs = {
            "user_data": self.runtime_dirs.user_data_dir,
            "config": self.runtime_dirs.config_dir,
            "data": self.runtime_dirs.data_dir,
            "logs": self.runtime_dirs.logs_dir,
            "db": self.runtime_dirs.db_dir,
            "videos": self.runtime_dirs.videos_dir,
            "screenshots": self.runtime_dirs.screenshots_dir,
            "exports": self.runtime_dirs.exports_dir,
        }
        diagnostics: list[dict[str, object]] = []
        for label, path in dirs.items():
            if not path.exists() or not path.is_dir():
                diagnostics.append({"code": "directory_missing", "label": label, "path": path.as_posix()})
                continue
            probe = path / ".odr-setup-write-test"
            try:
                probe.write_text("ok", encoding="utf-8")
                probe.unlink()
            except OSError:
                diagnostics.append({"code": "directory_not_writable", "label": label, "path": path.as_posix()})
        existing_data = any(path.exists() for path in (self.runtime_dirs.db_dir / "odr.sqlite3",))
        return _validation_payload("ready" if not diagnostics else "action_required", diagnostics, existing_data)

    def _validate_obs_integration(self) -> dict[str, object]:
        diagnostics = [
            {
                "code": "worker_api_compatible",
                "api_version": API_VERSION,
                "worker_version": __version__,
            }
        ]
        return _validation_payload("ready", diagnostics, False)

    def _validate_oauth(self) -> dict[str, object]:
        settings = build_upload_settings(user_data_dir=self.runtime_dirs.user_data_dir)
        diagnostics: list[dict[str, object]] = []
        if not settings.client_secret_configured:
            diagnostics.append({"code": "client_secret_missing", "path": settings.client_secret_path.as_posix()})
        if not settings.token_configured:
            diagnostics.append({"code": "token_missing", "path": settings.token_path.as_posix()})
        status = "ready" if not diagnostics else "action_required"
        return _validation_payload(status, diagnostics, False)

    def _validate_templates(self) -> dict[str, object]:
        try:
            config = load_template_config(self.runtime_dirs.user_data_dir)
            loaded = load_templates(config)
        except TemplateConfigError as exc:
            return _validation_payload("action_required", [exc.details], False)
        diagnostics: list[dict[str, object]] = list(config.errors)
        if not config.config_loaded:
            diagnostics.append({"code": "template_config_missing", "path": config.config_path.as_posix()})
        if config.config_loaded and not loaded:
            diagnostics.append({"code": "templates_not_loaded", "templates_dir": config.templates_dir.as_posix()})
        status = "ready" if config.config_loaded and loaded and not diagnostics else "action_required"
        return _validation_payload(status, diagnostics, False)


def _default_state() -> dict[str, object]:
    return {
        "setup_version": SETUP_VERSION,
        "completed_steps": [],
        "updated_at": "",
        "cancelled_at": "",
        "reset_count": 0,
    }


def _status(completed: set[str]) -> str:
    if not completed:
        return "first_run"
    if completed == set(SETUP_STEPS):
        return "complete"
    return "partial"


def _current_step(completed: set[str]) -> str:
    for step in SETUP_STEPS:
        if step not in completed:
            return step
    return ""


def _step_payload(step_id: str, *, completed: bool, validation: dict[str, object]) -> dict[str, object]:
    titles = {
        "runtime_path": "Runtime path",
        "obs_integration": "OBS integration",
        "oauth": "YouTube OAuth",
        "templates": "Detection templates",
    }
    return SetupStep(
        id=step_id,
        title=titles[step_id],
        required=True,
        completed=completed,
        status=str(validation["status"]),
        diagnostics=tuple(validation["diagnostics"]),
    ).as_payload()


def _validation_payload(status: str, diagnostics: list[dict[str, object]], existing_data: bool) -> dict[str, object]:
    return {
        "status": status,
        "diagnostics": diagnostics,
        "existing_runtime_data": existing_data,
    }


def _utc_now_iso() -> str:
    return _dt.datetime.now(tz=_dt.timezone.utc).isoformat().replace("+00:00", "Z")
