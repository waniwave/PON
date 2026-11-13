#pragma once

#include "BandFx.h"
#include "SyncDelay.h"
#include "Space.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace pon
{
// スポーク（真上から時計回り）: 1/2, 1/4, 1/8, 3連, 1/16, 5連, 6連, 7連
constexpr int kNumSpokes = 8;
constexpr double kSpokeRate[kNumSpokes] { 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0 };

// ANGLE(0..1) → R。SNAP オンは最寄りのスポーク、オフは隣のスポーク間を対数補間
// （7連 → 1/2 の間は補間せず、真ん中で切り替える）
double angleToRate (float angle, bool snap) noexcept;
int nearestSpoke (float angle) noexcept;

struct EngineInput
{
    float angle = 0.125f, radius = 0.0f, mix = 1.0f, space = 0.25f, chaos = 0.1f, punch = 0.6f;
    float outputDb = 0.0f;
    bool snap = true, anchor = true;

    // トランスポート
    bool   playing = false;
    bool   hasPpq = false;
    double ppq = 0.0;
    bool   hasBpm = false;
    double bpm = 120.0;
    bool   hasBarStart = false;
    double barStart = 0.0;
    int    timeSigNum = 4, timeSigDen = 4;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void process (juce::AudioBuffer<float>& buffer, int numInputChannels, const EngineInput& in) noexcept;

    // 音声スレッドから呼ぶ: 約 50ms のクロスフェードで切り替え（未準備なら即時）
    void setPreset (const PresetData& d, bool immediate) noexcept;

    // 音声スレッドから呼ぶ: 詳細パネルの編集。クロスフェードもリセットもせずに反映する
    void updatePresetLive (const PresetData& d) noexcept;

    int getLatencySamples() const noexcept { return latency; }

    // GUI 向け
    std::atomic<float> bandPan[3] { 0.0f, 0.0f, 0.0f };
    std::atomic<float> outPeak { 0.0f };
    std::atomic<float> gateMeter { 0.0f };

private:
    void applyPreset (const PresetData& d) noexcept;
    void beginBlocks() noexcept;
    void resetAll() noexcept;

    float sr = 48000.0f;
    int latency = 0;
    bool prepared = false;
    bool firstBlock = true;

    PresetData cur, next;
    int swapState = 0;          // 0: なし 1: フェードアウト中 2: フェードイン中
    float swapFade = 0.0f, swapInc = 0.0f;

    // 先読み
    SmallDelay inDelayL, inDelayR;

    // 3 バンド分割（LR4）。250 → (120, 3k)。各枝に反対側の分割点のオールパスを入れて位相をそろえる
    juce::dsp::LinkwitzRileyFilter<float> x250, x120, x3k, ap3k, ap120;

    // トランジェント検出
    float envFast = 0, envSlow = 0, gate = 0, duck = 0, gateHold = 0;
    float fAtk = 0, fRel = 0, sAtk = 0, sRel = 0, gateRel = 0, duckRel = 0;
    int holdSamples = 0;

    std::array<BandFx, 3> bands;
    std::array<SyncDelay, 3> delays;
    Space space;

    // ウェットの帯域制限（ラジオ越しなど）
    bool useWetHP = false, useWetLP = false;
    SvfCoef wetHPc, wetLPc;
    Svf wetHP[2], wetLP[2];
    int wetCoefCounter = 0;

    // 平滑化
    juce::SmoothedValue<float> radiusSm, mixSm, spaceSm, chaosSm, punchSm, gainSm;
    float engaged = 0.0f, engagedInc = 0.0f;
    float subMono = 0.0f, subMonoCoef = 0.0f;
    float anchorSm = 0.0f, anchorCoef = 0.0f;
    float sendsEnv = 0.0f, sendsEnvCoef = 0.0f;

    // 内部時計（停止中）
    double internalPpq = 0.0;

    // 無音検出
    int silentSamples = 0;

    // リミッター
    float limGain = 1.0f, limRel = 0.0f;

    BlockCtx blk;
};

} // namespace pon
