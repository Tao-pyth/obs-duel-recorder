# アップデート

← [日本語ユーザードキュメント](index.md)

このページは、OBS Duel Recorder を更新する際の基本方針をまとめます。

ステータス注記: このページには、未リリースの手順や UI 名称が含まれる可能性があります。利用可否は [ロードマップ](../../roadmap.md) で確認してください。

## 更新前の確認

- OBS を終了してから更新してください。
- 更新前のバージョンが分かる場合は控えてください。例: `v1.3.0`
- 更新前に `update.bat validate --from-version <現在のバージョン>` を実行し、互換性エラーがないことを確認します。
- 既定の runtime root は `%APPDATA%\obs-duel-recorder\user_data` です。別の場所を使う場合は、`ODR_USER_DATA_DIR` を設定してから `update.bat` を実行します。

## バックアップ

- `user_data/` はランタイムデータのため、更新時も保持します。
- DB が存在する場合、`update.bat apply` は migration 実行前に `user_data/data/db/backups/` へ SQLite バックアップを作成します。
- 念のため、重要な更新前は `user_data/` 全体を別フォルダまたは別ドライブへ退避してください。

## 更新手順

1. リリース ZIP をダウンロードします。
2. ZIP と同じ Release の `SHA256SUMS.txt` で checksum を確認します。
3. ZIP を展開します。
4. `app\plugin\obs-duel-recorder.dll` を OBS の `obs-plugins\64bit\` へコピーします。
5. `app\worker\odr-worker\` を OBS の `obs-plugins\worker\odr-worker\` へコピーします。
6. `update.bat validate --from-version <現在のバージョン>` を実行します。
7. `update.bat apply --from-version <現在のバージョン>` を実行します。
8. OBS を起動し、Dock と Worker heartbeat を確認します。

## 保持されるデータ

- `user_data/config/`（設定、OAuth secrets など）
- `user_data/data/db/`（SQLite DB とバックアップ）
- `user_data/data/videos/`
- `user_data/data/screenshots/`
- `user_data/data/exports/`
- `user_data/logs/`

`user_data/` は OBS プラグイン DLL や bundled Worker EXE の配置先とは別に保持します。更新時に OBS の `obs-plugins` 配下を入れ替えても、`user_data/` は削除しないでください。

## 失敗時の復旧

- `user_data/data/update-state.json` を確認します。
- `status` が `failed` の場合、`backup_path` に記録された DB バックアップを確認します。
- OBS を停止したまま、必要に応じてバックアップ DB を `user_data/data/db/odr.sqlite3` へ戻します。
- 旧バージョンのアプリ/プラグインへ戻す場合も、`user_data/` は削除しないでください。

## うまくいかないとき

- [トラブルシューティング](troubleshooting.md)
