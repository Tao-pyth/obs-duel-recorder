# Worker dependency management policy (v0.2)

This document defines how Python Worker dependencies are **declared**, **installed**, and **verified** for **v0.2 - Worker Core API**.

Scope:
- Developer setup and verification only.
- No runtime data (`user_data/**`) is created or committed by this policy.

## Responsibility boundaries (handoff)

This document is a **docs-only policy** for dependency declaration/installation/verification. To keep handoff clear:

- **Planned paths are made real elsewhere**: creating `app/**` / `app/worker/**`, adding `requirements*.txt`, and wiring entrypoints belong to the Worker skeleton tasks (e.g. #11 / #13).
- **Smoke / `requirements-dev.txt` usage is owned by the smoke plan**: defining the canonical smoke steps (including when to install `requirements-dev.txt`, and what checks to run) belongs to the smoke/verification work (e.g. #16).

## Principles

- Keep v0.2 dependency management **simple and Windows-friendly**.
- Avoid global Python assumptions; use a project-local virtual environment.
- Keep application code and runtime data separated:
  - Application code: `app/**` (planned)
  - Runtime data: `user_data/**` (must not be committed)

## Declaration

For v0.2, dependencies are declared as pip requirements files (planned locations):

- `app/worker/requirements.txt`: runtime dependencies (FastAPI, Uvicorn, etc.)
- `app/worker/requirements-dev.txt`: dev-only dependencies (tests, linters when introduced)

Notes:
- Pinning strategy can start as “direct deps pinned, transitive deps free” and tighten later.
- If a lockfile approach is introduced (e.g. `pip-tools`), do it in a dedicated issue/PR.

## Installation (Windows PowerShell)

Create a local venv at repo root:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
```

Install dependencies:

```powershell
python -m pip install -r app/worker/requirements.txt
# optional
python -m pip install -r app/worker/requirements-dev.txt
```

## Verification

Minimum verification for v0.2:

```powershell
python -m pip check
python -c "import fastapi; import uvicorn; print('OK')"
```

When the Worker entrypoint exists, add a smoke verification section to the v0.2 docs:

- Start Worker from the documented entrypoint
- Call `GET /health` and confirm required fields

## Changes to this policy

- Keep this policy minimal for v0.2.
- Any new tooling (Poetry, uv, pip-tools, lockfiles) must be introduced with rationale and kept to one responsibility per PR.
