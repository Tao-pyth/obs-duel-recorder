# FAQ

-> [日本語ユーザードキュメント](index.md)

このページは、OBS Duel Recorder の利用時によくある質問をまとめます。

## 基本

### Q. このプロジェクトは公式ツールですか？

A. いいえ。OBS Duel Recorder は非公式のファンメイドツールです。KONAMI とは無関係です。

### Q. ゲーム画像やカード画像は同梱されていますか？

A. いいえ。Yu-Gi-Oh! Master Duel の画像、音声、動画、テンプレート画像、学習データは配布しません。検出用テンプレートやサンプルは利用者がローカルで用意します。

### Q. どの環境を想定していますか？

A. Windows x64 と OBS Studio x64 を前提にしています。Worker はローカルホストで動作し、OBS Plugin は Worker API を通じて状態確認や操作を行います。

## セットアップ

### Q. 最初に何を確認すればよいですか？

A. [インストール](install.md)、[初回セットアップ](first-setup.md)、[OBS セットアップ](obs-setup.md)、[YouTube セットアップ](youtube-setup.md) の順に確認してください。Worker の状態は `/setup/status` と `/setup/validate` で確認できます。

### Q. `user_data/` は何のためにありますか？

A. 設定、SQLite DB、動画、スクリーンショット、エクスポート、ログを保存するローカル実行データ領域です。更新時にも保持される前提なので、削除や上書きは慎重に行ってください。

### Q. 更新前にバックアップは必要ですか？

A. v1.4 以降は update API が DB backup-before-migration を行います。ただし重要な録画や設定がある場合は、更新前に `user_data/` 全体を別の場所へコピーしておくと復旧しやすくなります。

## YouTube

### Q. YouTube にアップロードできない場合は何を確認しますか？

A. `/upload/status` で OAuth client secret と token の有無、queue state、manual action を確認してください。認証不足は `need_manual_review`、quota は `quota_waiting`、通信失敗は `upload_failed` として扱われます。

### Q. Google 認証情報はリポジトリに入れますか？

A. 入れません。OAuth token、client secret、authorization code、bearer token は `user_data/config/secrets/` などのローカル領域で扱い、GitHub Pages、release ZIP、issue、PR には含めません。

## キュー

### Q. キューに項目が残るのはなぜですか？

A. アップロード待ち、通信失敗、quota 待ち、認証不足、手動確認待ちなどの状態があるためです。`/queue/items` と `/upload/status` で状態を確認し、必要に応じて retry、discard、mark_uploaded を使います。

### Q. 失敗が続く場合はどうしますか？

A. 連続 retry の前に `last_error_code`、`manual_review_reason`、YouTube quota、OAuth token、動画ファイルの存在を確認してください。曖昧な失敗は二重アップロード防止のため手動確認に降格します。

## 認識と統計

### Q. v2.0 の OCR Integration は実OCRですか？

A. v2.0 は画像認識先行です。Worker は deterministic fixture provider から result、rank、DP の候補を返し、確定・修正・却下を監査レコードとして保存します。重いOCR/ML依存は必須ではありません。

### Q. 統計はどこから計算されますか？

A. v2.1 の統計は SQLite の match metadata と upload queue state から読み取り専用で計算します。履歴レコードを統計のために書き換えることはありません。
