# キュー / 履歴

-> [日本語ユーザードキュメント](index.md)

アップロードキューは Worker が所有します。OBS Plugin は SQLite を直接編集せず、Worker API を通じて状態を確認します。

## 主な状態

- `ready_upload`: アップロード待ちです。
- `uploading`: アップロード処理中です。
- `uploaded`: YouTube video id または URL が記録済みです。
- `upload_failed`: 通信失敗などで retry 可能です。
- `quota_waiting`: YouTube quota 待ちです。
- `need_manual_review`: 認証不足、曖昧な失敗、retry 上限などで手動確認が必要です。
- `discarded`: 処理対象から外れています。

## 状態確認

```powershell
Invoke-WebRequest http://127.0.0.1:8787/queue/items | Select-Object -ExpandProperty Content
Invoke-WebRequest http://127.0.0.1:8787/upload/status | Select-Object -ExpandProperty Content
```

## retry 前に確認すること

- ローカル動画ファイルが存在するか。
- OAuth token と client secret が配置されているか。
- YouTube quota 待ちではないか。
- `last_error_code` と `manual_review_reason` に二重アップロードの可能性がないか。

曖昧な失敗では、二重アップロード防止を優先して `need_manual_review` に降格します。
