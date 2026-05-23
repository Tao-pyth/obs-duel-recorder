# Image Recognition

v2.0 starts image-recognition-assisted metadata extraction. It does not require heavyweight OCR or ML runtime dependencies.

## Decision

- The first implementation should use an abstract Worker provider boundary and deterministic fixtures.
- Pillow may be used for lightweight image preprocessing such as crop, resize, grayscale, or threshold preparation.
- The initial provider may be fixture-backed so CI can verify behavior without external OCR binaries, model files, or game assets.
- Recognition output is a candidate signal, not authoritative state.

## Scope

The image recognition boundary may produce candidate metadata such as:
- result
- rank
- DP
- confidence/evidence fields

The recognized candidates should integrate with existing match metadata APIs so users can correct or confirm the values.

v2.0 exposes this through `POST /recognition/analyze`. The endpoint accepts fixture text or a fixture object and returns candidate `result`, `rank`, and `dp` values with confidence and evidence. It does not mutate metadata automatically. When a `match_id` is supplied, the response includes the `PUT /matches/{match_id}/metadata` correction endpoint and a `metadata_patch` body that the Plugin UI or user tooling can review before saving.

Recognition candidates are persisted when SQLite is available. `GET /recognition/candidates` lists the audit records, and `POST /recognition/candidates/{candidate_id}/command` resolves them as confirmed, corrected, or rejected. Confirming or correcting a candidate is an explicit manual action and updates match metadata through the existing metadata boundary.

## Fallback

When recognition fails, confidence is low, or fixture coverage does not support the input:
- do not mutate existing metadata automatically,
- return actionable diagnostics,
- fall back to manual correction and existing metadata editing.

## Non-Goals

- Making image recognition the primary duel trigger.
- Adding heavyweight OCR or ML dependencies as required runtime dependencies.
- Shipping game assets, user screenshots, or training data in the repository.
