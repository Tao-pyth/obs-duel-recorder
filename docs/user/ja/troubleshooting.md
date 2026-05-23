# トラブルシューティング

-> [日本語ユーザードキュメント](index.md)

このページは、よくある問題の切り分け入口です。秘密情報、OAuth token、client secret、動画、スクリーンショット、DB を issue や PR に貼らないでください。

## Worker が起動しない

1. OBS Duel Recorder Dock の状態を確認します。
2. `user_data_dir` が正しいか確認します。
3. OBS の `obs-plugins\worker\odr-worker\odr-worker.exe` が存在するか確認します。
4. Worker の `/health` を確認します。

```powershell
Invoke-WebRequest http://127.0.0.1:8787/health | Select-Object -ExpandProperty Content
```

`config_error` の場合は設定パスを見直します。`api_incompatible` の場合は Plugin と Worker のバージョンが一致しているか確認します。

通常の ZIP 配置では、Plugin は OBS の `obs-plugins\64bit\obs-duel-recorder.dll` から見て `..\worker\odr-worker\odr-worker.exe` を優先します。そこに Worker EXE がない場合、開発者向けの `odr-worker` fallback を試します。

## Plugin Dock が表示されない

1. OBS Studio x64 を使用しているか確認します。
2. Plugin DLL が OBS の plugin path に配置されているか確認します。
3. OBS のログで `OBS Duel Recorder plugin startup` を探します。
4. OBS のログで `OBS Duel Recorder dock registered` を探します。
5. 依存 DLL や Qt/OBS SDK の配置漏れがないか確認します。

## 録画が開始または停止しない

1. `/recording/state` を確認します。
2. Dock の手動 Start/Stop が失敗する場合は、Worker state と OBS recording state がずれていないか確認します。
3. 自動録画の場合は `/detection/state` と `/detection/templates` を確認します。

認識候補 API は録画トリガではありません。録画開始/停止の判定は detection/template matching 側を確認してください。

## YouTube アップロードに失敗する

1. `/upload/status` を確認します。
2. `client_secret_configured` と `token_configured` が `true` か確認します。
3. `/queue/items` で対象 item の state、retry_count、last_error_code、manual_review_reason を確認します。

主な状態:

- `upload_failed`: 通信失敗など。原因確認後に retry できます。
- `quota_waiting`: YouTube quota 待ちです。次回試行時刻を確認します。
- `need_manual_review`: 認証不足、曖昧な失敗、retry 上限などです。二重アップロードを避けるため手動確認してください。
- `discarded`: ローカル動画欠損などで処理対象から外れています。

## キューが進まない

1. `/queue/items` で state を確認します。
2. `ready_upload` がない場合、処理対象はありません。
3. `uploading` が残っている場合、Worker 再起動時に安全側へ復旧されます。
4. 失敗が続く場合は動画ファイル、OAuth、quota、network を確認します。

## 更新に失敗する

1. `update.bat status` を実行します。
2. `partial_update_detected` が true の場合、`docs/user/ja/update.md` の復旧手順を確認します。
3. DB migration 失敗時は backup directory を確認します。
4. `update.bat` が `app\worker\odr-worker\odr-worker.exe` を見つけられるか確認します。

更新処理は `user_data/` の設定、DB、動画、スクリーンショット、エクスポート、ログを削除しない設計です。

## 認識候補が間違っている

1. `/recognition/candidates` で候補を確認します。
2. 正しい候補は confirm、誤りは correct または reject します。
3. 低信頼候補は自動で match metadata を上書きしません。

## 統計が期待と違う

1. `/statistics/summary` で result count を確認します。
2. 空欄や未知の result は `unknown` に分類されます。
3. Win rate は `win / (win + loss + draw)` で計算され、unknown は分母に入りません。
4. deck/opponent の集計は trim と case-insensitive key に基づきます。
