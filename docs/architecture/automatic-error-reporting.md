# Automatic Error Reporting

This document defines the `v1.1.2` operating contract for optional automatic
error report aggregation through GitHub Issues.

## Goal

Distributed OBS Duel Recorder packages may need a support path for errors that
occur outside a developer workstation. The reporting path must help maintainers
group repeated failures without exposing secrets or shipping GitHub credentials
inside the release ZIP.

## Responsibility Boundary

The distributed Plugin and Worker may prepare an anonymized error report, but
they must not call GitHub directly with a token.

```text
Distributed app
  -> HTTPS POST
Relay API server
  -> GitHub REST API
Private GitHub repository Issues
```

The relay API owns GitHub authentication, issue lookup, issue creation, comment
append, rate limiting, and private repository selection.

## GitHub Token Rule

GitHub tokens must never be packaged in:

- the release ZIP
- Plugin DLL resources
- Worker source files
- Worker executable bundles
- user-facing config templates
- `user_data/`

The only supported place for a GitHub token is the relay API deployment
environment.

## Report Payload

Reports should use a small structured payload:

```json
{
  "version": "v1.1.2",
  "component": "worker",
  "severity": "crash",
  "error_type": "ValueError",
  "message": "redacted summary",
  "top_stack_file": "odr_worker/upload.py",
  "top_stack_function": "process_upload",
  "top_stack_line": 123,
  "traceback": "redacted traceback",
  "occurred_at": "2026-05-27T12:34:56+09:00",
  "environment": {
    "os": "Windows 11",
    "python": "3.12.1"
  }
}
```

## Fingerprint

The relay API groups the same root cause with an eight-character fingerprint.

Inputs:

- `error_type`
- `top_stack_file`
- `top_stack_function`
- `top_stack_line`

Example:

```python
fingerprint_source = error_type + top_stack_file + top_stack_function + str(top_stack_line)
fingerprint = sha256(fingerprint_source.encode("utf-8")).hexdigest()[:8]
```

## Issue Matching

The relay API searches open Issues in the configured private repository for the
same fingerprint.

- If no open matching Issue exists, create a new Issue.
- If an open matching Issue exists, append an occurrence comment.

Issue title:

```text
[auto-error] v{version} / {error_type} / {fingerprint}
```

## Issue Body Template

````md
## Error Summary

Unexpected exception detected from distributed application.

## Application Version

v1.1.2

## Component

worker

## Error Type

ValueError

## Fingerprint

a1b2c3d4

## Environment

- OS: Windows 11
- Python: 3.12.1

## First Occurred

2026-05-27T12:34:56+09:00

## Traceback

```text
Traceback...
```

## Notes

Automatically generated error report. Personal information and secrets must be
redacted before submission.
````

## Reoccurrence Comment Template

````md
## Reoccurred

- time: 2026-05-27T13:10:11+09:00
- version: v1.1.2
- component: worker
- os: Windows 11

### Traceback

```text
Traceback...
```
````

## Labels

Recommended labels:

| Label | Meaning |
|---|---|
| `auto-report` | Created or updated by the relay API |
| `crash` | Fatal error |
| `warning` | Non-fatal report |
| `version:vX.Y.Z` | Application version |
| `os:windows` | Windows report |
| `duplicate-candidate` | Human follow-up may merge with another root cause |

## Redaction Rules

Reports must not include:

- usernames
- email addresses
- full local file contents
- OAuth tokens
- API keys
- GitHub tokens
- credential JSON
- personal or business data
- screenshots, recordings, or game assets

Windows user paths should be redacted:

```text
C:\Users\Tao\Desktop\sample.txt
-> C:\Users\<redacted>\Desktop\sample.txt
```

## Rate Limiting

The app or relay should avoid flooding GitHub.

Minimum rule:

- suppress repeated submissions of the same fingerprint for 60 seconds
- preserve local evidence for the user even when network submission is skipped
- fail closed when relay configuration is missing

## v1.1.2 Scope

`v1.1.2` records this reporting contract and release responsibility boundary.
Shipping a hosted relay API, private reporting repository, and end-user opt-in
UX requires maintainer infrastructure and must not be implied by the release ZIP
unless that deployment is explicitly configured and documented.
