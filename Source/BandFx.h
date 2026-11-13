#pragma once

#include "Dsp.h"
#include "PresetData.h"
#include <array>
#include <atomic>

namespace pon
{
// ブロック単位で一定の値
struct BlockCtx
{
    float  sr  = 48000.0f;
    double bpm = 120.0;
    double spb = 24000.0;   // samples per beat
    double R   = 1.0;       // パッドの角度から決まる「1拍あたりの回数」
};

// サンプル単位の値
struct SampleCtx
{
    double  ppq    = 0.0;   // 効果用のテンポ位置（kPhasePeriodBeats で畳んだもの）
    double  ppqAbs = 0.0;   // 畳む前
    int64_t period = 0;     // 畳んだ周期の番号（乱数用）
    float   cr     = 0.0f;  // RADIUS カーブ後の効き 0..1
    float   rlin   = 0.0f;  // RADIUS そのもの 0..1
    float   chaos  = 0.0f;  // CHAOS 0..1
    float   gate   = 0.0f;  // アタック検出 0..1
    float   anchor = 0.0f;  // 頭拍アンカー 0..1
};

// 1 バンド分: ワウ → グラニュラー → パン(+Haas)
class BandFx
{
public:
    void prepare (float sampleRate, int ringSamples);
    void reset() noexcept;
    void setParams (const PresetData& d, int bandIndex) noexcept;
    // 詳細パネルからの編集: 状態を保ったまま反映（新しく有効になった部分だけリセット）
    void setParamsLive (const PresetData& d, int bandIndex) noexcept;
    void beginBlock (const BlockCtx& b) noexcept;
    void process (const SampleCtx& c, float& L, float& R) noexcept;

    bool isOn() const noexcept { return on; }
    float getPanPosition() const noexcept { return panOut; }

private:
    static constexpr int kMaxGrains = 32;

    struct Grain
    {
        bool   alive = false;
        double off = 0.0;    // 書き込み位置からの距離（サンプル）
        double inc = 0.0;    // 1 サンプルごとの距離の変化
        int    t = 0, len = 1;
        float  gl = 1.0f, gr = 1.0f;
    };

    void spawnGrains (const SampleCtx& c, int64_t step, float trigSalt) noexcept;
    void startGrain (const SampleCtx& c, float r1, float r2, float r3, float r4, float r5) noexcept;

    // 設定
    BandParams p;
    int   bandIdx = 0;
    bool  on = false;
    float wahMax = 0, panMax = 0, grnMax = 0;
    float rotate = 0, rotRate = 0;
    float grnDrift = 0, grnDriftBars = 8, grnWob = 0, grnWobRate = 0.25f;

    float sr = 48000.0f;
    BlockCtx blk;

    // ブロックごとの計算値
    double wahRate = 0, grnRate = 0, panRate = 0;
    float  panSmoothCoef = 0.0f;
    float  grnNorm = 1.0f;
    float  wahSmoothCoef = 0.0f;

    // ワウ
    std::array<Svf, 2> wahSvf;
    SvfCoef wahCoef;
    float wahLogFc = 10.0f;
    float envFast = 0, envPeak = 1e-4f;
    float envAtk = 0, envRel = 0, peakRel = 0;
    float wahDepthSm = 0;

    // グラニュラー
    Ring ringL, ringR;
    std::array<Grain, kMaxGrains> grains;
    int64_t lastStep = INT64_MIN, lastHalfStep = INT64_MIN;
    int64_t lastPeriod = INT64_MIN;
    float grnMixSm = 0;

    // パン
    float panPos = 0, panOut = 0, panDepthSm = 0;
    SmallDelay haasL, haasR;

    float ampCoef = 0;  // 効き量の平滑化
};

} // namespace pon
