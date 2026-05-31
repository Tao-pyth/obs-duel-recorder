# Pluginが使用するOBSソース

このページでは、OBS Duel Recorder Pluginが使用するOBS Text Sourceの現在の動作を説明します。これらのソースは録画画面に文字を表示するための補助表示です。動画ファイル、検出テンプレート画像、スクリーンショット、OAuthファイル、ゲーム素材ではありません。

## ソース一覧

| 項目 | 既定のOBSソース名 | 既定表示 | 意味 |
|---|---|---|---|
| デッキ名 | `ODR Deck Name` | `Deck: -` | 自分のデッキ名です。 |
| 連番 | `ODR Sequence` | `#---` | 自分のデッキ名ごとの使用回数連番です。 |
| 勝敗 | `ODR Result` | `Result: unknown` | 対戦結果です。対戦終了後の確認までは `unknown` のままで構いません。 |
| 相手デッキ | `ODR Opponent Deck` | `Opponent: unknown` | 相手のデッキ名です。 |
| 録画状態 | `ODR Recording State` | `Idle` | 録画状態の表示です。 |

上記はOBS上のソース名です。ドキュメント、確認作業、トラブルシューティングで見つけやすいよう、安定した名前として扱います。

## 作成と再利用

Overlayが有効な場合、Pluginは次の順でOBS Text Sourceを扱います。

1. 設定済みのソース名を確認します。
2. 同じ名前の対応済みOBS Text Sourceが既にあれば再利用します。
3. ソースがなく、`auto_create_sources` が有効な場合は現在のOBSシーンへ作成します。
4. `auto_create_sources` が無効な場合は、欠落診断を出してその項目をスキップします。
5. 同じ名前のソースが複数ある場合は、曖昧なため更新をスキップします。
6. 同じ名前のソースがText Sourceではない場合は、置き換えずに更新をスキップします。

## 更新タイミング

WorkerはOBS APIを直接呼びません。WorkerはOverlay stateを保持し、Pluginがそれを取得してOBS Text Sourceへ反映します。

| ソース | 更新元 |
|---|---|
| `ODR Deck Name` | Worker overlay stateの `deck_name` |
| `ODR Sequence` | Worker overlay stateの `sequence_number` |
| `ODR Result` | Worker overlay stateの `result` |
| `ODR Opponent Deck` | Worker overlay stateの `opponent_deck` |
| `ODR Recording State` | Worker overlay stateの `recording_state` |

更新のきっかけは次の通りです。

- Plugin起動時に、設定済みOBS Text Sourceを作成または再利用します。
- Pluginは通常の更新ループでWorkerのOverlay stateを確認し、前回値と差分がある場合だけOBSへ書き込みます。
- Dockから録画開始したとき、現在の自分デッキ名と次のデッキ別連番をWorker overlay stateへ送り、同じ値をOBS Text Sourceへ即時反映します。
- メタデータを保存したとき、WorkerはDB保存後にデッキ名、相手デッキ名、連番をOverlay stateへ反映します。Pluginは保存後の値をOBS Text Sourceへ即時反映します。
- 録画中で、まだ編集対象のmatch行が作られていない場合は、Dockの保存操作で現在の入力欄からOBS Text Sourceだけを更新します。編集対象動画が表示された後、DBへ保存するためにもう一度保存してください。
- 録画状態は、手動録画または自動録画の状態変更時にWorker側で更新されます。
- 勝敗は最後に分かる情報なので、確認前は `unknown` のままで問題ありません。

デッキ別連番は、アップロードキューIDやmatch IDとは別の番号です。メタデータ保存時に自分のデッキ名ごとに採番します。保存済みの対戦を別デッキ名へ変更した場合は、新しいデッキ名側の次番号を割り当てます。過去番号は詰め直しません。

## 安全なカスタマイズ

安全です。

- OBSプレビュー上でText Sourceを移動する。
- OBS上でサイズ、色、フォント、配置を調整する。
- 表示したくないソースを非表示にする。
- 作成後に専用のOverlayシーンやグループへ整理する。

注意が必要です。

- ソース名を変更すると、設定も変更しない限りPluginが見つけられません。
- 同じ名前のソースを複数作ると、どれを更新すべきか判断できません。
- Text Source以外の種類へ変更すると更新できません。
- ソースを削除しても構いませんが、自動作成が有効な場合は後で再作成される場合があります。

## Pluginが作成しないもの

Pluginは次のものを作成・配布しません。

- Yu-Gi-Oh! Master Duelの画像やゲーム素材
- ローカルの開始・終了検出テンプレート画像
- スクリーンショットや録画動画
- YouTube用のBrowser Source
- OAuthクライアントファイル、トークン、シークレット
- OBSシーン、シーンコレクション、トランジション

## トラブルシューティング

| 症状 | 主な原因 | 対応 |
|---|---|---|
| ソースが見えない | 非表示、背面、キャンバス外、別シーンにある | 現在のシーン、表示状態、変換を確認します。 |
| ソース文字が更新されない | Worker停止、メタデータ未保存、ソース名変更、未対応ソース、重複名 | メタデータを保存し、Dock診断とソース名を確認します。 |
| ソースが作成されない | 自動作成が無効、またはOBSに対応Text Sourceがない | 自動作成を有効にするか、対応Text Sourceを手動作成します。 |
| 重複診断が出る | 同じ名前のソースが複数ある | 期待される名前のソースを1つだけにします。 |

関連ページ:

- [操作フロー](operation-flow.md)
- [初回セットアップ](first-setup.md)
- [トラブルシューティング](troubleshooting.md)
