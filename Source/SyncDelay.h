#pragma once

#include "BandFx.h"

namespace pon
{
// テンポ同期ステレオディレイ（1 バンド分）。
// 時間 = 1 ÷ (R × 倍率) 拍。時間が変わったら 2 つの読み出し位置をクロスフェードして切り替える（ピッチが跳ねない）。
class SyncDelay
{
public:
    static constexpr double kMaxBeats = 4.0;   // これより長い時間は半分に折り返す

    void prepare (float sampleRate, int maxSamples);
    void reset() noexcept;
    void setParams (const PresetData& d, int bandIndex) noexcept;
    void setParamsLive (const PresetData& d, int bandIndex) noexcept
    {
        const bool wasOn = on;
        setParams (d, bandIndex);
        if (on && ! wasOn) reset();   // 止まっていた間の古い中身を鳴らさない
    }
    void beginBlock (const BlockCtx& b) noexcept;
    void process (const SampleCtx& c, float inL, float inR, float& outL, float& outR) noexcept;

    bool isOn() const noexcept { return on; }

private:
    Ring bufL, bufR;
    float sr = 48000.0f;
    BlockCtx blk;

    bool on = false;
    double delayBeats = 1.0;
    double target = 0.0, tapA = 0.0, tapB = 0.0;
    float xf = 0.0f, xfInc = 0.0f;
    bool fading = false, fresh = true;

    float fb = 0.0f;
    bool ping = false;
    int fbType = 0;
    SvfCoef fbCoef;
    Svf fbSvf[2];

    float loopWah = 0.0f;
    double loopWahRate = 1.0;
    SvfCoef loopWahCoef;
    Svf loopWahSvf[2];

    float rev = 0.0f;
    double revSegBeats = 1.0;

    bool useHP = false, useLP = false;
    SvfCoef hpCoef, lpCoef;
    Svf hpSvf[2], lpSvf[2];
};

} // namespace pon
