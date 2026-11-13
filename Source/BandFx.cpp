#include "BandFx.h"

namespace pon
{
namespace
{
struct HannTable
{
    float v[1026];
    HannTable()
    {
        for (int i = 0; i <= 1025; ++i)
            v[i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) juce::jmin (i, 1024) / 1024.0f);
    }
};

const HannTable& hannTable()
{
    static const HannTable t;
    return t;
}

inline float hannAt (const HannTable& h, float x) noexcept
{
    const float f = x * 1024.0f;
    const int i = (int) f;
    if (i < 0 || i >= 1024) return 0.0f;
    const float fr = f - (float) i;
    return h.v[i] + fr * (h.v[i + 1] - h.v[i]);
}

constexpr float kSqrt2 = 1.41421356f;
}

void BandFx::prepare (float sampleRate, int ringSamples)
{
    sr = sampleRate;
    (void) hannTable();
    ringL.init (ringSamples);
    ringR.init (ringSamples);
    haasL.init ((int) (0.035f * sr) + 8);
    haasR.init ((int) (0.035f * sr) + 8);
    envAtk  = onePoleCoef (0.003f, sr);
    envRel  = onePoleCoef (0.12f, sr);
    peakRel = onePoleCoef (1.5f, sr);
    ampCoef = onePoleCoef (0.02f, sr);
    reset();
}

void BandFx::reset() noexcept
{
    for (auto& s : wahSvf) s.reset();
    wahLogFc = std::log2 (std::sqrt (juce::jmax (20.0f, p.wahLo) * juce::jmax (20.0f, p.wahHi)));
    envFast = 0.0f;
    envPeak = 1e-4f;
    ringL.resetValid();
    ringR.resetValid();
    for (auto& g : grains) g.alive = false;
    lastStep = lastHalfStep = lastPeriod = INT64_MIN;
    panPos = panOut = 0.0f;
    haasL.clear();
    haasR.clear();
    wahDepthSm = grnMixSm = panDepthSm = 0.0f;
}

void BandFx::setParams (const PresetData& d, int bandIndex) noexcept
{
    bandIdx = bandIndex;
    p = d.band[bandIndex];
    on = p.on > 0.5f && p.mul > 0.0f;
    wahMax = d.wah * p.wahAmt;
    panMax = d.pan * p.panAmt;
    grnMax = d.grn * p.grnAmt;
    rotate = d.rotate;
    rotRate = d.rotRate;
    grnDrift = d.grnDrift;
    grnDriftBars = juce::jmax (1.0f, d.grnDriftBars);
    grnWob = d.grnWob;
    grnWobRate = d.grnWobRate;
}

void BandFx::setParamsLive (const PresetData& d, int bandIndex) noexcept
{
    const bool wasOn = on;
    const bool hadGrn = grnMax > 0.0f;
    const bool hadHaas = p.panHaas > 0.0f;
    setParams (d, bandIndex);
    if (on && ! wasOn)
    {
        reset();
        return;
    }
    if (grnMax > 0.0f && ! hadGrn)
    {
        ringL.resetValid();
        ringR.resetValid();
        for (auto& g : grains) g.alive = false;
        lastStep = lastHalfStep = lastPeriod = INT64_MIN;
    }
    if (p.panHaas > 0.0f && ! hadHaas)
    {
        haasL.clear();
        haasR.clear();
    }
}

void BandFx::beginBlock (const BlockCtx& b) noexcept
{
    blk = b;
    const double base = b.R * (double) p.mul;
    wahRate = foldRate (base * (double) p.wahMul, b.bpm, 25.0);
    panRate = foldRate (base, b.bpm, 25.0);
    grnRate = foldRate (base * (double) juce::jmax (0.125f, p.grnSub), b.bpm, 0.0);

    // パンの切り替えのなめらかさ: 1 区間の長さに応じて 2〜10ms
    const double beatSec = 60.0 / b.bpm;
    const double segSec  = panRate > 0.0 ? (0.4 / panRate) * beatSec : 1.0;
    panSmoothCoef = onePoleCoef ((float) juce::jlimit (0.002, 0.010, segSec * 0.08), sr);

    wahSmoothCoef = onePoleCoef (p.wahShape > 1.5f ? 0.012f : 0.0015f, sr);

    // グレインの重なりに応じた音量補正
    const double grainsPerSec = grnRate * b.bpm / 60.0 * (double) juce::jmax (1.0f, p.grnVoices);
    const double overlap = grainsPerSec * (double) p.grnSize * 0.001 * 0.5;
    grnNorm = 1.25f / std::sqrt ((float) juce::jmax (1.0, overlap));
}

