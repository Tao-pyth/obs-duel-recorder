# GitHub Pages Publication

This document defines the v2.2 documentation publication contract.

## Source And Generated Paths

- Source documentation lives under `docs/`.
- User-facing documentation source lives under `docs/user/`.
- Generated GitHub Pages artifacts are written to `build/docs-site/`.
- The generated artifact is not a release ZIP and must not be used as an application package.

## Publication Flow

1. Run Markdown link validation.
2. Run Japanese user docs coverage validation.
3. Build the static documentation artifact:

   ```powershell
   python scripts/build_docs_site.py --root . --output build/docs-site
   ```

4. Upload `build/docs-site/` as the GitHub Pages artifact.
5. Deploy through GitHub Pages after the validation and artifact steps pass.

The GitHub Actions workflow is `.github/workflows/pages.yml`.

## Published Entry Point

The generated artifact entry point is:

```text
build/docs-site/index.html
```

Markdown sources are also copied into the artifact for traceability, but published navigation should use the generated `.html` files.

The expected public URL is:

```text
https://tao-pyth.github.io/obs-duel-recorder/
```

If repository Pages settings use a different owner or project URL, record the actual URL in the release notes.

## Publication Exclusions

The Pages artifact must not include:

- `user_data/`
- logs
- SQLite databases
- screenshots
- videos
- OAuth tokens or client secrets
- local template images or game assets
- generated runtime exports

## Relationship To Release ZIPs

GitHub Pages publishes documentation only.

Release ZIP contents remain governed by `docs/architecture/packaging.md`. Release ZIPs may include a minimal documentation subset, but Pages must not include plugin DLLs, Worker runtime packages, update bundles, runtime data, secrets, or generated media.
