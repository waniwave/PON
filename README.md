# PON — 円形パッド・カオスエフェクター

ドラムループやサンプルに挿して、**大きな円形パッド 1 つ**で一気に変な音にするエフェクトです。
中身は 3 バンドの **ワウ／パン／ディレイ／グラニュラー**。すべてテンポに同期し、
**どれだけ壊してもビート（キックの重さ、アタック、拍の頭）は崩れない**ように作ってあります。

- 形式: Windows VST3 / macOS VST3 + AU（Universal: Apple Silicon + Intel、macOS 11.0 以降）
- 入出力: ステレオ → ステレオ（モノ → ステレオも可）
- レイテンシー: 3ms（アタック検出の先読み。ホストに報告済みなので自動で補正されます）

---

## 円形パッドの使い方

玉の **角度 = 拍の割り方（ANGLE）**、**中心からの距離 = カオス量（RADIUS）** です。
どちらもホストからオートメーションできます。**オートメーションでパッドを動かす使い方がおすすめ**です。

| スポーク（真上から時計回り） | 表示 | 1 拍あたりの回数 R |
|---|---|---|
| 1 | 1/2 | 0.5 |
| 2 | 1/4 | 1 |
| 3 | 1/8 | 2 |
| 4 | 3連 | 3 |
| 5 | 1/16 | 4 |
| 6 | 5連 | 5 |
| 7 | 6連 | 6 |
| 8 | 7連 | 7 |

- ワウ・パン・ディレイ・グレインのタイミングは、すべて「R × プリセットがバンドごとに決めた倍率」で決まります。
- **SNAP オン**（デフォルト）: 一番近いスポークにスナップ。選ばれているスポークが光ります。
- **SNAP オフ**: 隣のスポークの間をなめらかにつなぎます（3連と5連の間のような「拍が溶ける」音）。7連と 1/2 の間は境界で、真ん中でどちらかに切り替わります。
- **RADIUS**: 中心 = エフェクトなし（出力 = 入力）、外周 = プリセットが決めた最大の効き。0〜30% はゆるやかで、そこから急に効きます。パンの左右比（例 3:2）も外に行くほど強くなります。
- 外周の外へドラッグしても玉は外周に留まります。**中心付近をダブルクリック**すると玉が中心に戻ります。
- パッドの下側の 3 つの点（LOW / MID / HIGH）は、各バンドが今どれだけ左右に振れているかを示します。ポリリズムでずれていく様子が見えます。

### ビートを守る仕組み（常に動作）

1. **アタック保護** — ドラムのアタック（約 30ms）は原音を優先。エフェクトは主に余韻にかかります（強さ = PUNCH）。
2. **サブの固定** — 120Hz 以下はモノラルのまま、何のエフェクトも通しません。
3. **ウェットのダッキング** — ドラムが鳴った瞬間はディレイ/リバーブを引っ込め、隙間で戻します（リリースは拍に合わせて自動）。
4. **頭拍アンカー（ANCHOR）** — 各小節の 1 拍目の瞬間（約 30ms）だけパンを中央に寄せます。
5. **全部テンポ同期** — LFO の位相はホストの再生位置から毎回計算。再生位置を飛ばしても拍のグリッドにそろいます。

## ノブとボタン

