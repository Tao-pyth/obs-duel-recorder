# YouTube セットアップ

-> [日本語ユーザードキュメント](index.md)

YouTube アップロードは任意です。動画を YouTube Data API 経由で公開したい場合だけ設定してください。

OAuth とアップロード処理は Worker が担当します。OBS Plugin はトークンを保存せず、YouTube へ直接アップロードしません。

## 保存場所

OAuth 関連ファイルはローカルの次の場所に保存します。

```text
user_data/config/secrets/
```

必要なファイル:

- `youtube-client-secret.json`
- `youtube-token.json`

`youtube-client-secret.json` はユーザーが Google Cloud Console から取得した OAuth デスクトップクライアントの JSON です。`youtube-token.json` は認可完了後に Worker が作成します。

## 認可手順

1. `youtube-client-secret.json` を `user_data/config/secrets/` に配置します。
2. Worker が起動している状態で認可 URL を取得します。

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/authorization-url -Method Post -ContentType "application/json" -Body '{}' | Select-Object -ExpandProperty Content
   ```

3. 返却された `authorization_url` をブラウザで開き、YouTube upload scope を許可します。
4. 通常はブラウザが次の URL に戻ります。

   ```text
   http://127.0.0.1:8787/upload/oauth/callback
   ```

5. ブラウザに認可コードだけが表示された場合は、手動でコードを交換します。

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/exchange-code -Method Post -ContentType "application/json" -Body '{"code":"PASTE_CODE_HERE"}' | Select-Object -ExpandProperty Content
   ```

6. 状態を確認します。

   ```powershell
   Invoke-WebRequest http://127.0.0.1:8787/upload/status | Select-Object -ExpandProperty Content
   ```

## readiness_state

`/upload/status` の `readiness_state` を主な状態として確認してください。`readiness_next_action` には次に行うべき操作が入ります。

| 状態 | 意味 | 次の操作 |
|---|---|---|
| `ready` | アップロード準備ができています。 | アップロード可能です。 |
| `client_secret_missing` | OAuth クライアントファイルがありません。 | Dock の `認証ファイルを選択` を使うか、`youtube-client-secret.json` を `user_data/config/secrets/` に配置します。 |
| `token_missing` | まだ認可が完了していません。 | ブラウザ認可を実行します。 |
| `token_invalid` | トークンが不正、未完成、または更新できません。 | 再認可して `youtube-token.json` を作り直します。 |
| `token_expired_refreshable` | トークン期限切れですが更新可能です。 | トークン更新を実行します。 |
| `dependencies_missing` | Google upload 用の依存ライブラリがありません。 | Google upload 対応の配布版を使うか依存関係を導入します。 |
| `quota_waiting` | YouTube quota 待ちです。 | quota リセット後に再試行します。 |
| `manual_review_required` | キュー項目に手動確認が必要です。 | YouTube 側を確認し、再試行、破棄、アップロード済み設定を選びます。 |

## トークン更新

```powershell
Invoke-WebRequest http://127.0.0.1:8787/upload/oauth/refresh -Method Post | Select-Object -ExpandProperty Content
```

更新できない場合は再認可してください。

## 秘密情報の扱い

issue、PR、ログ添付、GitHub Pages、Release ZIP、スクリーンショットに次の情報を含めないでください。

- `youtube-client-secret.json`
- `youtube-token.json`
- authorization code
- bearer token
- `code=...` を含む OAuth callback URL
- ローカル動画、スクリーンショット、ゲーム資産、テンプレート画像
