# 使い方

← [日本語ユーザードキュメント](index.md)

このページは、録画からアップロードまでの基本的な使い方をまとめます。

ステータス注記: このページには、未リリースの手順や UI 名称が含まれる可能性があります。利用可否は [ロードマップ](../../roadmap.md) で確認してください。

## 自動録画

システムは自動的に次の処理を行う想定です。

- デュエル開始を検知
- 録画開始
- デュエル終了を検知
- アップロードキューを作成

## 手動操作（概要）

Dock UI から次の操作ができる想定です。

- 手動開始
- 手動停止
- リトライ
- キュー復旧

## 対戦メモ

- 相手デッキの入力
- メモの追加
- メタデータ付きでアップロード

## 関連ページ

- [キュー / 履歴](queue.md)
- [トラブルシューティング](troubleshooting.md)

## Statistics

v2.1 では Worker の読み取り専用統計 API を追加します。

- `/statistics/summary`
- `/statistics/decks`
- `/statistics/opponents`
- `/statistics/uploads`
- `/statistics/memos?query=...`

統計はローカル SQLite の match metadata と upload queue state から計算されます。match record は書き換えず、OAuth secrets や local media path は返しません。

Result は `win` / `loss` / `draw` / `unknown` に分類します。Win rate は known results のみを分母にします。Memo search は local case-insensitive partial search です。

## TODO

- TODO: 具体的な画面/操作フロー（将来のUI確定後）