| 名前 | 範囲 | 説明 |
|---|---|---|
| MIX | 0〜100% | ドライとウェットの比率（デフォルト 100%）。0% で出力 = 入力 |
| SPACE | 0〜100% | 空間系（Haas の広がり + 小型リバーブ、プリディレイはテンポ同期）の量。ウェットにだけかかります |
| CHAOS | 0〜100% | ランダム性（パンの行き先の入れ替え、グレインの間引き/追加、ワウの揺らぎなど）。**再生位置で決まる乱数**なので、同じ場所は何度書き出しても同じ音です |
| PUNCH | 0〜100% | アタック保護とダッキングの強さ。上げるほどドラムの芯が残ります（デフォルト 60%） |
| OUTPUT | -24〜+12dB | 出力音量。最後に安全リミッター（-0.1dBFS）が常にかかります |
| SNAP | On/Off | 角度のスナップ |
| ANCHOR | On/Off | 頭拍アンカー |
| ◀ ▶ / プリセット | — | 「カテゴリ > 名前」から選択。◀▶ で前後へ。PRESET もオートメーション可能（約 50ms のクロスフェード） |
| Save / Load | — | ユーザープリセット（`.ponpreset`、XML）。保存先: Windows `ドキュメント\PON\Presets\`、macOS `~/Documents/PON/Presets/` |

- ノブはダブルクリックでデフォルト値、マウスホイールでも動き、ホバーで値を表示します。

### 詳細パラメーター（DETAIL）

上部の **DETAIL ▶** を押すと、ウィンドウが右に広がり、プリセットの中身（内部パラメーター）を直接いじれます。もう一度押すと閉じます（開閉状態もプロジェクトに保存）。

| タブ | 主な項目 |
|---|---|
| BANDS | RADIUS カーブ、各バンドのオン/オフ、レート倍率 |
| WAH | 深さ、配分、周波数の下端/上端、Q、LP ブレンド、LFO の形、逆ワウ、位相、ワウだけの倍率 |
| GRAIN | 量、ピッチの下降/揺れ、配分、サイズ、密度、同時数、ピッチ、散らばり、オクターブ限定、読み出し位置/ずれ、逆再生確率、余韻のみ、左右の幅 |
| PAN | 振れ幅、擬似スペクトル回転、配分、L:R 比、位相、なめらかさ、Haas |
| DELAY | 量、フィードバック、ピンポン、ループ内フィルター（種類/周波数/Q）、ループ内ワウ、逆再生、出力のローカット/ハイカット、バンドごとの送り量と分割 |
| SPACE | リバーブの大きさ、ダンプ、プリディレイ（音符）、Haas、ウェット全体のローカット/ハイカット |

- 1 行 = 1 項目、列 = LOW / MID / HIGH。全体にかかる項目は 3 列ぶち抜きで表示されます。
- 編集はすぐ音に反映されます（クロスフェードなし）。プリセット名に「*」が付きます。
- バーは**ダブルクリックで読み込んだプリセットの値**に戻ります。**REVERT** で全部戻ります。同じプリセットを選び直しても戻ります。
- 編集した値は DAW のプロジェクトと、Save したユーザープリセットに保存されます。
- 詳細パラメーターは GUI 専用で、DAW のオートメーション対象にはしていません（オートメーションは ANGLE / RADIUS などのメインのパラメーターで行います）。
- プリセットを選ぶと SPACE / CHAOS / PUNCH が推奨値に変わります（ANGLE・RADIUS・MIX・OUTPUT・SNAP・ANCHOR は変わりません）。
- DAW のプロジェクトには内部パラメーターを含む全状態が保存されます。

## プリセット一覧（5 カテゴリ × 9）

倍率は Low / Mid / High のレート倍率（R に掛ける）。「—」はそのバンドに効かせない。

### DRIFT（ワウ中心・軽め）
| 名前 | 倍率 | 説明 |
|---|---|---|
| ぬるい扇風機 | 1/4・1/4・1/4 | 全帯域が同じ周期でゆっくり首振り。Q低めの入門用ワウ |
| 水槽ワウ | —・1/2・— | 中域だけ深いワウ。泡っぽく丸い音 |
| 二日酔い | 1/4・1/3・3/8 | 帯域ごとに周期が少しずつずれ、数小節かけてうねる |
| 逆ワウ | 1/2・1/2・1/2 | 逆ワウモード。音が大きいほど閉じ、隙間で開く |
| ラジオ越し | 1/2・1/2・1/2 | ウェットを300Hz〜3kHzに絞ってワウ。ローファイ |
| ハットだけ喋る | —・—・2 | 高域だけ速いワウ |
| 午後のカーテン | 1/2・1/2・1/2 | 薄いワウに薄いディレイ。BGM向け |
| ゴムの壁 | 1/2・1・1 | 高レゾナンスで粘る共振ワウ |
| 3枚刃 | 1・3・2 | 低・中・高が4分・3連・8分の別周期で揺れる |

### ORBIT（パン中心・ポリリズム）
| 名前 | 倍率 | 説明 |
|---|---|---|
| 振り子 | 1・1・1 | 左右1:1の素直な往復。基準の音 |
| 左3右2 | 1・1・1 | L:R＝3:2の偏ったパン |
| 5連の衛星 | —・1・5 | 高域だけ5倍速で周回し、中域はゆったり回る |
| 七つ星 | 3・5・7 | 3・5・7のポリリズムでパン。看板プリセット（起動時のプリセット） |
| 交差点 | 1・1・1 | 中域だけ位相反転。帯域同士が逆方向にすれ違う |
| 千鳥足 | 1・1・2 | 行き先がランダムに入れ替わり、よろける |
| ヤジロベエ | 2・2・2 | 中央付近で小刻みに揺れる |
| ステレオ迷子 | 1・1・1 | 拍に合わせたパンに、Haasの幅（最大25ms）の切り替えを足す |
| 回転寿司 | 1/2・1/2・1/2 | 擬似スペクトル回転。3帯域が120°ずつずれて回り、音像が螺旋状に回る |

### ECHO（ディレイ中心）
| 名前 | 倍率 | 説明 |
|---|---|---|
| オウム返し | 1・1・1 | 1拍遅れで1回だけ返す（フィードバックなし） |
| 階段エコー | 4/3・4/3・4/3 | 付点8分。繰り返すたびに暗くなる |
| ピンポン迷子 | 1・3/2・2 | 帯域ごとに違う分割でピンポン |
| 5連こだま | 5・5・5 | 5連のディレイ |
| トンネル | 1/2・1/2・1/2 | フィードバック85%、LP 1.5kHzの暗いディレイ |
| 溶ける鏡 | 1・1・1 | ディレイのループ内でワウが動く |
| ダブ部屋 | 4/3・4/3・4/3 | ダブ風。フィードバック75%、HPで低域を逃がす |
| 電話の向こう | 2・2・2 | ディレイ音を500Hz〜2.5kHzに絞る |
| 逆回転の気配 | 1・1・1 | ディレイ音だけが逆再生のグレインになる |

### DUST（グラニュラー中心）
| 名前 | 倍率 | 説明 |
|---|---|---|
| 砂嵐 | —・4・4 | 20ms前後の細かいグレインを高密度で |
| 粉ハット | —・—・4 | 高域だけグラニュラー |
| 霧のスネア | —・2・2 | 余韻のみモード。アタックの後だけグレインになる |
| ガラスの雨 | —・2・4 | ピッチを+7〜+12半音に散らしたグレイン |
| 金平糖 | —・2・2 | +12半音のグレインがきらめく |
| 地下鉄 | —・1・— | 中域を-12半音のグレインで唸らせる |
| 呼吸する壁 | —・1/2・1/2 | 200〜400msの長いグレインでパッド状にする |
| ほつれ | —・2・2 | 読み出し位置が大きくランダムにずれる |
| 古いVHS | 1/4・2・2 | 粗いグレインに、ゆっくりしたピッチの揺れ |

### CHAOS（全部入り）
| 名前 | 倍率 | 説明 |
|---|---|---|
| 回る部屋 | 1/2・1/2・1 | パンと空間が主役。部屋ごと回る |
| 洗濯機 | 2・3・4 | ワウ・パン・ディレイが速く絡む |
| 宇宙酔い | 3・5・7 | 全帯域が3・5・7のポリリズムでパンし、ディレイも連動 |
| 夜の遊園地 | 1・2・3 | オクターブ違いのグラニュラーとピンポンディレイ |
| 万華鏡 | —・2・3 | ピッチ違い（±12半音）のグレインが左右に散る |
| ゲームセンター | —・4・4 | 細かい刻みのグレインと速いワウでピコピコ |
| 混線 | 1・2・3 | 低域はワウ、中域はディレイ、高域はグレインと、帯域ごとに別のエフェクトだけを鳴らす |
| 電池切れ | 1/4・1/4・1/4 | 全部が遅く暗く、グレインのピッチがゆっくり下がっていく |
| 粉砕 | 2・3・5 | 全部最大。それでもPUNCHでビートは聞き取れる |

プリセットは `Source/FactoryPresets.cpp` にデータとしてまとまっています。`kDefs` に 1 行足せば追加できます。

---

## FL Studio での使い方

1. **スキャン**: `Options > Manage plugins` を開き、`Find installed plugins`（または `Find more plugins`）を押す。PON が一覧に出たら ☆ を付けておくと探しやすくなります。
2. **ミキサーに挿す**: ドラムループのチャンネルをミキサーのトラックに送り、そのトラックのエフェクトスロットの ▼ から `PON` を選ぶ。
3. **パッドのオートメーション**:
   - パッドの玉を少し動かす → `Tools > Last tweaked > Create automation clip` で、最後に触ったパラメーター（ANGLE または RADIUS）のクリップができます。もう一方も同じ手順で作ります。
   - または FL の「Browse parameters」で PON の `Angle` / `Radius` を右クリック → `Create automation clip`。
   - 例: RADIUS を 0 → 100% にゆっくり上げると、普通のループが少しずつ壊れていきます。ANGLE を拍ごとに切り替えると割り方が次々変わります（SNAP オンなら必ずスポークに乗ります）。
4. PRESET もオートメーションできます（切り替えは約 50ms のクロスフェード）。

## ビルド（Windows）

必要なもの: Visual Studio 2022（C++ によるデスクトップ開発）。CMake は VS 付属のものを自動で探します。
JUCE 8.0.4 は HazePad のビルドに既にあるものを読み取り専用で再利用し、なければ自動ダウンロードします。

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1          # VST3 をビルド
powershell -ExecutionPolicy Bypass -File .\install.ps1        # C:\Program Files\Common Files\VST3 へコピー（管理者の許可が必要）
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Tests   # 検証ツールもビルド
build\PONRenderTest_artefacts\Release\PONRenderTest.exe build\renders   # 検証を実行（WAV も書き出し）
powershell -ExecutionPolicy Bypass -File .\package.ps1        # 配布用 ZIP（dist\PON-1.0.0-win64.zip）
```

