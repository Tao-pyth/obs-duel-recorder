# YouTube OAuth Setup

Status note: This page describes the v1.1.4 YouTube readiness contract. Real
upload still depends on a valid local Google OAuth client, a local token, and a
Worker build that includes the optional Google upload dependencies.

## What OAuth Enables

OAuth authorizes the Worker to upload videos to the user's YouTube account.
The OBS Plugin does not store tokens and does not upload directly. The Worker
stores OAuth files locally under:

```text
user_data/config/secrets/
```

Do not put this directory into git, release ZIPs, documentation, screenshots, or
issue attachments.

## Required Files

| File | Created by | Purpose |
|---|---|---|
| `youtube-client-secret.json` | User downloads from Google Cloud Console | Identifies the local OAuth desktop client. |
| `youtube-token.json` | Worker creates after browser authorization | Stores the local access and refresh token data. |

## Setup Steps

1. Create or choose a Google OAuth desktop client that is allowed to use the
   YouTube Data API upload scope.
2. Save the downloaded JSON as:

   ```text
   user_data/config/secrets/youtube-client-secret.json
   ```

3. Start OBS Duel Recorder and confirm the Worker is running.
4. Request an authorization URL:

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/authorization-url -Method Post -ContentType "application/json" -Body '{}' | Select-Object -ExpandProperty Content
   ```

5. Open the returned `authorization_url` in a browser and approve the YouTube
   upload scope.
6. Let the browser return to:

   ```text
   http://127.0.0.1:8787/upload/oauth/callback
   ```

   If the browser shows a code instead of returning to the Worker, exchange it
   manually:

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/exchange-code -Method Post -ContentType "application/json" -Body '{"code":"PASTE_CODE_HERE"}' | Select-Object -ExpandProperty Content
   ```

7. Check readiness:

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/status | Select-Object -ExpandProperty Content
   ```

## Readiness States

Use `readiness_state` as the main upload readiness value. Use
`readiness_next_action` as the user-facing next step.

| State | Meaning | Next action |
|---|---|---|
| `ready` | Upload prerequisites are satisfied. | Upload can proceed. |
| `client_secret_missing` | The OAuth client file is missing. | Place `youtube-client-secret.json` under `user_data/config/secrets/`. |
| `token_missing` | Authorization has not produced a token yet. | Complete browser authorization. |
| `token_invalid` | The token is invalid, incomplete, or cannot be refreshed. | Reauthorize and replace `youtube-token.json`. |
| `token_expired_refreshable` | The token is expired but has a refresh token. | Refresh the token. |
| `dependencies_missing` | Google upload libraries are unavailable in this runtime. | Use a packaged build with Google upload support or install the missing dependencies. |
| `quota_waiting` | YouTube quota was exceeded. | Wait for quota reset before retrying quota-waiting uploads. |
| `manual_review_required` | A queue item needs a user decision. | Review YouTube, then retry, discard, or mark uploaded. |

## Token Refresh

Refresh can be requested without printing token contents:

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/refresh -Method Post | Select-Object -ExpandProperty Content
```

If refresh fails because no refresh token exists, run authorization again and
replace `youtube-token.json`.

## Safe Reporting

When reporting an OAuth problem, include only:

- `readiness_state`
- `readiness_next_action`
- `auth.token_state`
- whether `client_secret_configured` and `token_configured` are true or false

Do not share:

- `youtube-client-secret.json`
- `youtube-token.json`
- authorization codes
- bearer tokens
- full OAuth callback URLs that include `code=...`
- local videos, screenshots, game assets, or template images
