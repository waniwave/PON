#pragma once

#include "Dsp.h"
#include <array>

namespace pon
{
// SPACE: Haas の広がり（M/S 疑似ステレオ: モノに畳んでも消えない）＋ 小型 FDN リバーブ。
// FDN は HazePad の FdnReverb を元に、PON 用に軽く（拡散 2 段・ミックスなし）したもの。
class Space
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        const int lineSize = (int) std::ceil (sr * 0.2f) + 8;
        for (auto& l : lines) l.init (lineSize);
        preL.init ((int) std::ceil (sr * 0.55f) + 8);
        preR.init ((int) std::ceil (sr * 0.55f) + 8);
        haas.init ((int) std::ceil (sr * 0.035f) + 8);
        for (int i = 0; i < kDiff; ++i)
        {
            diffL[(size_t) i].init ((int) (kDiffMsL[i] * 0.001f * sr));
            diffR[(size_t) i].init ((int) (kDiffMsR[i] * 0.001f * sr));
        }
        for (int i = 0; i < kN; ++i)
        {
            lfoPhase[i] = (float) i / (float) kN;
            lfoInc[i] = kLfoHz[i] / sr;
        }
        modDepth = 0.0005f * sr;
        reset();
    }

    void reset() noexcept
    {
        for (auto& l : lines) l.clear();
        preL.clear(); preR.clear(); haas.clear();
        for (auto& d : diffL) d.clear();
        for (auto& d : diffR) d.clear();
        std::fill (std::begin (lp), std::end (lp), 0.0f);
    }

    // size 0..1, damp 0..1, predelay [samples], haasMs
    void setBlock (float size, float damp, float preSamples, float haasMs) noexcept
    {
        scale = 0.35f + 1.1f * clamp01 (size);
        const float decaySec = 0.5f + 2.6f * clamp01 (size);
        for (int i = 0; i < kN; ++i)
            fbGain[i] = std::pow (10.0f, -3.0f * (kLineMs[i] * 0.001f * scale) / decaySec);
        const float dampHz = juce::jmin (18000.0f * std::pow (0.06f, clamp01 (damp)), sr * 0.45f);
        dampC = std::exp (-kTwoPi * dampHz / sr);
        pre = juce::jlimit (1.0f, (float) preL.size - 4.0f, preSamples);
        haasD = juce::jlimit (1.0f, (float) haas.size - 4.0f, haasMs * 0.001f * sr);
        outGain = 0.55f / std::sqrt (1.0f + decaySec / 3.0f);
    }

    // in: 空間をかける信号, amount: SPACE 0..1 → 出力は「足す分」だけ
    inline void process (float inL, float inR, float amount, float& addL, float& addR) noexcept
    {
        // Haas: 遅らせたミッドをサイドに足す（L=+、R=−）
        const float m = 0.5f * (inL + inR);
        haas.push (m);
        const float side = haas.read (haasD) * 0.45f * amount;

        // リバーブ
        preL.push (inL * amount);
        preR.push (inR * amount);
        float xL = preL.read (pre);
        float xR = preR.read (pre);
        for (int d = 0; d < kDiff; ++d)
        {
            xL = diffL[(size_t) d].process (xL);
            xR = diffR[(size_t) d].process (xR);
        }

        float y[kN];
        for (int i = 0; i < kN; ++i)
        {
            lfoPhase[i] += lfoInc[i];
            if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
            const float dl = kLineMs[i] * 0.001f * sr * scale + modDepth * (1.0f + std::sin (kTwoPi * lfoPhase[i]));
            const float v = lines[(size_t) i].read (dl);
            lp[i] = v + dampC * (lp[i] - v);
            y[i] = lp[i];
        }
        const float wL = (y[0] - y[1] + y[2] - y[3] + y[4] - y[5] + y[6] - y[7]) * outGain;
        const float wR = (y[0] + y[1] - y[2] - y[3] + y[4] + y[5] - y[6] - y[7]) * outGain;
        hadamard (y);
        for (int i = 0; i < kN; ++i)
            lines[(size_t) i].push (y[i] * fbGain[i] + ((i & 1) ? xR : xL) * 0.5f);

        addL = side + wL * amount;
        addR = -side + wR * amount;
    }

private:
    static constexpr int kN = 8;
    static constexpr int kDiff = 2;
    static constexpr float kLineMs[kN] { 29.3f, 34.9f, 41.7f, 47.3f, 53.1f, 61.9f, 70.3f, 79.9f };
    static constexpr float kLfoHz[kN]  { 0.11f, 0.17f, 0.23f, 0.29f, 0.37f, 0.43f, 0.53f, 0.61f };
    static constexpr float kDiffMsL[kDiff] { 4.77f, 11.73f };
    static constexpr float kDiffMsR[kDiff] { 4.93f, 12.37f };

    static void hadamard (float* x) noexcept
    {
        for (int h = 1; h < kN; h *= 2)
            for (int i = 0; i < kN; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }
        for (int i = 0; i < kN; ++i) x[i] *= 0.35355339f;
    }

    struct Allpass
    {
        std::vector<float> buf;
        int idx = 0;
        void init (int len) { buf.assign ((size_t) juce::jmax (1, len), 0.0f); idx = 0; }
        void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); idx = 0; }
        inline float process (float x) noexcept
        {
            const float d = buf[(size_t) idx];
            const float y = d - 0.6f * x;
            buf[(size_t) idx] = x + 0.6f * y;
            if (++idx >= (int) buf.size()) idx = 0;
            return y;
        }
    };

    float sr = 48000.0f;
    std::array<SmallDelay, kN> lines;
    SmallDelay preL, preR, haas;
    std::array<Allpass, kDiff> diffL, diffR;
    float lfoPhase[kN] {}, lfoInc[kN] {}, lp[kN] {}, fbGain[kN] {};
    float modDepth = 0.0f, scale = 1.0f, dampC = 0.0f, pre = 1.0f, haasD = 1.0f, outGain = 0.5f;
};

} // namespace pon