void BandFx::process (const SampleCtx& c, float& L, float& R) noexcept
{
    if (! on)
    {
        panOut = 0.0f;
        return;
    }

    const float wahT = wahMax * c.cr;
    const float grnT = grnMax * c.cr;
    const float panT = panMax * c.cr;
    wahDepthSm = wahT + ampCoef * (wahDepthSm - wahT);
    grnMixSm   = grnT + ampCoef * (grnMixSm - grnT);
    panDepthSm = panT + ampCoef * (panDepthSm - panT);

    const uint32_t bandSalt = (uint32_t) bandIdx * 1000u;

    // ---------------------------------------------------------------- ワウ
    if (wahMax > 0.0f)
    {
        const double x = c.ppq * wahRate + (double) p.wahPhase;
        const double phi = fracd (x);
        const int64_t step = (int64_t) std::floor (x);
        float lfo;
        if (p.wahShape > 1.5f)
            lfo = rand01 (c.period, step, bandSalt + 11u);
        else if (p.wahShape > 0.5f)
            lfo = 1.0f - std::abs (2.0f * (float) phi - 1.0f);
        else
            lfo = 0.5f - 0.5f * std::cos (kTwoPi * (float) phi);

        if (c.chaos > 0.0f)
            lfo = lerpf (lfo, rand01 (c.period, step, bandSalt + 101u), c.chaos * 0.6f);

        float pos = lfo;
        if (p.wahInv > 0.5f)
        {
            // 逆ワウ: 入力が大きいほど閉じる
            const float in = 0.5f * (std::abs (L) + std::abs (R));
            envFast = in > envFast ? in + envAtk * (envFast - in) : in + envRel * (envFast - in);
            envPeak = juce::jmax (envFast, juce::jmax (1e-5f, envPeak * peakRel));
            const float e = std::pow (clamp01 (envFast / envPeak), 0.7f);
            pos = 0.8f * (1.0f - e) + 0.2f * lfo;
        }

        const float lo = std::log2 (juce::jmax (20.0f, p.wahLo));
        const float hi = std::log2 (juce::jmax (20.0f, p.wahHi));
        const float target = lo + (hi - lo) * pos;
        wahLogFc = target + wahSmoothCoef * (wahLogFc - target);
        wahCoef.set (std::exp2 (wahLogFc), p.wahQ, sr);

        const float q = juce::jmax (1.0f, p.wahQ);
        const float lpGain = 1.0f / std::sqrt (q);
        const float bpGain = 1.0f + 0.12f * q;
        const float lpMix = p.wahLp;

        float* ch[2] = { &L, &R };
        for (int i = 0; i < 2; ++i)
        {
            auto& f = wahSvf[(size_t) i];
            const float in = *ch[i];
            f.process (in, wahCoef);
            const float wet = lpMix * f.lp * lpGain + (1.0f - lpMix) * f.bp * wahCoef.k * bpGain;
            *ch[i] = in + wahDepthSm * (wet - in);
        }
    }

    // ---------------------------------------------------------------- グラニュラー
    if (grnMax > 0.0f)
    {
        ringL.push (L);
        ringR.push (R);

        const double pos = c.ppq * grnRate;
        const int64_t step = (int64_t) std::floor (pos);
        const int64_t half = (int64_t) std::floor (pos * 2.0);

        if (step != lastStep || c.period != lastPeriod)
        {
            const bool first = (lastStep == INT64_MIN);
            lastStep = step;
            lastPeriod = c.period;
            if (! first || fracd (pos) < grnRate / blk.spb * 1.5)
                spawnGrains (c, step, 0.0f);
        }
        else if (half != lastHalfStep && (half & 1) != 0 && c.chaos > 0.0f)
        {
            // CHAOS: 裏の位置にグレインを追加（グリッド上）
            if (rand01 (c.period, half, bandSalt + 71u) < c.chaos * 0.35f)
                spawnGrains (c, half + 1000000, 1.0f);
        }
        lastHalfStep = half;

        const auto& ht = hannTable();
        float gL = 0.0f, gR = 0.0f;
        for (auto& g : grains)
        {
            if (! g.alive) continue;
            const float env = hannAt (ht, (float) g.t / (float) g.len);
            gL += ringL.read (g.off) * env * g.gl;
            gR += ringR.read (g.off) * env * g.gr;
            g.off += g.inc;
            if (++g.t >= g.len) g.alive = false;
        }

        const float m = grnMixSm;
        L = L * (1.0f - 0.85f * m) + gL * grnNorm * m;
        R = R * (1.0f - 0.85f * m) + gR * grnNorm * m;
    }

    // ---------------------------------------------------------------- パン
    if (panMax > 0.0f)
    {
        const double rot = rotate > 0.5f ? fracd (c.ppq * (double) rotRate) + (double) bandIdx / 3.0 : 0.0;
        const double x = c.ppq * panRate + (double) p.panPhase + rot;
        const float phi = (float) fracd (x);
        const int64_t cyc = (int64_t) std::floor (x);

        const float fl = p.panL / juce::jmax (0.01f, p.panL + p.panR);
        const float fracL = juce::jlimit (0.1f, 0.9f, 0.5f + (fl - 0.5f) * c.rlin);
        const bool inL = phi < fracL;

        const float w = inL ? 0.5f * phi / fracL : 0.5f + 0.5f * (phi - fracL) / (1.0f - fracL);
        const float sq = inL ? -1.0f : 1.0f;
        const float sn = -std::sin (kTwoPi * w);
        float target = lerpf (sq, sn, p.panSoft);

        // CHAOS: 区間ごとに行き先を入れ替える
        if (c.chaos > 0.0f && rand01 (c.period, cyc * 2 + (inL ? 0 : 1), bandSalt + 57u) < c.chaos * 0.5f)
            target = -target;

        panPos = target + panSmoothCoef * (panPos - target);
        // 頭拍アンカーはパンの平滑化の後に掛ける（拍頭の瞬間に確実に中央へ）
        const float pp = juce::jlimit (-1.0f, 1.0f, panPos * panDepthSm * (1.0f - c.anchor));

        const float M = 0.5f * (L + R);
        const float S = 0.5f * (L - R);
        const float a = (pp + 1.0f) * (kPi * 0.25f);
        const float gainL = std::cos (a) * kSqrt2;
        const float gainR = std::sin (a) * kSqrt2;
        const float width = 1.0f - std::abs (pp);
        float outL = M * gainL + S * width;
        float outR = M * gainR - S * width;

        if (p.panHaas > 0.0f)
        {
            haasL.push (outL);
            haasR.push (outR);
            const float d = p.panHaas * 0.001f * sr;
            if (pp < 0.0f) outR = lerpf (outR, haasR.read (d), -pp);
            else           outL = lerpf (outL, haasL.read (d), pp);
        }

        L = outL;
        R = outR;
        panOut = pp;
    }
    else
    {
        panOut = 0.0f;
    }
}