成果物: `build\PON_artefacts\Release\VST3\PON.vst3`

## macOS 版のビルド

### A. Mac を持っていない場合（GitHub Actions）

1. GitHub に新しいリポジトリを作り、この `PON` フォルダの中身（`build/` 以外）をアップロードする（Web 画面の「Add file > Upload files」で可。`.github` フォルダも含めること）。
2. push すると `.github/workflows/build.yml` が自動で動きます（Actions タブの「Run workflow」でも実行可）。
   - Windows: VST3 のビルド → 検証ツール → pluginval（strictness 5）→ ZIP
   - macOS: Universal の VST3 + AU のビルド → 検証ツール → アドホック署名 → auval → ZIP
3. 終わったら Actions の実行結果ページ下部の **Artifacts** から `PON-mac`（`PON.vst3`、`PON.component`、ZIP）をダウンロードします。

### B. Mac で直接ビルドする場合

Xcode（コマンドラインツール）と CMake（`brew install cmake`）を入れて:

```bash
./package_mac.sh            # build-mac/ にビルドし、アドホック署名して dist/PON-1.0.0-mac.zip を作る
./package_mac.sh --tests    # 検証ツールも実行
cp -R build-mac/PON_artefacts/Release/VST3/PON.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R build-mac/PON_artefacts/Release/AU/PON.component ~/Library/Audio/Plug-Ins/Components/
auval -v aufx Pon1 Pnfx     # AU の検証
```

