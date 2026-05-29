# First Setup

Status note: v1.3 adds a Worker setup wizard API for first-run setup status, validation, cancel, and reset/rerun flows. Graphical setup UI remains later work.

## Initial Startup

Check setup state:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/setup/status | Select-Object -ExpandProperty Content
```

The setup status can be:

- `first_run`
- `partial`
- `complete`

The current step is reported as `runtime_path`, `obs_integration`, `oauth`, or `templates`.

Validation responses include stable `code` values such as
`runtime_path_ready`, `runtime_path_action_required`,
`worker_api_compatible`, `oauth_action_required`, and
`templates_action_required`. The Dock can use these codes without parsing
diagnostic text.

---

## Runtime Path Setup

The Worker validates that `user_data/` and its runtime directories exist and are writable.

Existing runtime data is preserved. Setup validation must not delete, move, overwrite, or migrate existing DBs, videos, screenshots, exports, logs, or config.

In OBS, open the `OBS Duel Recorder` Dock and check the `Setup` row. Saving
Settings starts the minimum first-run validation path: the Plugin creates
`user_data_dir` when possible, checks that it is writable, starts the Worker,
and reports endpoint/API compatibility in the Dock. Manual Start/Stop stays
disabled until the Worker state is `running`.

---

## YouTube Setup

YouTube upload is optional. Configure it only when you want OBS Duel Recorder
to publish videos through the YouTube Data API.

The setup wizard validates whether YouTube credential files exist under:

```text
user_data/config/secrets/
```

Required files:

- `youtube-client-secret.json`
- `youtube-token.json`, created after authorization

The setup wizard reports only file presence and safe paths. It does not display token or client-secret contents.

For production upload OAuth setup:

1. Place your Google OAuth desktop client file at
   `user_data/config/secrets/youtube-client-secret.json`.
2. Request an authorization URL:

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/authorization-url -Method Post -ContentType "application/json" -Body '{}' | Select-Object -ExpandProperty Content
   ```

3. Open the returned URL in your browser and approve the YouTube upload scope.
   The default redirect URI is
   `http://127.0.0.1:8787/upload/oauth/callback`.
4. Let the browser return to `/upload/oauth/callback`. If the browser shows an
   authorization code instead, exchange the code manually:

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/exchange-code -Method Post -ContentType "application/json" -Body '{"code":"PASTE_CODE_HERE"}' | Select-Object -ExpandProperty Content
   ```

5. Check readiness:

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/status | Select-Object -ExpandProperty Content
   ```

Readiness is reported with `readiness_state` and `readiness_next_action` so the
Dock can show one clear action instead of raw debug fields:

| `readiness_state` | Meaning | User action |
|---|---|---|
| `ready` | OAuth, provider dependencies, and queue state allow upload. | Upload can proceed. |
| `client_secret_missing` | `youtube-client-secret.json` is not present. | Place the Google OAuth desktop client JSON under `user_data/config/secrets/`. |
| `token_missing` | Authorization has not created `youtube-token.json`. | Request an authorization URL and complete browser approval. |
| `token_invalid` | The token file is unreadable, incomplete, or expired without refresh support. | Reauthorize and replace `youtube-token.json`. |
| `token_expired_refreshable` | The token is expired but has a refresh token. | Use token refresh. |
| `dependencies_missing` | Optional Google upload libraries are unavailable. | Use a packaged release with Google upload support or install the missing runtime dependencies. |
| `quota_waiting` | Upload quota was exceeded. | Wait for quota reset before retrying. |
| `manual_review_required` | A queue item needs a user decision. | Review YouTube and then retry, discard, or mark uploaded. |

Token refresh can be requested without exposing token contents:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/refresh -Method Post | Select-Object -ExpandProperty Content
```

Do not commit `youtube-client-secret.json`, `youtube-token.json`,
authorization codes, or bearer tokens.

Do not paste authorization codes, tokens, client secrets, or full OAuth error
payloads into GitHub issues, screenshots, release ZIPs, logs, or documentation.
Use only the safe status fields from `/upload/status` when reporting problems.

For a focused OAuth guide, see [YouTube OAuth setup](youtube-oauth.md).

---

## Template Setup

The setup wizard validates:

- `user_data/config/templates.toml`
- local template files referenced by that config

The repository does not distribute game assets or local template images.

You can register local templates without hand-editing TOML by calling the setup
registration endpoint after placing your own files under `user_data/templates/`:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/setup/templates/register -Method Post -ContentType "application/json" -Body '{"kind":"start","path":"duel-start.tpl","threshold":1.0,"confirmations":2}' | Select-Object -ExpandProperty Content
Invoke-WebRequest http://127.0.0.1:8787/setup/templates/register -Method Post -ContentType "application/json" -Body '{"kind":"end","path":"duel-end.tpl","threshold":1.0,"confirmations":2}' | Select-Object -ExpandProperty Content
```

The endpoint validates that the file exists, is not empty, the threshold is
between `0.0` and `1.0`, and confirmations is a positive integer. Successful
registration writes `user_data/config/templates.toml` in the same format used by
the detection API.

Before enabling automatic recording, test your own local sample input:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/detection/test -Method Post -ContentType "application/json" -Body '{"kind":"start","frame_text":"sample frame bytes or fixture text"}' | Select-Object -ExpandProperty Content
```

Use samples captured from your own environment and keep them under runtime
storage such as `user_data/templates/`. Do not commit local template files,
screenshots, or game assets to the repository. The test result includes
template name, detection kind, score, match status, and diagnostics for missing
or low-confidence matches.

---

## OBS Integration Setup

The setup wizard reports Worker API and Worker version compatibility. The OBS Plugin still owns actual OBS process integration and Worker launch.

---

## Cancel And Reset

- `POST /setup/cancel` records cancellation and preserves completed steps.
- `POST /setup/reset` clears completed steps and preserves runtime data.
- After reset, setup can be run again from the first step.
