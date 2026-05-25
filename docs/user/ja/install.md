# インストール

このページは、OBS Duel Recorder を Windows + OBS Studio x64 環境で使い始めるための手順です。

ステータス注記: 現在の実用頒布形態は GitHub Release ZIP です。Installer/MSI はまだ提供しません。

## 必要なもの

- Windows x64
- OBS Studio x64
- GitHub Releases の ZIP
- ZIP と同じ Release にある `SHA256SUMS.txt`

> 注意: 本プロジェクトは Yu-Gi-Oh! Master Duel のアセットを配布しません。

## チェックサム確認

ZIP を展開する前に、PowerShell で SHA256 を確認します。

```powershell
Get-FileHash .\obs-duel-recorder-v1.0.1.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

表示された hash が一致していれば、ZIP を展開します。

## OBS への配置

ZIP を任意の作業フォルダへ展開してから、Plugin DLL と Worker bundle を OBS Studio の配置先へコピーします。

ZIP 展開後の主な配置元は次の通りです。

```text
<ZIP展開先>\app\plugin\obs-duel-recorder.dll
<ZIP展開先>\app\worker\odr-worker\odr-worker.exe
<ZIP展開先>\app\worker\odr-worker\<その他の Worker bundle ファイル>
```

OBS 側の正しい配置先は次の通りです。

```text
<OBSインストール先>\
`-- obs-plugins\
    |-- 64bit\
    |   `-- obs-duel-recorder.dll
    `-- worker\
        `-- odr-worker\
            |-- odr-worker.exe
            `-- <その他の Worker bundle ファイル>
```

コピー対応は次の通りです。

```text
<ZIP展開先>\app\plugin\obs-duel-recorder.dll
  -> <OBSインストール先>\obs-plugins\64bit\obs-duel-recorder.dll

<ZIP展開先>\app\worker\odr-worker\
  -> <OBSインストール先>\obs-plugins\worker\odr-worker\
```

`odr-worker.exe` だけではなく、`odr-worker` ディレクトリ全体をコピーしてください。Worker は実行ファイルの隣にある bundle 内の依存ファイルも必要です。

Portable OBS の場合も同じ考え方で、portable OBS フォルダ直下の `obs-plugins` に配置します。

## よくある誤配置

Worker を `obs-plugins\64bit\worker` 配下に置かないでください。

誤った配置例:

```text
<OBSインストール先>\
`-- obs-plugins\
    `-- 64bit\
        |-- obs-duel-recorder.dll
        `-- worker\
            `-- odr-worker\
                `-- odr-worker.exe
```

通常の ZIP 配置では、Plugin は `64bit` の兄弟ディレクトリにある Worker を探します。

```text
<OBSインストール先>\obs-plugins\worker\odr-worker\odr-worker.exe
```

Worker を `64bit` の下へ入れてしまうと、OBS Duel Recorder Dock は表示されても Worker 起動に失敗します。この場合、Start / Stop は Worker state が `running` にならないためグレーアウトしたままになります。

## 初回起動

1. OBS を起動します。
2. `OBS Duel Recorder` Dock が表示されることを確認します。
3. Dock の Worker 状態が `running` になることを確認します。
4. 必要に応じて `Tools > OBS Duel Recorder Settings` から `user_data_dir` を変更します。

`user_data_dir` を明示しない場合、既定では次を使います。

```text
%APPDATA%\obs-duel-recorder\user_data
```

このフォルダには設定、DB、ログ、動画、スクリーンショット、エクスポートが保存されます。アプリ本体や OBS プラグイン配置先とは分離してください。

## Start / Stop がグレーアウトしている場合

1. Worker bundle が `<OBSインストール先>\obs-plugins\worker\odr-worker\odr-worker.exe` にあるか確認します。
2. `<OBSインストール先>\obs-plugins\64bit\worker\` 配下に誤って置いていないか確認します。
3. 配置を直した後、OBS を再起動します。
4. Dock の Worker 状態と OBS ログの Worker launch error を確認します。

## 次にやること

- [初回セットアップ](first-setup.md)
- [OBS セットアップ](obs-setup.md)
- [トラブルシューティング](troubleshooting.md)
