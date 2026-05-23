# Packaging

This document defines the release packaging direction for v2.0+.

## Release ZIP

The preferred distribution format is a GitHub Actions generated ZIP.

Expected layout:

```text
obs-duel-recorder-vX.Y.Z/
|-- app/
|   |-- plugin/
|   |   `-- obs-duel-recorder.dll
|   `-- worker/
|       `-- <worker package files>
|-- docs/
|   `-- <minimal install/update/user docs>
|-- update.bat
|-- README.md
|-- LICENSE
`-- SHA256SUMS.txt
```

## Exclusions

Release ZIPs must not include:
- `user_data/`
- logs
- SQLite databases
- screenshots
- videos
- OAuth tokens or client secrets
- local template images or game assets
- generated runtime exports

## Release Asset Policy

- GitHub Actions may build and upload draft artifacts automatically.
- Release publication or attaching final assets should require maintainer approval unless a later policy explicitly enables full automation.
- A SHA256 checksum must be generated for each release ZIP.
