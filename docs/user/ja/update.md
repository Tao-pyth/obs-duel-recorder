# アップデート

← [日本語ユーザードキュメント](index.md)

このページは、OBS Duel Recorder を更新する際の基本方針をまとめます。

ステータス注記: このページには、未リリースの手順や UI 名称が含まれる可能性があります。利用可否は [ロードマップ](../../roadmap.md) で確認してください。

## 更新前の確認

- OBS を終了してから更新してください。
- 更新前のバージョンが分かる場合は控えてください。例: `v1.3.0`
- 更新前に `update.bat validate --from-version <現在のバージョン>` を実行し、互換性エラーがないことを確認します。

## バックアップ

- `user_data/` はランタイムデータのため、更新時も保持します。
- DB が存在する場合、`update.bat apply` は migration 実行前に `user_data/data/db/backups/` へ SQLite バックアップを作成します。
- 念のため、重要な更新前は `user_data/` 全体を別フォルダまたは別ドライブへ退避してください。

## 更新手順

1. リリース ZIP をダウンロードします。
2. アプリ/プラグインのファイルを更新します。
3. `user_data/` を上書きしないよう注意します。
4. `update.bat validate --from-version <現在のバージョン>` を実行します。
5. `update.bat apply --from-version <現在のバージョン>` を実行します。
6. OBS と Worker を起動して動作確認します。

## 保持されるデータ

- `user_data/config/`（設定、OAuth secrets など）
- `user_data/data/db/`（SQLite DB とバックアップ）
- `user_data/data/videos/`
- `user_data/data/screenshots/`
- `user_data/data/exports/`
- `user_data/logs/`

## 失敗時の復旧

- `user_data/data/update-state.json` を確認します。
- `status` が `failed` の場合、`backup_path` に記録された DB バックアップを確認します。
- OBS を停止したまま、必要に応じてバックアップ DB を `user_data/data/db/odr.sqlite3` へ戻します。
- 旧バージョンのアプリ/プラグインへ戻す場合も、`user_data/` は削除しないでください。

## うまくいかないとき

- [トラブルシューティング](troubleshooting.md)
