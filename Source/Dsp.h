#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace pon
{
constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

// LFO 位相を畳む周期（拍）。PON で使う有理数レートは 48 拍（4/4 で 12 小節）で必ず一周する。
constexpr double kPhasePeriodBeats = 48.0;

inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float lerpf (float a, float b, float t) noexcept { return a + (b - a) * t; }

inline double fracd (double x) noexcept { return x - std::floor (x); }

// one-pole 係数（時定数 sec）
inline float onePoleCoef (float seconds, float sampleRate) noexcept
{
    if (seconds <= 0.0f) return 0.0f;
    return std::exp (-1.0f / (seconds * sampleRate));
}

// ---------------------------------------------------------------------------------------------
// 決定論的乱数（CHAOS 用）: 周期番号・ステップ番号・用途 salt から [0,1) を返す
inline uint32_t hash32 (uint32_t x) noexcept
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

inline float rand01 (int64_t period, int64_t step, uint32_t salt) noexcept
{
    uint32_t h = hash32 ((uint32_t) period * 0x9e3779b9U + 0x632be5abU);
    h = hash32 (h ^ ((uint32_t) step * 0x85ebca6bU));
    h = hash32 (h ^ ((uint32_t) (step >> 32) * 0xc2b2ae35U));
    h = hash32 (h ^ (salt * 0x27d4eb2fU));
    return (float) (h >> 8) * (1.0f / 16777216.0f);
}

// ---------------------------------------------------------------------------------------------
// RADIUS → 効き。0〜0.3 はゆるやか、そこから急に効く（k が大きいほど急）
inline float radiusCurve (float r, float k) noexcept
{
    r = clamp01 (r);
    k = juce::jmax (0.5f, k);
    const float e = (std::exp (k * r) - 1.0f) / (std::exp (k) - 1.0f);
    return 0.25f * r + 0.75f * e;
}

// レートの折り返し: 1 拍あたり 32 回超、または maxHz 超なら半分にしていく
inline double foldRate (double perBeat, double bpm, double maxHz) noexcept
{
    if (perBeat <= 0.0) return 0.0;
    while (perBeat > 32.0) perBeat *= 0.5;
    if (maxHz > 0.0)
        while (perBeat * bpm / 60.0 > maxHz) perBeat *= 0.5;
    return perBeat;
}

// ---------------------------------------------------------------------------------------------
// TPT（Zavalishin）ステートバリアブルフィルター。高 Q でも発散しない。
struct SvfCoef
{
    float a1 = 0, a2 = 0, a3 = 0, k = 1;

    void set (float fc, float q, float sr) noexcept
    {
        fc = juce::jlimit (10.0f, sr * 0.45f, fc);
        const float g = std::tan (kPi * fc / sr);
        k  = 1.0f / juce::jmax (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct Svf
{
    float ic1 = 0, ic2 = 0;
    float lp = 0, bp = 0, hp = 0;

    void reset() noexcept { ic1 = ic2 = lp = bp = hp = 0.0f; }

    inline void process (float v0, const SvfCoef& c) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - c.k * v1 - v2;
    }

    // 0:LP 1:HP 2:BP（ピーク 0dB に正規化）
    inline float processType (float v0, const SvfCoef& c, int type) noexcept
    {
        process (v0, c);
        switch (type)
        {
            case 1:  return hp;
            case 2:  return bp * c.k;
            default: return lp;
        }
    }
};

// ---------------------------------------------------------------------------------------------
// リングバッファ。read(d) は「d サンプル前」（0 = 今書いたサンプル）を線形補間で返す。
// リセット後に書かれていない領域は 0 を返す（巨大バッファを音声スレッドでクリアしないため）。
struct Ring
{
    std::vector<float> buf;
    int size = 0, w = 0;
    int written = 0;

    void init (int n)
    {
        size = juce::jmax (4, n);
        buf.assign ((size_t) size, 0.0f);
        w = 0; written = 0;
    }

    void resetValid() noexcept { written = 0; }

    inline void push (float x) noexcept
    {
        buf[(size_t) w] = x;
        if (++w >= size) w = 0;
        if (written < size) ++written;
    }

    inline float at (int d) const noexcept
    {
        if (d < 0 || d >= written) return 0.0f;
        int i = w - 1 - d;
        if (i < 0) i += size;
        return buf[(size_t) i];
    }

    inline float read (double d) const noexcept
    {
        if (d < 0.0) d = 0.0;
        const int   di = (int) d;
        const float f  = (float) (d - (double) di);
        const float a  = at (di);
        const float b  = at (di + 1);
        return a + f * (b - a);
    }

    int available() const noexcept { return written; }
};

// 小さなリング（Haas 用など）。常に全体が有効。
struct SmallDelay
{
    std::vector<float> buf;
    int size = 0, w = 0;

    void init (int n) { size = juce::jmax (4, n); buf.assign ((size_t) size, 0.0f); w = 0; }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); }

    inline void push (float x) noexcept { buf[(size_t) w] = x; if (++w >= size) w = 0; }

    inline float read (float d) const noexcept
    {
        d = juce::jlimit (0.0f, (float) (size - 2), d);
        const int di = (int) d;
        const float f = d - (float) di;
        int i0 = w - 1 - di; if (i0 < 0) i0 += size;
        int i1 = i0 - 1;     if (i1 < 0) i1 += size;
        return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
};

// 柔らかいクリップ（|x|→∞ で ±1 に近づく）
inline float softClip (float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float sanitize (float x) noexcept
{
    return std::isfinite (x) ? x : 0.0f;
}

} // namespace pon
