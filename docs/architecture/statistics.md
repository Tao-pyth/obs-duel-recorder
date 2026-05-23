# Statistics

v2.1 introduces Worker-owned statistics derived from existing SQLite runtime data.

## Ownership

- Statistics are computed by the Worker.
- The OBS Plugin and documentation site should consume statistics through Worker APIs.
- Statistics must not rewrite historical match, upload, screenshot, or recognition records merely to improve aggregates.

## Data Sources

Statistics may derive from:
- match metadata
- upload queue state
- screenshot linkage metadata
- image-recognition candidate metadata once confirmed or stored through match metadata

## Expected Outputs

The statistics boundary should support deterministic summaries such as:
- win rate
- deck statistics
- opponent deck statistics
- memo search summaries
- upload state statistics

## Worker API

v2.1 exposes statistics through read-only Worker endpoints:

- `GET /statistics/summary`
- `GET /statistics/decks`
- `GET /statistics/opponents`
- `GET /statistics/uploads`
- `GET /statistics/memos?query=...`

Supported match filters:

- `from_date`
- `to_date`
- `deck`
- `opponent_deck`
- `result`

`/statistics/uploads` supports `from_date` and `to_date` over upload queue creation time.

Result values are normalized to `win`, `loss`, `draw`, or `unknown`. Win rate is computed as `win / (win + loss + draw)` and returns `0.0` when no known results exist.

Deck and opponent names are grouped by trimmed case-insensitive keys. Empty names are grouped as `unknown`.

Memo search is case-insensitive partial matching over the memo field only. It returns excerpts and never mutates match records.

## Safety

- Empty datasets must return stable zero-count responses.
- Filters must be documented before release.
- Diagnostics must not expose OAuth secrets, logs, local media contents, or unnecessary absolute paths.
