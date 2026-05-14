## Summary

## Related Issues

## Documentation-First Check

- [ ] Documentation has been added or updated where needed.
- [ ] Important design decisions are documented under `docs/`.
- [ ] User-facing documentation remains under `docs/user/`.
- [ ] `docs/roadmap.md` was not changed unless this PR is explicitly a roadmap maintenance PR.

## Responsibility Separation Check

- [ ] OBS Plugin responsibilities remain limited to OBS integration, Dock UI, overlay control, and Worker lifecycle behavior.
- [ ] Python Worker responsibilities remain responsible for queue processing, SQLite, image analysis, OCR, upload processing, exports, and recovery behavior.
- [ ] Runtime data and application code remain separated.

## Runtime Safety Check

- [ ] No runtime databases are committed.
- [ ] No OAuth tokens or secrets are committed.
- [ ] No logs, videos, screenshots, or generated exports are committed.
- [ ] No Yu-Gi-Oh! Master Duel assets, game screenshots, or extracted template images are committed.

## Testing

- [ ] Not run, documentation/scaffold only.
