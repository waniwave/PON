#include "Engine.h"

namespace pon
{
int nearestSpoke (float angle) noexcept
{
    const double a = fracd ((double) angle);
    return (int) std::lround (a * kNumSpokes) % kNumSpokes;
}

double angleToRate (float angle, bool snap) noexcept
{
    if (snap)
        return kSpokeRate[nearestSpoke (angle)];

    const double pos = fracd ((double) angle) * kNumSpokes;
    const int k = juce::jlimit (0, kNumSpokes - 1, (int) std::floor (pos));
    const double t = pos - (double) k;
    if (k == kNumSpokes - 1)                      // 7連 → 1/2 は補間しない境界
        return t < 0.5 ? kSpokeRate[k] : kSpokeRate[0];
    return std::exp (std::log (kSpokeRate[k]) + (std::log (kSpokeRate[k + 1]) - std::log (kSpokeRate[k])) * t);
}

namespace
{
constexpr float kCeiling = 0.989f;   // -0.1 dBFS
constexpr double kMinBpmForBuffers = 40.0;
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = (float) sampleRate;
    latency = (int) std::lround (0.003 * sampleRate);

    inDelayL.init (latency + 16);
    inDelayR.init (latency + 16);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
    auto setup = [&spec] (juce::dsp::LinkwitzRileyFilter<float>& f, float fc, bool allpass)
    {
        f.prepare (spec);
        f.setType (allpass ? juce::dsp::LinkwitzRileyFilterType::allpass : juce::dsp::LinkwitzRileyFilterType::lowpass);
        f.setCutoffFrequency (fc);
        f.reset();
    };
    setup (x250, 250.0f, false);
    setup (x120, 120.0f, false);
    setup (x3k, 3000.0f, false);
    setup (ap3k, 3000.0f, true);
    setup (ap120, 120.0f, true);

    // グラニュラー: 2 小節（40BPM, 4/4 → 12 秒）+ 余裕。ディレイ: 4 拍（40BPM → 6 秒）+ 余裕
    const double beatSecMax = 60.0 / kMinBpmForBuffers;
    const int ringSamples  = (int) std::ceil ((8.0 * beatSecMax + 0.5) * sampleRate);
    const int delaySamples = (int) std::ceil ((SyncDelay::kMaxBeats * beatSecMax + 0.2) * sampleRate);
    for (auto& b : bands)  b.prepare (sr, ringSamples);
    for (auto& d : delays) d.prepare (sr, delaySamples);
    space.prepare (sr);

    fAtk = onePoleCoef (0.0003f, sr);
    fRel = onePoleCoef (0.015f, sr);
    sAtk = onePoleCoef (0.025f, sr);
    sRel = onePoleCoef (0.2f, sr);
    gateRel = onePoleCoef (0.018f, sr);      // アタック保護はアタック部分（約 30〜40ms）だけ
    holdSamples = (int) (0.006f * sr);
    subMonoCoef = onePoleCoef (0.02f, sr);
    anchorCoef = onePoleCoef (0.0015f, sr);
    sendsEnvCoef = onePoleCoef (0.3f, sr);
    limRel = onePoleCoef (0.08f, sr);
    swapInc = 1.0f / (0.025f * sr);
    engagedInc = 1.0f / (0.02f * sr);

    radiusSm.reset (sampleRate, 0.05);
    mixSm.reset (sampleRate, 0.03);
    spaceSm.reset (sampleRate, 0.05);
    chaosSm.reset (sampleRate, 0.05);
    punchSm.reset (sampleRate, 0.03);
    gainSm.reset (sampleRate, 0.03);

    blk.sr = sr;
    blk.bpm = 120.0;
    blk.spb = sr * 0.5;
    blk.R = 1.0;

    envFast = envSlow = gate = duck = 0.0f;
    gateHold = 0.0f;
    engaged = 0.0f;
    subMono = anchorSm = sendsEnv = 0.0f;
    limGain = 1.0f;
    silentSamples = 0;
    swapState = 0;
    swapFade = 0.0f;
    inDelayL.clear();
    inDelayR.clear();

