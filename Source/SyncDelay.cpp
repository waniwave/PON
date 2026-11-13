#include "SyncDelay.h"

namespace pon
{
void SyncDelay::prepare (float sampleRate, int maxSamples)
{
    sr = sampleRate;
    bufL.init (maxSamples);
    bufR.init (maxSamples);
    xfInc = 1.0f / (0.03f * sr);   // 30ms クロスフェード
    reset();
}

void SyncDelay::reset() noexcept
{
    bufL.resetValid();
    bufR.resetValid();
    fading = false;
    fresh = true;
    xf = 0.0f;
    for (int i = 0; i < 2; ++i)
    {
        fbSvf[i].reset();
        loopWahSvf[i].reset();
        hpSvf[i].reset();
        lpSvf[i].reset();
    }
}

void SyncDelay::setParams (const PresetData& d, int bandIndex) noexcept
{
    const auto& b = d.band[bandIndex];
    on = d.dly > 0.0f && b.on > 0.5f && b.mul > 0.0f && b.dlyAmt > 0.0f && b.dlyMul > 0.0f;
    delayBeats = 1.0 / juce::jmax (0.01, (double) b.dlyMul);
    fb = juce::jlimit (0.0f, 0.95f, d.dlyFb);
    ping = d.dlyPing > 0.5f;
    fbType = juce::jlimit (0, 3, (int) std::lround (d.dlyFbType));
    fbCoef.set (d.dlyFbFreq, d.dlyFbQ, sr);
    loopWah = clamp01 (d.dlyWah);
    rev = clamp01 (d.dlyRev);
    useHP = d.dlyOutHP > 25.0f;
    useLP = d.dlyOutLP < 18000.0f;
    hpCoef.set (d.dlyOutHP, 0.707f, sr);
    lpCoef.set (d.dlyOutLP, 0.707f, sr);
}

void SyncDelay::beginBlock (const BlockCtx& b) noexcept
{
    blk = b;
    if (! on) return;

    // 時間 = 1 / (R × 倍率) 拍。速すぎ/長すぎは 2 倍単位で折り返す（グリッドは保たれる）
    double rate = foldRate (b.R / delayBeats, b.bpm, 0.0);
    while (rate > 0.0 && 1.0 / rate > kMaxBeats) rate *= 2.0;
    const double beats = rate > 0.0 ? 1.0 / rate : 1.0;

    target = juce::jlimit (2.0, (double) bufL.size - 8.0, beats * b.spb);
    loopWahRate = foldRate (rate * 0.5, b.bpm, 10.0);

    revSegBeats = juce::jmin (beats, 2.0);
    // 逆再生ヘッドは 2×区間ぶん過去を読むのでバッファに収める
    while (revSegBeats * b.spb * 2.0 > (double) bufL.size - 8.0) revSegBeats *= 0.5;

    if (fresh)
    {
        tapA = tapB = target;
        fresh = false;
    }
    else if (! fading && std::abs (target - tapA) > 0.5)
    {
        tapB = target;
        xf = 0.0f;
        fading = true;
    }
}

void SyncDelay::process (const SampleCtx& c, float inL, float inR, float& outL, float& outR) noexcept
{
    if (! on)
    {
        outL = outR = 0.0f;
        return;
    }

    // 読み出し（クロスフェード切り替え）
    float yL = bufL.read (tapA - 1.0);
    float yR = bufR.read (tapA - 1.0);
    if (fading)
    {
        const float bL = bufL.read (tapB - 1.0);
        const float bR = bufR.read (tapB - 1.0);
        yL += xf * (bL - yL);
        yR += xf * (bR - yR);
        xf += xfInc;
        if (xf >= 1.0f)
        {
            tapA = tapB;
            fading = false;
            xf = 0.0f;
        }
    }

    // フィードバック経路: フィルター → ループ内ワウ → 量 → ソフトクリップ（暴走しても有限）
    float fL = yL, fR = yR;
    if (fbType > 0)
    {
        const int t = fbType == 1 ? 0 : (fbType == 2 ? 1 : 2);
        fL = fbSvf[0].processType (fL, fbCoef, t);
        fR = fbSvf[1].processType (fR, fbCoef, t);
    }
    if (loopWah > 0.0f)
    {
        const float phi = (float) fracd (c.ppq * loopWahRate);
        const float lfo = 0.5f - 0.5f * std::cos (kTwoPi * phi);
        loopWahCoef.set (450.0f * std::exp2 (lfo * 3.0f), 3.0f, sr);
        loopWahSvf[0].process (fL, loopWahCoef);
        loopWahSvf[1].process (fR, loopWahCoef);
        fL = lerpf (fL, loopWahSvf[0].bp * loopWahCoef.k * 1.4f, loopWah);
        fR = lerpf (fR, loopWahSvf[1].bp * loopWahCoef.k * 1.4f, loopWah);
    }
    fL = softClip (fL * fb);
    fR = softClip (fR * fb);

    if (ping)
    {
        bufL.push (0.5f * (inL + inR) + fR);
        bufR.push (fL);
    }
    else
    {
        bufL.push (inL + fL);
        bufR.push (inR + fR);
    }

    // ディレイ音だけを逆再生グレインに（2 つのヘッドをハン窓で重ねる、テンポ同期）
    if (rev > 0.0f)
    {
        const double seg = revSegBeats * blk.spb;
        const double ph  = fracd (c.ppq / revSegBeats);
        float rL = 0.0f, rR = 0.0f;
        for (int h = 0; h < 2; ++h)
        {
            const double hp = h == 0 ? ph : fracd (ph + 0.5);
            const float win = 0.5f - 0.5f * std::cos (kTwoPi * (float) hp);
            const double off = 2.0 * hp * seg;
            rL += bufL.read (off) * win;
            rR += bufR.read (off) * win;
        }
        yL = lerpf (yL, rL, rev);
        yR = lerpf (yR, rR, rev);
    }

    if (useHP)
    {
        yL = hpSvf[0].processType (yL, hpCoef, 1);
        yR = hpSvf[1].processType (yR, hpCoef, 1);
    }
    if (useLP)
    {
        yL = lpSvf[0].processType (yL, lpCoef, 0);
        yR = lpSvf[1].processType (yR, lpCoef, 0);
    }

    outL = yL;
    outR = yR;
}

} // namespace pon
