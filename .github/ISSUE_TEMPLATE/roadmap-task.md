---
name: Roadmap task
description: Track a roadmap-driven repository task
title: ""
labels: [roadmap]
assignees: []
---

## Summary

## Goals

## Deliverables

## Milestone

## Responsibility Boundaries

- [ ] OBS Plugin work stays limited to OBS integration, Dock UI, overlay control, and Worker lifecycle behavior.
- [ ] Python Worker work owns queue processing, SQLite, image analysis, OCR, upload processing, exports, and recovery behavior.
- [ ] Runtime data and application code remain separated.

## Documentation-First Check

- [ ] Required design decisions are documented under `docs/`.
- [ ] User-facing documentation stays under `docs/user/`.
- [ ] `docs/roadmap.md` is not changed unless the task explicitly requires roadmap maintenance.

## Safety Check

- [ ] No runtime databases, logs, videos, screenshots, exports, secrets, or OAuth tokens are committed.
- [ ] No Yu-Gi-Oh! Master Duel assets, game screenshots, or extracted template images are committed.