    applyPreset (cur);
    prepared = true;
    firstBlock = true;
}

void Engine::setPreset (const PresetData& d, bool immediate) noexcept
{
    if (! prepared || immediate || engaged <= 0.0f)
    {
        swapState = 0;
        swapFade = 0.0f;
        applyPreset (d);
        return;
    }
    next = d;
    if (swapState != 1)
        swapState = 1;
}

void Engine::updatePresetLive (const PresetData& d) noexcept
{
    if (! prepared)
    {
        applyPreset (d);
        return;
    }
    if (swapState == 1)
    {
        next = d;      // フェードアウト中なら、切り替わり先を差し替えるだけ
        return;
    }
    cur = d;
    for (int b = 0; b < 3; ++b)
    {
        bands[(size_t) b].setParamsLive (cur, b);
        delays[(size_t) b].setParamsLive (cur, b);
    }
    const bool hp = cur.wetHP > 25.0f, lp = cur.wetLP < 18000.0f;
    if (hp && ! useWetHP) { wetHP[0].reset(); wetHP[1].reset(); }
    if (lp && ! useWetLP) { wetLP[0].reset(); wetLP[1].reset(); }
    useWetHP = hp;
    useWetLP = lp;
    wetCoefCounter = 0;
    beginBlocks();
}

void Engine::applyPreset (const PresetData& d) noexcept
{
    cur = d;
    for (int b = 0; b < 3; ++b)
    {
        bands[(size_t) b].setParams (cur, b);
        bands[(size_t) b].reset();
        delays[(size_t) b].setParams (cur, b);
        delays[(size_t) b].reset();
    }
    space.reset();
    for (int i = 0; i < 2; ++i) { wetHP[i].reset(); wetLP[i].reset(); }
    useWetHP = cur.wetHP > 25.0f;
    useWetLP = cur.wetLP < 18000.0f;
    wetCoefCounter = 0;
    sendsEnv = 0.0f;
    beginBlocks();
}

void Engine::beginBlocks() noexcept
{
    for (auto& b : bands)  b.beginBlock (blk);
    for (auto& d : delays) d.beginBlock (blk);
    space.setBlock (cur.spSize, cur.spDamp, (float) (cur.spPre * blk.spb), cur.spHaas);

    // ダッキングのリリースは拍に合わせる（1 ステップの 3 割、30〜400ms）
    const double tau = juce::jlimit (0.03, 0.4, 0.3 * (60.0 / blk.bpm) / juce::jmax (1.0, blk.R));
    duckRel = onePoleCoef ((float) tau, sr);
}

void Engine::resetAll() noexcept
{
    for (auto& b : bands)  b.reset();
    for (auto& d : delays) d.reset();
    space.reset();
    for (int i = 0; i < 2; ++i) { wetHP[i].reset(); wetLP[i].reset(); }
    x250.reset(); x120.reset(); x3k.reset(); ap3k.reset(); ap120.reset();
    sendsEnv = 0.0f;
}

void Engine::process (juce::AudioBuffer<float>& buffer, int numInputChannels, const EngineInput& in) noexcept
{
    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() < 1) return;

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (outR != nullptr && numInputChannels < 2)
        juce::FloatVectorOperations::copy (outR, outL, n);   // モノ → ステレオ

    // ---------------------------------------------------------------- テンポ
    const double bpm = (in.hasBpm && in.bpm > 1.0) ? juce::jlimit (20.0, 999.0, in.bpm) : 120.0;
    const double bps = bpm / 60.0 / (double) sr;
    double ppq0;
    if (in.playing && in.hasPpq)
    {
        ppq0 = in.ppq;
        internalPpq = in.ppq + (double) n * bps;
    }
    else
    {
        ppq0 = internalPpq;
        internalPpq += (double) n * bps;
    }
    ppq0 -= (double) latency * bps;   // 処理しているのは latency サンプル前の音

