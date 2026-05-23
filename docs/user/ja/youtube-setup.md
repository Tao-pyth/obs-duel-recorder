# YouTube セットアップ

-> [日本語ユーザードキュメント](index.md)

YouTube upload は Worker が管理します。OAuth 情報はローカルの `user_data/` 配下に保存し、リポジトリ、GitHub Pages、release ZIP には含めません。

## 必要なファイル

既定の配置先:

```text
user_data/config/secrets/
```

必要なファイル:

- `youtube-client-secret.json`
- `youtube-token.json`

## 状態確認

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/status | Select-Object -ExpandProperty Content
```

確認する項目:

- `client_secret_configured`
- `token_configured`
- `queue_counts`
- `manual_actions`

## 失敗時の分類

- quota: `quota_waiting`
- auth: `need_manual_review`
- network: `upload_failed`
- ambiguous: `need_manual_review`

曖昧な失敗では、アップロード済みか判断できない可能性があります。二重アップロード防止のため、手動確認してから retry、discard、mark_uploaded を選択してください。

## 秘密情報の扱い

以下は issue、PR、ログ添付、GitHub Pages、release ZIP に含めないでください。

- OAuth token
- client secret
- authorization code
- bearer token
- YouTube API response の秘密情報
