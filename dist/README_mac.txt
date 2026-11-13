PON - 円形パッド・カオスエフェクター (macOS VST3 + AU, Apple Silicon / Intel, macOS 11.0+)

■ インストール
  PON.vst3      → ~/Library/Audio/Plug-Ins/VST3/
  PON.component → ~/Library/Audio/Plug-Ins/Components/   （Logic Pro / GarageBand 用）

  Finder で「移動」>「フォルダへ移動」に上のパスを入力するとフォルダが開きます。

■ 「開発元を確認できない」「壊れている」と表示される場合
  このビルドは Apple の Developer ID で署名・公証されていないため、Gatekeeper に止められます。
  ターミナルで次を実行してから DAW を起動し直してください。

    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/PON.vst3
    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/PON.component

  Logic Pro で AU が出てこない場合は、ターミナルで次を実行して再起動してください。
    killall -9 AudioComponentRegistrar

ユーザープリセットの保存先: ~/Documents/PON/Presets/
