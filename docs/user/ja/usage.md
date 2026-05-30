# 使い方

-> [日本語ユーザードキュメント](index.md)

このページは録画、キュー、メタデータ、認識候補、統計の基本的な利用方法をまとめます。

## 自動録画

自動録画は detection/template matching の結果に基づきます。認識候補 API は録画トリガではありません。

基本フロー:

1. duel start を検出します。
2. Worker recording state が `starting` になります。
3. OBS Plugin が OBS 録画開始を要求します。
4. duel end を検出します。
5. Worker が upload queue item を作成します。

## 手動操作

OBS Dock から手動 Start/Stop を使えます。

v1.1.3 以降の Dock は通常の流れに合わせて並びます。

- `Record`: 録画状態、出力の紐づき、手動開始/停止
- `Metadata`: 最新対戦の deck/opponent deck、result、rank、DP、memo の直接編集
- `Upload`: アップロードキュー操作、選択中動画の確認、YouTube連携状態、OAuth操作
- `Template`: タイトル/説明文/タグテンプレートの編集、プレビュー
- `Manage`: セットアップ状態、設定、ヘルプ、自動録画セットアップ、詳細診断

失敗時は Manage の詳細診断、または `/recording/state` を確認してください。

## Match metadata

match metadata には deck name、opponent deck、result、rank、DP、memo、title template、description template、tags template を保存できます。

Deck と opponent deck は手入力できるプルダウンです。保存した値は候補として残り、OBS や Worker を再起動しても再利用できます。

Deck、opponent deck、rank、DP は次の match が空欄の場合に前回保存値を引き継ぎます。必要に応じて直接書き換えられます。

YouTube upload metadata は Worker が同じテンプレートから生成します。Template タブのプレビューでタイトル、説明文、タグ、警告を確認してからアップロードしてください。

## 認識候補

v2.0 以降、Worker は result、rank、DP の認識候補を返せます。候補は自動で match metadata を上書きしません。

主な操作:

- 候補確認: `/recognition/candidates`
- 確定: `confirm`
- 修正: `correct`
- 却下: `reject`

## 統計

v2.1 以降、Worker は読み取り専用の統計 API を提供します。

- `/statistics/summary`
- `/statistics/decks`
- `/statistics/opponents`
- `/statistics/uploads`
- `/statistics/memos?query=...`

統計はローカル SQLite の match metadata と upload queue state から計算されます。match record は書き換えず、OAuth secrets や local media path は返しません。

Result は `win` / `loss` / `draw` / `unknown` に分類します。Win rate は known results のみを分母にします。Memo search は local case-insensitive partial search です。