### macOS: 署名と公証（配布するとき）

**Developer ID で署名・公証していないプラグインは、ダウンロードして使うと Gatekeeper に止められます**
（「開発元を確認できない」「壊れているため開けません」など）。自分で使うだけなら、受け取った側で
`xattr -dr com.apple.quarantine <PON.vst3 / PON.component のパス>` を実行すれば読み込めます（`dist/README_mac.txt` に記載）。

一般に配布する場合は、Apple Developer Program（有料）に登録して次を行います。

1. **Developer ID Application 証明書**を作成し、キーチェーンに入れる。
2. ハードンドランタイム付きで署名する:
   ```bash
   IDENTITY="Developer ID Application: Your Name (TEAMID)"
   for B in build-mac/PON_artefacts/Release/VST3/PON.vst3 build-mac/PON_artefacts/Release/AU/PON.component; do
       codesign --force --deep --options runtime --timestamp --sign "$IDENTITY" "$B"
       codesign --verify --deep --strict --verbose=2 "$B"
   done
   ```
3. ZIP にして公証サービスへ送る（App 用パスワードは appleid.apple.com で発行）:
   ```bash
   xcrun notarytool store-credentials PON_NOTARY --apple-id "you@example.com" --team-id TEAMID --password "app-specific-password"
   ditto -c -k --keepParent build-mac/PON_artefacts/Release/VST3/PON.vst3 PON_vst3.zip
   ditto -c -k --keepParent build-mac/PON_artefacts/Release/AU/PON.component PON_au.zip
   xcrun notarytool submit PON_vst3.zip --keychain-profile PON_NOTARY --wait
   xcrun notarytool submit PON_au.zip   --keychain-profile PON_NOTARY --wait
   ```
4. 公証が通ったら、バンドルにチケットを添付（staple）してから配布用 ZIP / pkg を作る:
   ```bash
   xcrun stapler staple build-mac/PON_artefacts/Release/VST3/PON.vst3
   xcrun stapler staple build-mac/PON_artefacts/Release/AU/PON.component
   spctl --assess --type install -vv build-mac/PON_artefacts/Release/VST3/PON.vst3
   ```

GitHub Actions で自動化する場合は、証明書（.p12）とパスワード、Apple ID 情報をリポジトリの Secrets に入れ、
macOS ジョブの中でキーチェーンに読み込んでから上の手順を実行します。

---

## 技術メモ

- 信号の流れ・クラス設計は [DESIGN.md](DESIGN.md) を参照。
- オーディオスレッドではメモリ確保・ロック待ち・ファイル入出力をしません（プリセットの受け渡しは try-lock）。
- バッファはすべて `prepareToPlay` で確保（グラニュラー 2 小節 = 40BPM で 12 秒分、ディレイ 4 拍分）。
- 44.1〜192kHz、バッファサイズ 1〜4096 で同じ音・同じタイミングになることを `Tools/RenderTest.cpp` で確認しています。
