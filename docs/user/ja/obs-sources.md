# Plugin が使う OBS Sources

ステータス注記: このページは、現在の Plugin が使う OBS Text Source の動作と v1.1.1 のドキュメント目標を説明します。

OBS Duel Recorder Plugin は、overlay 表示用に少数の OBS Text Sources を作成または再利用できます。これらは任意の表示補助です。動画ファイル、ローカル検出テンプレート、スクリーンショット、OAuth ファイル、ゲーム資産ではありません。

## Source 一覧

| Field | 既定の OBS source name | 既定テキスト | 意味 |
|---|---|---|---|
| deck name | `ODR Deck Name` | `Deck: -` | ユーザー向けのデッキ表示です。 |
| sequence number | `ODR Sequence` | `#---` | 現在または次の録画番号表示です。 |
| result | `ODR Result` | `Result: unknown` | 対戦結果表示です。ユーザー確認または認識候補の反映までは unknown のままになる場合があります。 |
| opponent deck | `ODR Opponent Deck` | `Opponent: unknown` | 相手デッキ表示です。 |
| recording state | `ODR Recording State` | `Idle` | 表示専用の録画状態テキストです。 |

上記は OBS 上の source name です。ドキュメント、スモークテスト、トラブルシュートで見つけられるように安定させています。

## 作成と再利用

overlay が有効な場合:

1. Plugin は設定済み source name を確認します。
2. その名前の対応 OBS Text Source がすでにある場合、Plugin は再利用します。
3. source がなく `auto_create_sources` が有効な場合、Plugin は現在の OBS scene に不足している Text Source を作成します。
4. `auto_create_sources` が無効な場合、Plugin は missing source を診断として報告し、その field をスキップします。
5. 同じ configured name の source が複数ある場合、Plugin は duplicate-source diagnostic を報告し、その field をスキップします。
6. configured name の source が存在しても対応 text source ではない場合、Plugin は unsupported-source diagnostic を報告し、置換や削除はしません。

対応 source kind は OBS build によって異なり、`text_gdiplus` または `text_ft2_source` などの OBS text source です。

## 更新動作

Plugin は Worker overlay state をもとに configured source へテキストを書き込みます。

| Source | 更新元 |
|---|---|
| `ODR Deck Name` | deck name payload または configured default |
| `ODR Sequence` | sequence number payload または configured default |
| `ODR Result` | result payload または configured default |
| `ODR Opponent Deck` | opponent deck payload または configured default |
| `ODR Recording State` | `idle`, `recording`, `paused`, `unknown` の mapping または configured default |

Worker は OBS API を直接呼びません。Worker は overlay state を保持し、Plugin が OBS Text Sources へ反映します。

## 安全なカスタマイズ

安全:
- OBS preview 上で Text Sources を移動する。
- OBS 上で resize、crop、color、font-style など見た目を調整する。
- 表示したくない source を非表示にする。
- 作成後に overlay 専用 scene や group へ整理する。

注意が必要:
- source name を変更すると、settings 側も同じ名前にしない限り Plugin が見つけられません。
- 同じ名前で source を複製すると、更新対象が曖昧になります。
- source type を text 以外に変えると更新できません。
- source を削除しても構いませんが、auto-create が有効で current scene が対象の場合、Plugin が後で再作成する場合があります。

## Plugin が作成しないもの

Plugin は以下を作成・配布しません。
- Yu-Gi-Oh! Master Duel の画像やゲーム資産
- ローカル start/end 検出テンプレート画像
- スクリーンショットや録画動画
- YouTube 用 browser source
- OAuth client ファイル、token、secret
- OBS scene、scene collection、transition

## トラブルシューティング

| 症状 | 主な原因 | 対応 |
|---|---|---|
| source が見えない | 非表示、他 source の背面、canvas 外、別 scene にある | current scene、source visibility、transform を確認します。 |
| source text が更新されない | Worker 停止、source name 変更、unsupported source type、duplicate names | Dock diagnostics を確認し、期待される source name に戻します。 |
| source が作成されない | auto-create 無効、または OBS に対応 text source kind がない | source creation を有効にするか、対応 Text Source を手動作成します。 |
| duplicate diagnostic が出る | 同じ configured name の source が複数ある | 各 expected name が 1 つだけになるよう rename または削除します。 |

関連ページ:
- [操作フローとシステム概要](operation-flow.md)
- [初回セットアップ](first-setup.md)
- [トラブルシューティング](troubleshooting.md)
