# Upload Provider

The upload system must support deterministic tests and production YouTube uploads through a provider boundary.

## Providers

- `MockUploader`: deterministic local behavior for tests, CI, and smoke flows.
- `GoogleUploader`: production YouTube upload implementation using optional official Google client libraries.

The Worker API should preserve the existing upload boundary while allowing provider selection through configuration or runtime capability detection.

## Optional Dependencies

The Google provider may use:
- `google-api-python-client`
- `google-auth-oauthlib`
- `google-auth-httplib2`

These dependencies should remain optional so non-upload workflows and CI fixtures do not require Google credentials or network access.

## Failure Classification

Google API failures should map to stable Worker outcomes:

| Category | Input Signal | Worker Outcome |
|---|---|---|
| quota | HTTP 403 with quota-related API reason | `quota_waiting` |
| auth | HTTP 401/403 auth or credential reason | `need_manual_review` |
| network | timeout, DNS, connection reset, retryable 5xx | `upload_failed` with retry evidence |
| ambiguous | response unknown after possible upload side effect | `need_manual_review` |

Diagnostics must not include OAuth tokens, client secrets, authorization codes, or bearer strings.

## Preflight

Setup wizard and `/upload/status` should detect missing OAuth prerequisites before a real upload attempt.
