# YouTube OAuth セットアップ

-> [日本語ユーザードキュメント](index.md)

このページは v1.1.4 の YouTube readiness contract に対応する OAuth 手順です。実際のアップロードには、有効な Google OAuth クライアント、ローカルに保存されたトークン、Google upload 依存関係を含む Worker 実行環境が必要です。

## OAuth でできること

OAuth は、Worker がユーザーの YouTube アカウントへ動画をアップロードするための認可です。OBS Plugin はトークンを保存せず、YouTube へ直接アップロードしません。

OAuth ファイルは次のローカルディレクトリに保存します。

```text
user_data/config/secrets/
```

このディレクトリを git、Release ZIP、ドキュメント、スクリーンショット、issue 添付に含めないでください。

## 必要なファイル

| ファイル | 作成者 | 目的 |
|---|---|---|
| `youtube-client-secret.json` | ユーザーが Google Cloud Console から取得 | ローカル OAuth デスクトップクライアントを識別します。 |
| `youtube-token.json` | ブラウザ認可後に Worker が作成 | ローカルの access token / refresh token 情報を保存します。 |

## セットアップ手順

1. YouTube Data API upload scope を使える Google OAuth デスクトップクライアントを用意します。
2. ダウンロードした JSON を次の名前で保存します。

   ```text
   user_data/config/secrets/youtube-client-secret.json
   ```

3. OBS Duel Recorder を起動し、Worker が動作していることを確認します。
4. 認可 URL を取得します。

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/authorization-url -Method Post -ContentType "application/json" -Body '{}' | Select-Object -ExpandProperty Content
   ```

5. 返却された `authorization_url` をブラウザで開き、YouTube upload scope を許可します。
6. 通常はブラウザが次の URL に戻ります。

   ```text
   http://127.0.0.1:8787/upload/oauth/callback
   ```

   ブラウザに認可コードだけが表示された場合は、手動でコードを交換します。

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/exchange-code -Method Post -ContentType "application/json" -Body '{"code":"PASTE_CODE_HERE"}' | Select-Object -ExpandProperty Content
   ```

7. readiness を確認します。

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/status | Select-Object -ExpandProperty Content
   ```

## readiness_state

`readiness_state` を主な状態値として使います。`readiness_next_action` にはユーザーが次に行う操作が入ります。

| 状態 | 意味 | 次の操作 |
|---|---|---|
| `ready` | アップロード前提条件を満たしています。 | アップロード可能です。 |
| `client_secret_missing` | OAuth クライアントファイルがありません。 | `youtube-client-secret.json` を `user_data/config/secrets/` に配置します。 |
| `token_missing` | まだトークンが作成されていません。 | ブラウザ認可を完了します。 |
| `token_invalid` | トークンが不正、未完成、または更新不能です。 | 再認可して `youtube-token.json` を置き換えます。 |
| `token_expired_refreshable` | トークン期限切れですが refresh token があります。 | トークン更新を実行します。 |
| `dependencies_missing` | Google upload ライブラリが実行環境にありません。 | Google upload 対応の配布版を使うか、依存関係を導入します。 |
| `quota_waiting` | YouTube quota を超過しています。 | quota リセット後に再試行します。 |
| `manual_review_required` | キュー項目にユーザー判断が必要です。 | YouTube 側を確認し、再試行、破棄、アップロード済み設定を選びます。 |

## トークン更新

トークン内容を表示せずに更新できます。

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/refresh -Method Post | Select-Object -ExpandProperty Content
```

refresh token がなく更新できない場合は、再認可して `youtube-token.json` を置き換えてください。

## 問題報告で共有してよい情報

OAuth 問題の報告では、次の安全な状態情報だけを共有してください。

- `readiness_state`
- `readiness_next_action`
- `auth.token_state`
- `client_secret_configured` と `token_configured` が true か false か

共有してはいけない情報:

- `youtube-client-secret.json`
- `youtube-token.json`
- authorization code
- bearer token
- `code=...` を含む OAuth callback URL
- ローカル動画、スクリーンショット、ゲーム資産、テンプレート画像
