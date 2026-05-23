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

## Fallback

When recognition fails, confidence is low, or fixture coverage does not support the input:
- do not mutate existing metadata automatically,
- return actionable diagnostics,
- fall back to manual correction and existing metadata editing.

## Non-Goals

- Making image recognition the primary duel trigger.
- Adding heavyweight OCR or ML dependencies as required runtime dependencies.
- Shipping game assets, user screenshots, or training data in the repository.
