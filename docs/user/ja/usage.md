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

OBS Dock から手動 Start/Stop を使えます。失敗時は `/recording/state` と Dock の diagnostic state を確認してください。

## Match metadata

match metadata には deck name、opponent deck、result、rank、DP、memo、title template を保存できます。YouTube upload metadata はこの情報から生成されます。

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