    const double barLen = juce::jmax (0.25, (double) juce::jmax (1, in.timeSigNum) * 4.0 / (double) juce::jmax (1, in.timeSigDen));
    const double barRef = (in.playing && in.hasBarStart) ? in.barStart : 0.0;
    const double secPerBeat = 60.0 / bpm;

    blk.sr = sr;
    blk.bpm = bpm;
    blk.spb = (double) sr * secPerBeat;
    blk.R = angleToRate (in.angle, in.snap);
    beginBlocks();

    radiusSm.setTargetValue (clamp01 (in.radius));
    mixSm.setTargetValue (clamp01 (in.mix));
    spaceSm.setTargetValue (clamp01 (in.space));
    chaosSm.setTargetValue (clamp01 (in.chaos));
    punchSm.setTargetValue (clamp01 (in.punch));
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, in.outputDb)));
    if (firstBlock)
    {
        radiusSm.setCurrentAndTargetValue (radiusSm.getTargetValue());
        mixSm.setCurrentAndTargetValue (mixSm.getTargetValue());
        spaceSm.setCurrentAndTargetValue (spaceSm.getTargetValue());
        chaosSm.setCurrentAndTargetValue (chaosSm.getTargetValue());
        punchSm.setCurrentAndTargetValue (punchSm.getTargetValue());
        gainSm.setCurrentAndTargetValue (gainSm.getTargetValue());
        firstBlock = false;
    }

    // ---------------------------------------------------------------- 無音検出（余韻が消えたら処理を止める）
    float inPeak = 0.0f;
    for (int c = 0; c < (outR != nullptr ? 2 : 1); ++c)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (c, 0, n));
    silentSamples = inPeak < 1e-6f ? juce::jmin (silentSamples + n, 1 << 30) : 0;
    // ディレイ（最長 4 拍）やグレイン（最長 2 小節前を読む）の中に音が残っている間は止めない
    const double holdSec = 8.0 * secPerBeat + 0.3;
    const bool idle = silentSamples > latency + (int) (holdSec * sr) && sendsEnv < 1e-5f && swapState == 0;

    float blockPeak = 0.0f;
    float gateMax = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float xL = outL[i];
        const float xR = outR != nullptr ? outR[i] : xL;

        // ------------------------------------------------ トランジェント検出（遅延前 = 先読み）
        const float mono = 0.5f * (std::abs (xL) + std::abs (xR));
        envFast = mono > envFast ? mono + fAtk * (envFast - mono) : mono + fRel * (envFast - mono);
        envSlow = mono > envSlow ? mono + sAtk * (envSlow - mono) : mono + sRel * (envSlow - mono);
        float gT = 0.0f;
        if (envFast > 0.003f)
        {
            const float ratioDb = 20.0f * std::log10 ((envFast + 1e-9f) / (envSlow + 1e-9f));
            gT = clamp01 ((ratioDb - 2.0f) / 6.0f);
        }
        if (gT > gate)            { gate = gT; gateHold = (float) holdSamples; }
        else if (gateHold > 0.0f) { gateHold -= 1.0f; }
        else                      { gate *= gateRel; }
        duck = gT > duck ? gT : duck * duckRel;
        gateMax = juce::jmax (gateMax, gate);

        // ------------------------------------------------ 先読み遅延（ドライも同じだけ遅らせる）
        inDelayL.push (xL);
        inDelayR.push (xR);
        const float dL = inDelayL.read ((float) latency);
        const float dR = inDelayR.read ((float) latency);

        const float rad   = radiusSm.getNextValue();
        const float mixv  = mixSm.getNextValue();
        const float spv   = spaceSm.getNextValue();
        const float chv   = chaosSm.getNextValue();
        const float pnv   = punchSm.getNextValue();
        const float gain  = gainSm.getNextValue();

        float yL = dL, yR = dR;

        if (! idle)
        {
            // ------------------------------------------------ 3 バンド分割（+サブ）
            float sub[2], lo[2], mid[2], hi[2], clean[2];
            const float dIn[2] = { dL, dR };
            for (int c = 0; c < 2; ++c)
            {
                float la, ha;
                x250.processSample (c, dIn[c], la, ha);
                const float laC = ap3k.processSample (c, la);
                const float haC = ap120.processSample (c, ha);
                x120.processSample (c, laC, sub[c], lo[c]);
                x3k.processSample (c, haC, mid[c], hi[c]);
                clean[c] = sub[c] + lo[c] + mid[c] + hi[c];   // 位相補正済みのドライ
            }

            // ------------------------------------------------ 効きの有無
            const bool engageTarget = (rad > 1e-4f || in.radius > 1e-4f || sendsEnv > 1e-4f || swapState != 0) && mixv > 1e-4f;
            engaged = engageTarget ? juce::jmin (1.0f, engaged + engagedInc) : juce::jmax (0.0f, engaged - engagedInc);

            // ------------------------------------------------ 頭拍アンカー
            // +1e-9 拍: グリッドの境界がちょうどサンプル位置に来るとき、丸め誤差でブロックの切れ目によって
            // 1 サンプル前後しないようにする（≒ 0.00003 サンプル）
            const double ppqAbs = ppq0 + (double) i * bps + 1e-9;
            double posInBar = ppqAbs - barRef;
            posInBar -= std::floor (posInBar / barLen) * barLen;
            const double tIn = posInBar * secPerBeat;
            const double tTo = (barLen - posInBar) * secPerBeat;
            // 拍頭の 6ms 前から 30ms 後まで（平滑化 1.5ms のぶん早めに寄せ始める）
            const float aT = (in.anchor && (tIn < 0.030 || tTo < 0.006)) ? 1.0f : 0.0f;
            anchorSm = aT + anchorCoef * (anchorSm - aT);

            if (engaged > 0.0f)
            {
                SampleCtx ctx;
                ctx.ppqAbs = ppqAbs;
                ctx.period = (int64_t) std::floor (ppqAbs / kPhasePeriodBeats);
                ctx.ppq = ppqAbs - (double) ctx.period * kPhasePeriodBeats;
                ctx.rlin = rad;
                ctx.cr = radiusCurve (rad, cur.curve);
                ctx.chaos = chv;
                ctx.gate = gate;
                ctx.anchor = anchorSm;

                // サブ: モノラルのまま、何も通さない
                const float smT = juce::jmin (1.0f, rad * 10.0f);
                subMono = smT + subMonoCoef * (subMono - smT);
                const float sm = 0.5f * (sub[0] + sub[1]);
                const float subL = sub[0] + subMono * (sm - sub[0]);
                const float subR = sub[1] + subMono * (sm - sub[1]);

                // 各バンド: ワウ → グラニュラー → パン
                float bL[3] = { lo[0], mid[0], hi[0] };
                float bR[3] = { lo[1], mid[1], hi[1] };
                float sumL = 0.0f, sumR = 0.0f;
                for (int b = 0; b < 3; ++b)
                {
                    bands[(size_t) b].process (ctx, bL[b], bR[b]);
                    sumL += bL[b];
                    sumR += bR[b];
                }

                // ウェットの帯域制限（RADIUS に応じて閉じていく）
                if (useWetHP || useWetLP)
                {
                    if (wetCoefCounter-- <= 0)
                    {
                        wetCoefCounter = 32;
                        wetHPc.set (20.0f * std::pow (juce::jmax (20.0f, cur.wetHP) / 20.0f, ctx.cr), 0.707f, sr);
                        wetLPc.set (20000.0f * std::pow (juce::jmin (20000.0f, cur.wetLP) / 20000.0f, ctx.cr), 0.707f, sr);
                    }
                    if (useWetHP) { sumL = wetHP[0].processType (sumL, wetHPc, 1); sumR = wetHP[1].processType (sumR, wetHPc, 1); }
                    if (useWetLP) { sumL = wetLP[0].processType (sumL, wetLPc, 0); sumR = wetLP[1].processType (sumR, wetLPc, 0); }
                }

                // ディレイ（バンドごとに分割）
                float dlyL = 0.0f, dlyR = 0.0f;
                for (int b = 0; b < 3; ++b)
                {
                    auto& d = delays[(size_t) b];
                    if (! d.isOn()) continue;
                    const float send = ctx.cr * cur.band[b].dlyAmt;
                    float oL, oR;
                    d.process (ctx, bL[b] * send, bR[b] * send, oL, oR);
                    dlyL += oL;
                    dlyR += oR;
                }
                dlyL *= cur.dly;
                dlyR *= cur.dly;

                // SPACE（アタックとサブは入れない）
                const float spAmt = spv * std::sqrt (ctx.cr);
                const float att = 1.0f - 0.8f * gate;
                float spL = 0.0f, spR = 0.0f;
                space.process (sumL * att + dlyL, sumR * att + dlyR, spAmt, spL, spR);

                float sendL = dlyL + spL;
                float sendR = dlyR + spR;
                sendsEnv = juce::jmax (juce::jmax (std::abs (sendL), std::abs (sendR)), sendsEnv * sendsEnvCoef);

                // ウェットのダッキング
                const float duckG = 1.0f - 0.9f * pnv * duck;
                sendL *= duckG;
                sendR *= duckG;

                // アタック保護: アタック中は原音を優先
                const float ap = std::sqrt (pnv) * gate;
                float insL = subL + sumL;
                float insR = subR + sumR;
                insL += ap * (clean[0] - insL);
                insR += ap * (clean[1] - insR);

                float wetL = insL + sendL;
                float wetR = insR + sendR;

                // プリセット切り替え中は clean を経由
                wetL += swapFade * (clean[0] - wetL);
                wetR += swapFade * (clean[1] - wetR);

                if (! std::isfinite (wetL) || ! std::isfinite (wetR))
                {
                    resetAll();
                    wetL = clean[0];
                    wetR = clean[1];
                }

                const float mixedL = clean[0] + mixv * (wetL - clean[0]);
                const float mixedR = clean[1] + mixv * (wetR - clean[1]);
                yL = dL + engaged * (mixedL - dL);
                yR = dR + engaged * (mixedR - dR);
            }
            else
            {
                subMono = 0.0f;
            }

            // プリセットのクロスフェード進行
            if (swapState == 1)
            {
                swapFade += swapInc;
                if (swapFade >= 1.0f)
                {
                    swapFade = 1.0f;
                    applyPreset (next);
                    swapState = 2;
                }
            }
            else if (swapState == 2)
            {
                swapFade -= swapInc;
                if (swapFade <= 0.0f)
                {
                    swapFade = 0.0f;
                    swapState = 0;
                }
            }
        }

        // ------------------------------------------------ OUTPUT → 安全リミッター
        yL *= gain;
        yR *= gain;
        const float pk = juce::jmax (std::abs (yL), std::abs (yR));
        const float target = pk > kCeiling ? kCeiling / pk : 1.0f;
        limGain = target < limGain ? target : target + limRel * (limGain - target);
        yL = juce::jlimit (-kCeiling, kCeiling, sanitize (yL * limGain));
        yR = juce::jlimit (-kCeiling, kCeiling, sanitize (yR * limGain));
        if (! std::isfinite (limGain)) limGain = 1.0f;

        outL[i] = yL;
        if (outR != nullptr) outR[i] = yR;
        blockPeak = juce::jmax (blockPeak, juce::jmax (std::abs (yL), std::abs (yR)));
    }

    x250.snapToZero(); x120.snapToZero(); x3k.snapToZero(); ap3k.snapToZero(); ap120.snapToZero();

    for (int b = 0; b < 3; ++b)
        bandPan[b].store ((idle || engaged <= 0.0f) ? 0.0f : bands[(size_t) b].getPanPosition(), std::memory_order_relaxed);
    outPeak.store (juce::jmax (blockPeak, outPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    gateMeter.store (gateMax, std::memory_order_relaxed);
}

} // namespace pon
