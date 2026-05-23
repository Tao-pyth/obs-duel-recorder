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

## Safety

- Empty datasets must return stable zero-count responses.
- Filters must be documented before release.
- Diagnostics must not expose OAuth secrets, logs, local media contents, or unnecessary absolute paths.