void BandFx::spawnGrains (const SampleCtx& c, int64_t step, float trigSalt) noexcept
{
    if (p.grnTail > 0.5f && c.gate > 0.25f)
        return; // 余韻のみモード: アタック中は出さない

    const uint32_t salt = (uint32_t) bandIdx * 1000u + 300u + (uint32_t) (trigSalt * 50.0f);

    if (trigSalt == 0.0f && rand01 (c.period, step, salt) < c.chaos * 0.35f)
        return; // CHAOS: 間引き

    const int voices = trigSalt == 0.0f ? juce::jlimit (1, 4, (int) std::lround (p.grnVoices)) : 1;
    for (int v = 0; v < voices; ++v)
    {
        const uint32_t s = salt + 7u + (uint32_t) v * 17u;
        startGrain (c,
                    rand01 (c.period, step, s + 1u), rand01 (c.period, step, s + 2u),
                    rand01 (c.period, step, s + 3u), rand01 (c.period, step, s + 4u),
                    rand01 (c.period, step, s + 5u));
    }
}

void BandFx::startGrain (const SampleCtx& c, float r1, float r2, float r3, float r4, float r5) noexcept
{
    Grain* slot = nullptr;
    for (auto& g : grains)
        if (! g.alive) { slot = &g; break; }
    if (slot == nullptr) return;

    const float sizeSec = p.grnSize * 0.001f * (1.0f + (r1 - 0.5f) * 0.66f);
    const int len = juce::jmax (16, (int) (sizeSec * sr));

    float st = p.grnPitch;
    if (p.grnOct > 0.5f)
    {
        const int k = juce::jlimit (0, 2, (int) (r2 * 3.0f)) - 1;     // -1, 0, +1
        st += (float) k * juce::jmin (12.0f, p.grnSpread);
    }
    else
    {
        st += (2.0f * r2 - 1.0f) * p.grnSpread;
    }
    st += (2.0f * r1 - 1.0f) * 0.3f * c.chaos; // CHAOS: わずかな揺らぎ

    if (grnDrift != 0.0f)
        st -= grnDrift * (float) fracd (c.ppqAbs / (4.0 * (double) grnDriftBars));
    if (grnWob != 0.0f)
        st += grnWob * std::sin (kTwoPi * (float) fracd (c.ppq * (double) grnWobRate));

    st = juce::jlimit (-24.0f, 24.0f, st);
    const double ratio = std::exp2 ((double) st / 12.0);
    const bool rev = r3 < juce::jmin (1.0f, p.grnRev + 0.1f * c.chaos * (p.grnRev > 0.0f ? 1.0f : 0.0f));
    const double inc = rev ? 1.0 + ratio : 1.0 - ratio;

    double off = ((double) p.grnOffset + (double) p.grnJitter * (double) r4 * (0.5 + 0.5 * (double) c.chaos)) * blk.spb;
    const double avail = (double) ringL.available() - 4.0;
    const double minOff = 2.0 - juce::jmin (0.0, inc) * (double) len;
    const double maxOff = avail - juce::jmax (0.0, inc) * (double) len;
    if (maxOff < minOff) return;
    off = juce::jlimit (minOff, maxOff, off);

    const float pan = (2.0f * r5 - 1.0f) * p.grnWidth;
    const float a = (pan + 1.0f) * (kPi * 0.25f);

    slot->alive = true;
    slot->off = off;
    slot->inc = inc;
    slot->t = 0;
    slot->len = len;
    slot->gl = std::cos (a) * kSqrt2;
    slot->gr = std::sin (a) * kSqrt2;
}

} // namespace pon
