# インストール

このページは、Windows + OBS Studio x64 で OBS Duel Recorder を使うための通常インストール手順です。

ステータス注記: 現在の配布形式は GitHub Release ZIP です。Installer / MSI はまだ提供していません。

## 必要なもの

- Windows x64
- OBS Studio x64
- GitHub Release ZIP
- 同じリリースに含まれる `SHA256SUMS.txt`

このプロジェクトは Yu-Gi-Oh! Master Duel の画像、動画、テンプレートなどのゲーム資産を配布しません。

## ZIPのチェックサム確認

ZIP を展開する前に、PowerShell で SHA256 を確認します。

```powershell
Get-FileHash .\obs-duel-recorder-v1.1.6.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

表示されたハッシュが `SHA256SUMS.txt` と一致する場合だけ、ZIP を展開してください。

## OBSへインストール

ZIP を作業フォルダへ展開し、展開したリリースZIPのルートで次を実行します。

```powershell
.\install.bat "C:\Program Files\obs-studio"
```

引数には OBS のインストール先ルートフォルダを指定します。Portable OBS の場合は Portable OBS のフォルダを指定します。

インストール補助スクリプトは次を行います。

- 指定されたフォルダがOBSルートに見えるか確認する
- Plugin DLL を `obs-plugins\64bit\` へコピーする
- Worker bundle 全体を `obs-plugins\worker\odr-worker\` へコピーする
- インストール後に検証スクリプトを実行する

`C:\Program Files\obs-studio` 配下へコピーする場合、Windowsの仕様により管理者権限が必要です。通常権限で実行した場合、補助スクリプトはUAC昇格を要求します。昇格できない場合は、PowerShellを「管理者として実行」してから同じコマンドを実行してください。

インストール前にOBSを閉じてください。OBSが起動中の場合、`obs-duel-recorder.dll` を安全に置き換えられないため、補助スクリプトはコピー前に停止します。

この補助スクリプトは runtime `user_data` を変更しません。設定、DB、ログ、OAuthトークン、録画ファイル、ローカルテンプレートは削除しません。

## 正しい配置

リリースZIP内の主なファイルは次の配置です。

```text
<ZIP展開先>\app\plugin\obs-duel-recorder.dll
<ZIP展開先>\app\worker\odr-worker\odr-worker.exe
<ZIP展開先>\app\worker\odr-worker\<その他のWorker bundleファイル>
```

OBS側の正しい配置は次のとおりです。

```text
<OBSインストール先>\
`-- obs-plugins\
    |-- 64bit\
    |   `-- obs-duel-recorder.dll
    `-- worker\
        `-- odr-worker\
            |-- odr-worker.exe
            `-- <その他のWorker bundleファイル>
```

`odr-worker.exe` だけをコピーしないでください。Workerは同じフォルダにある依存ファイルも必要です。`odr-worker` ディレクトリ全体をコピーしてください。

## インストール検証

インストール後、または手動コピー後に、展開したリリースZIPのルートで次を実行します。

```powershell
.\verify-install.bat "C:\Program Files\obs-studio"
```

検証スクリプトは次を確認します。

- package側のPlugin DLLが存在する
- package側のWorker bundleが揃っている
- OBS側のPlugin DLLが存在する
- OBS側のPlugin DLLのSHA256がpackageと一致する
- OBS側のWorker bundleが揃っている
- OBS側のWorker実行ファイルのSHA256がpackageと一致する
- `obs-plugins\64bit\worker\` という既知の誤配置が存在しない

ハッシュ不一致が出た場合、OBS側に古いファイルが残っています。OBSを閉じ、管理者PowerShellでインストール補助スクリプトを再実行し、もう一度検証してください。

## よくある誤配置

Worker を `obs-plugins\64bit\worker` の下に置かないでください。

誤った配置:

```text
<OBSインストール先>\
`-- obs-plugins\
    `-- 64bit\
        |-- obs-duel-recorder.dll
        `-- worker\
            `-- odr-worker\
                `-- odr-worker.exe
```

正しいWorkerパスは次です。

```text
<OBSインストール先>\obs-plugins\worker\odr-worker\odr-worker.exe
```

Workerが誤配置されている場合、OBS Duel Recorder Dock は表示されても Worker 起動に失敗します。Worker state が `running` にならないため、録画開始/停止などのボタンが無効のままになります。

## 初回起動

1. OBSを起動します。
2. `OBS Duel Recorder` Dock が表示されることを確認します。
3. Dock の Worker state が `running` になることを確認します。
4. 必要に応じて `Tools > OBS Duel Recorder Settings` から `user_data_dir` を変更します。

`user_data_dir` を明示しない場合、既定では次のフォルダを使います。

```text
%APPDATA%\obs-duel-recorder\user_data
```

runtimeデータは、OBSのPlugin配置先とは分けて管理してください。このフォルダには設定、DB、ログ、動画、スクリーンショット、エクスポートが保存されます。

## トラブルシューティング

Settings は開くが録画ボタンが無効のままの場合:

1. 展開したリリースZIPのルートで `.\verify-install.bat "<OBSインストール先>"` を実行します。
2. Worker bundle が `<OBSインストール先>\obs-plugins\worker\odr-worker\odr-worker.exe` にあるか確認します。
3. `<OBSインストール先>\obs-plugins\64bit\worker\` に誤配置されていないか確認します。
4. 検証が通ったらOBSを再起動します。
5. DockのWorker状態とOBSログのWorker launch errorを確認します。

## 次に読むもの

- [初回セットアップ](first-setup.md)
- [OBSセットアップ](obs-setup.md)
- [トラブルシューティング](troubleshooting.md)
