# PON — 設計メモ

## ファイル構成

```
PON/
├─ CMakeLists.txt            JUCE 8.0.4 + CMake（Windows: VST3 / macOS: VST3+AU, Universal, 11.0+）
├─ build.ps1 / install.ps1   Windows ビルド / C:\Program Files\Common Files\VST3 へのコピー（HazePad と同じ方式）
├─ package.ps1 / package_mac.sh   配布用 ZIP（Cheapbeat と同じ方式）
├─ .github/workflows/build.yml    Windows + macOS の Release ビルド（成果物: .vst3 / .component）
├─ Source/
│  ├─ Dsp.h                  小物 DSP（TPT-SVF、リングバッファ、決定論的乱数、カーブ、レート折り返し）
│  ├─ PresetData.h/.cpp      内部パラメーター構造体（X マクロで定義 → XML 保存/読込を自動生成）
│  ├─ FactoryPresets.h/.cpp  45 個のファクトリープリセット（データ＋音作り）
│  ├─ BandFx.h/.cpp          1 バンド分の ワウ → グラニュラー → パン
│  ├─ SyncDelay.h/.cpp       テンポ同期ステレオディレイ（バンドごと）。クロスフェード切替・ループ内フィルター/ワウ・逆再生
│  ├─ Space.h                Haas（M/S 疑似ステレオ）＋ 小型 FDN リバーブ（HazePad の FdnReverb を元に軽量化）
│  ├─ Engine.h/.cpp          全体の信号処理（先読み・分割・ビート保護・ミックス・リミッター・プリセット切替）
│  ├─ Params.h/.cpp          APVTS のパラメーター定義
│  ├─ PluginProcessor.h/.cpp AudioProcessor（状態保存、プリセット受け渡し、ユーザープリセット）
│  ├─ PonLookAndFeel.h       配色・ノブ・ボタン描画、日本語フォント
│  ├─ PadComponent.h/.cpp    円形パッド
│  └─ PluginEditor.h/.cpp    GUI 全体
└─ Tools/RenderTest.cpp      オフライン検証（ヌルテスト、NaN、ブロックサイズ、サンプルレート、全プリセットの描画と測定）
```

## クラス設計

- `PonProcessor` … APVTS（ユーザーパラメーター 10 個）と `Engine` を持つ。
  - プリセットの内部データ (`PresetData`、float だけの POD) は、
    - ファクトリー: PRESET パラメーターの変化をオーディオスレッドが検出して直接コピー
    - ユーザー/状態復元: メッセージスレッドが `SpinLock` 付きの受け渡し枠に書き、オーディオスレッドは `ScopedTryLock`（待たない）で取り込む
  - SPACE/CHAOS/PUNCH の推奨値はメッセージスレッドのタイマー（Processor 自身が持つ）で書き込む。
- `Engine` … `prepare()` で全バッファを確保。`process()` はサンプル単位でテンポ位置を計算。
- `BandFx` ×3、`SyncDelay` ×3、`Space` ×1、`Limiter`。

## 信号の流れ（1 サンプル）

```
入力 x ─┬─ トランジェント検出（遅延なしの信号 = 3ms 先読み）→ gate（アタック）, duck（ダッキング）
        └─ 3ms 遅延 → xd ─┬────────────────────────────── 純ドライ（ヌル用）
                           └─ LR4 分割 250 → (120, 3k)
                                sub(<120, モノ) / low / mid / high
                                clean = sub+low+mid+high（= 位相補正済みドライ）
            low/mid/high: ワウ → [リング 2小節] → グラニュラー → パン(+Haas)
            inserts = sub(モノ) + Σbands
            sends   = Σ SyncDelay(band) + Space(bands+delay, アタック除外)
            wet     = lerp(inserts, clean, PUNCH×gate) + sends×(1-PUNCH×duck)
            mixed   = clean×(1-MIX) + wet×MIX
            out     = lerp(xd, mixed, engaged)   ← 中心かつ余韻なし / MIX0 なら純ドライ
            → OUTPUT → 安全リミッター（-0.1dBFS 天井）→ NaN/Inf チェック
```

- テンポ位置: `ppq = ホストPPQ - レイテンシー分`（停止中は 120BPM またはホストBPMで内部時計）。
  LFO の位相は `frac(ppq × rate)`（精度確保のため 48 拍周期で畳む。PON の有理数レートはすべて 48 拍で一周する）。
- CHAOS の乱数は `hash(周期番号, ステップ番号, 用途ごとの salt)` → 同じ位置なら同じ結果。
- プリセット切替: 旧 → clean（位相補正済み原音）→ 新 を 25ms + 25ms でクロスフェード。clean 区間でディレイ/リバーブ/グレインをリセット。
