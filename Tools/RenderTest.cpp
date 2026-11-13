// PON オフライン検証ツール
//  - ヌルテスト（RADIUS 0 / MIX 0 で入力と同一か）
//  - 全プリセットを RADIUS 最大でドラムループに掛け、ピーク/RMS/ビートの聞き取りやすさを測定し WAV を書き出す
//  - ブロックサイズ 1〜4096 で結果が同じか、44.1/96/192kHz でタイミングと音量が同じか
//  - 全開ストレス（NaN/Inf、0dBFS 超え）、状態の保存→復元、プリセット切替のクリック、無音時の CPU
#include <cstdio>
#include <chrono>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "FactoryPresets.h"

using namespace pon;

namespace
{
constexpr double kBpm = 120.0;

struct TestPlayHead : juce::AudioPlayHead
{
    double ppq = 0.0;
    double bpm = kBpm;
    bool playing = true;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        p.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        return p;
    }
};

void setParam (PonProcessor& p, const juce::String& id, float plain)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

// ---- テスト用ドラムループ（キック / スネア / ハット / 和音）
struct Hit { double beat; int type; };

juce::AudioBuffer<float> makeLoop (double sr, int bars, std::vector<double>* hitTimes = nullptr)
{
    const int total = (int) (bars * 4 * 60.0 / kBpm * sr);
    juce::AudioBuffer<float> b (2, total);
    b.clear();
    juce::Random rng (1234);
    const double spb = 60.0 / kBpm * sr;

    std::vector<Hit> hits;
    for (int bar = 0; bar < bars; ++bar)
    {
        const double o = bar * 4.0;
        hits.push_back ({ o + 0.0, 0 });
        hits.push_back ({ o + 1.5, 0 });
        hits.push_back ({ o + 2.5, 0 });
        hits.push_back ({ o + 1.0, 1 });
        hits.push_back ({ o + 3.0, 1 });
        for (int e = 0; e < 8; ++e) hits.push_back ({ o + e * 0.5, 2 });
    }

    for (const auto& h : hits)
    {
        const int start = (int) (h.beat * spb);
        if (hitTimes != nullptr && h.type <= 1) hitTimes->push_back (h.beat);
        double ph = 0.0;
        float hp = 0.0f, prev = 0.0f;
        for (int i = 0; start + i < total && i < (int) (0.6 * sr); ++i)
        {
            const double t = i / sr;
            float l = 0.0f, r = 0.0f;
            if (h.type == 0)
            {
                const double f = 45.0 + 90.0 * std::exp (-t * 30.0);
                ph += f / sr;
                const float v = (float) (std::sin (6.2831853 * ph) * std::exp (-t * 7.0)) * 0.8f;
                l = r = v;
            }
            else if (h.type == 1)
            {
                ph += 185.0 / sr;
                const float n = rng.nextFloat() * 2.0f - 1.0f;
                const float v = (float) ((0.45 * n + 0.35 * std::sin (6.2831853 * ph)) * std::exp (-t * 18.0));
                l = v; r = v * 0.95f;
            }
            else
            {
                if (t > 0.08) break;
                const float n = rng.nextFloat() * 2.0f - 1.0f;
                hp = 0.6f * (hp + n - prev); prev = n;
                const float v = hp * (float) std::exp (-t * 60.0) * 0.25f;
                l = v * 0.8f; r = v;
            }
            b.addSample (0, start + i, l);
            b.addSample (1, start + i, r);
        }
    }

    // 中域の和音（ワウやパンが分かるように）
    for (int i = 0; i < total; ++i)
    {
        const double t = i / sr;
        const float v = 0.06f * (float) (std::sin (6.2831853 * 220.0 * t) + 0.7 * std::sin (6.2831853 * 330.0 * t)
                                         + 0.5 * std::sin (6.2831853 * 554.0 * t) + 0.3 * std::sin (6.2831853 * 1320.0 * t));
        b.addSample (0, i, v);
        b.addSample (1, i, v);
    }
    b.applyGain (0.7f);   // 最大ピークを 0dBFS より十分下に（ヌルテストでリミッターが働かないように）
    return b;
}

juce::AudioBuffer<float> render (PonProcessor& p, const juce::AudioBuffer<float>& in, int block, TestPlayHead& ph, double sr,
                                 int extraSamples = 0)
{
    const int total = in.getNumSamples() + extraSamples;
    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c)
        {
            if (pos < in.getNumSamples())
            {
                const int m = juce::jmin (n, in.getNumSamples() - pos);
                buf.copyFrom (c, 0, in, c, pos, m);
                if (m < n) buf.clear (c, m, n - m);
            }
            else buf.clear (c, 0, n);
        }
        ph.ppq = pos / sr * ph.bpm / 60.0;
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
    }
    return out;
}

void prepare (PonProcessor& p, double sr, int block, TestPlayHead& ph)
{
    p.setPlayHead (&ph);
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
}

void selectPreset (PonProcessor& p, int idx)
{
    setParam (p, ids::preset, (float) idx);
    p.syncPresetFromParam();
}

float maxAbs (const juce::AudioBuffer<float>& b, bool& finite)
{
    float m = 0.0f;
    finite = true;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float x = b.getSample (c, i);
            if (! std::isfinite (x)) finite = false;
            m = juce::jmax (m, std::abs (x));
        }
    return m;
}

float rmsDb (const juce::AudioBuffer<float>& b, int from, int to)
{
    double s = 0.0;
    int n = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = juce::jmax (0, from); i < juce::jmin (to, b.getNumSamples()); ++i) { s += b.getSample (c, i) * b.getSample (c, i); ++n; }
    return n > 0 ? (float) (10.0 * std::log10 (s / n + 1e-20)) : -200.0f;
}

// ビートの聞き取りやすさ: キック/スネアの直後 25ms のピークと、直前 80〜10ms の RMS の比（dB）の平均
float punchScore (const juce::AudioBuffer<float>& b, const std::vector<double>& hits, double sr, int latency)
{
    double sum = 0.0;
    int n = 0;
    const double spb = 60.0 / kBpm * sr;
    for (double beat : hits)
    {
        const int t = (int) (beat * spb) + latency;
        if (t < (int) (0.1 * sr) || t + (int) (0.03 * sr) >= b.getNumSamples()) continue;
        float pk = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = t; i < t + (int) (0.025 * sr); ++i) pk = juce::jmax (pk, std::abs (b.getSample (c, i)));
        const float pre = rmsDb (b, t - (int) (0.08 * sr), t - (int) (0.01 * sr));
        sum += juce::Decibels::gainToDecibels (pk, -120.0f) - pre;
        ++n;
    }
    return n > 0 ? (float) (sum / n) : 0.0f;
}

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    if (os == nullptr) return;
    std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) return;
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

float maxDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int offsetB, int from, int to)
{
    float m = 0.0f;
    for (int c = 0; c < 2; ++c)
        for (int i = from; i < to; ++i)
        {
            const int j = i + offsetB;
            if (j < 0 || j >= b.getNumSamples() || i >= a.getNumSamples()) continue;
            m = juce::jmax (m, std::abs (a.getSample (c, i) - b.getSample (c, j)));
        }
    return m;
}

int failures = 0;
void check (bool ok, const char* what, const juce::String& detail = {})
{
    std::printf ("[%s] %s %s\n", ok ? " OK " : "FAIL", what, detail.toRawUTF8());
    if (! ok) ++failures;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("renders");
    outDir.createDirectory();

    const auto& presets = getFactoryPresets();
    check (presets.size() == 45, "45 factory presets", juce::String ((int) presets.size()));
    check (presets[(size_t) getDefaultPresetIndex()].id == "orbit_seven_stars", "default preset is 七つ星");

    const double sr = 48000.0;
    std::vector<double> hits;
    const auto loop = makeLoop (sr, 4, &hits);
    writeWav (outDir.getChildFile ("00_dry.wav"), loop, sr);

    // ------------------------------------------------------------ ヌルテスト
    {
        PonProcessor p;
        TestPlayHead ph;
        prepare (p, sr, 512, ph);
        const int lat = p.getLatencySamples();
        check (lat == (int) std::lround (0.003 * sr), "latency = 3ms", juce::String (lat));

        setParam (p, ids::radius, 0.0f);
        auto out = render (p, loop, 512, ph, sr, lat);
        check (maxDiff (loop, out, lat, 0, loop.getNumSamples()) < 1e-6f, "null: RADIUS 0 → identical to input",
               juce::String (maxDiff (loop, out, lat, 0, loop.getNumSamples()), 9));

        PonProcessor p2;
        prepare (p2, sr, 512, ph);
        selectPreset (p2, 44);
        setParam (p2, ids::radius, 1.0f);
        setParam (p2, ids::mix, 0.0f);
        out = render (p2, loop, 512, ph, sr, lat);
        const float d = maxDiff (loop, out, lat, (int) (0.1 * sr), loop.getNumSamples());
        check (d < 1e-6f, "null: MIX 0 (粉砕, RADIUS 1) → identical to input", juce::String (d, 9));
    }

    // ------------------------------------------------------------ 全プリセット
    std::printf ("\n%-28s %8s %8s %8s %8s %8s %8s\n", "preset", "peak", "rms", "punch", "dryPunch", "fx(dB)", "side(dB)");
    {
        PonProcessor ref;
        TestPlayHead ph;
        prepare (ref, sr, 512, ph);
        const int lat = ref.getLatencySamples();
        const float dryPunch = punchScore (loop, hits, sr, 0);

        // 基準: RADIUS がほぼ 0（効きなし・位相補正済み）の出力
        juce::AudioBuffer<float> base;
        {
            PonProcessor p;
            prepare (p, sr, 512, ph);
            setParam (p, ids::radius, 0.0005f);
            base = render (p, loop, 512, ph, sr, (int) (1.0 * sr));
        }

        for (int i = 0; i < (int) presets.size(); ++i)
        {
            PonProcessor p;
            prepare (p, sr, 512, ph);
            selectPreset (p, i);
            setParam (p, ids::radius, 1.0f);
            setParam (p, ids::angle, 0.125f);
            const auto out = render (p, loop, 512, ph, sr, (int) (1.0 * sr));
            bool finite = true;
            const float pk = maxAbs (out, finite);
            const float ps = punchScore (out, hits, sr, lat);
            // 効きの大きさ: (出力 - ドライ) のエネルギー / ドライのエネルギー [dB]
            double de = 0.0, dd = 0.0, side = 0.0;
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < loop.getNumSamples(); ++s)
                {
                    const float dry = base.getSample (c, s + lat);
                    const float o = out.getSample (c, s + lat);
                    de += (o - dry) * (o - dry);
                    dd += dry * dry;
                }
            for (int s = 0; s < loop.getNumSamples(); ++s)
                side += 0.25 * std::pow (out.getSample (0, s + lat) - out.getSample (1, s + lat), 2.0);
            std::printf ("%-28s %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f\n", presets[(size_t) i].id.toRawUTF8(),
                         juce::Decibels::gainToDecibels (pk), rmsDb (out, 0, out.getNumSamples()), ps, dryPunch,
                         10.0 * std::log10 (de / dd + 1e-12), 10.0 * std::log10 (2.0 * side / dd + 1e-12));
            if (! finite || pk > 0.9891f || ps < 6.0f)
            {
                ++failures;
                std::printf ("   FAIL: finite=%d peak=%f punch=%f\n", (int) finite, pk, ps);
            }
            writeWav (outDir.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + "_" + presets[(size_t) i].id + ".wav"), out, sr);
        }
    }
    std::printf ("\n");

    // ------------------------------------------------------------ デモ: RADIUS を 0 → 1 へ 8 小節かけて動かす（オートメーション想定）
    for (const char* pid : { "orbit_seven_stars", "chaos_pulverize", "dust_foggy_snare" })
    {
        int pidx = 0;
        for (int i = 0; i < (int) presets.size(); ++i) if (presets[(size_t) i].id == pid) pidx = i;
        const auto longLoop = makeLoop (sr, 8);
        TestPlayHead ph;
        PonProcessor p;
        prepare (p, sr, 512, ph);
        selectPreset (p, pidx);
        juce::AudioBuffer<float> out (2, longLoop.getNumSamples());
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int pos = 0; pos + 512 <= longLoop.getNumSamples(); pos += 512)
        {
            setParam (p, ids::radius, (float) pos / (float) longLoop.getNumSamples());
            for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, longLoop, c, pos, 512);
            ph.ppq = pos / sr * 2.0;
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, 512);
        }
        writeWav (outDir.getChildFile (juce::String ("zz_sweep_") + pid + ".wav"), out, sr);
    }

    // ------------------------------------------------------------ パンが実際に振れているか（振り子 / 左3右2 / アンカー）
    {
        TestPlayHead ph;
        PonProcessor p;
        prepare (p, sr, 64, ph);
        int idx = 0;
        for (int i = 0; i < (int) presets.size(); ++i) if (presets[(size_t) i].id == "orbit_left3_right2") idx = i;
        selectPreset (p, idx);
        setParam (p, ids::radius, 1.0f);
        setParam (p, ids::chaos, 0.0f);   // CHAOS の入れ替えを止めて比だけを見る
        juce::AudioBuffer<float> buf (2, 64);
        juce::MidiBuffer midi;
        int leftBlocks = 0, rightBlocks = 0, anchorBlocks = 0, anchorCentred = 0;
        float maxPan = 0.0f;
        for (int pos = 0; pos + 64 <= loop.getNumSamples(); pos += 64)
        {
            for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, loop, c, pos, 64);
            ph.ppq = pos / sr * 2.0;
            p.processBlock (buf, midi);
            const float pan = p.getEngine().bandPan[1].load();
            maxPan = juce::jmax (maxPan, std::abs (pan));
            if (pan < -0.3f) ++leftBlocks;
            if (pan > 0.3f) ++rightBlocks;
            const double inBar = std::fmod ((pos + 63 - p.getLatencySamples()) / sr * 2.0, 4.0);
            if (inBar > 0.004 && inBar < 0.03) { ++anchorBlocks; if (std::abs (pan) < 0.1f) ++anchorCentred; }
        }
        const float ratio = (float) leftBlocks / (float) juce::jmax (1, rightBlocks);
        check (maxPan > 0.7f, "pan swings hard at RADIUS 1", juce::String (maxPan, 2));
        check (ratio > 1.25f && ratio < 1.8f, "L:R = 3:2 at RADIUS 1", juce::String (leftBlocks) + ":" + juce::String (rightBlocks));
        check (anchorBlocks > 0 && anchorCentred == anchorBlocks, "ANCHOR centres the pan on downbeats",
               juce::String (anchorCentred) + "/" + juce::String (anchorBlocks));
    }

    // ------------------------------------------------------------ 詳細パラメーターの編集（即反映・保存・REVERT）
    {
        TestPlayHead ph;
        PonProcessor p;
        prepare (p, sr, 512, ph);
        int idx = 0;
        for (int i = 0; i < (int) presets.size(); ++i) if (presets[(size_t) i].id == "echo_stairs") idx = i;
        selectPreset (p, idx);
        setParam (p, ids::radius, 1.0f);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        bool finite = true;
        float worstJump = 0.0f, prevL = 0.0f;
        // 再生しながらフィードバック・時間・ワウなどを 1 ブロックごとにいじる
        for (int k = 0; k < 300; ++k)
        {
            p.editCurrentData ([k] (PresetData& d)
            {
                d.dlyFb = 0.2f + 0.7f * (float) (k % 50) / 50.0f;
                d.wah = 0.5f;
                d.band[1].wahQ = 1.0f + (float) (k % 30) * 0.3f;
                d.band[2].dlyMul = (k / 60) % 2 == 0 ? 1.0f : 2.0f;
                d.band[0].on = (k / 100) % 2 == 0 ? 1.0f : 0.0f;
                d.grn = (k / 75) % 2 == 0 ? 0.0f : 0.6f;
            });
            for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, loop, c, (k * 512) % (loop.getNumSamples() - 512), 512);
            ph.ppq = k * 512 / sr * 2.0;
            p.processBlock (buf, midi);
            bool f = true;
            maxAbs (buf, f);
            finite = finite && f;
            for (int i = 0; i < 512; ++i) { worstJump = juce::jmax (worstJump, std::abs (buf.getSample (0, i) - prevL)); prevL = buf.getSample (0, i); }
        }
        check (finite, "live detail edits while playing: no NaN/Inf");
        check (p.isModified() && std::abs (p.getCurrentData().dlyFb - p.getBaseData().dlyFb) > 1e-3f, "edits mark the preset as modified");

        juce::MemoryBlock state;
        p.getStateInformation (state);
        PonProcessor q;
        prepare (q, sr, 512, ph);
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (std::abs (q.getCurrentData().dlyFb - p.getCurrentData().dlyFb) < 1e-6f
                   && std::abs (q.getCurrentData().band[1].wahQ - p.getCurrentData().band[1].wahQ) < 1e-6f
                   && q.isModified(),
               "edited detail values are saved with the project");
        q.revertEdits();
        check (! q.isModified() && std::abs (q.getCurrentData().dlyFb - presets[(size_t) idx].data.dlyFb) < 1e-6f, "REVERT restores the preset");
    }

    // ------------------------------------------------------------ GUI のスナップショット（閉じた状態 / 詳細を開いた状態）
    for (bool open : { false, true })
    {
        TestPlayHead ph;
        PonProcessor p;
        prepare (p, sr, 512, ph);
        setParam (p, ids::radius, 0.65f);
        setParam (p, ids::angle, 0.375f);
        p.setDetailOpen (open);
        render (p, loop, 512, ph, sr);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        const auto f = outDir.getChildFile (open ? "gui_detail.png" : "gui.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png;
        check (os.openedOk() && png.writeImageToStream (img, os) && img.getWidth() == (open ? 1100 : 640), "GUI snapshot", f.getFullPathName());
    }

    // ------------------------------------------------------------ ブロックサイズ
    for (const char* pid : { "drift_lukewarm_fan", "orbit_seven_stars", "echo_stairs", "dust_sandstorm", "chaos_spinning_room", "chaos_pulverize" })
    {
        int pidx = 0;
        for (int i = 0; i < (int) presets.size(); ++i) if (presets[(size_t) i].id == pid) pidx = i;
        TestPlayHead ph;
        juce::AudioBuffer<float> refOut;
        juce::String detail;
        bool ok = true;
        for (int block : { 512, 1, 7, 64, 333, 4096 })
        {
            PonProcessor p;
            prepare (p, sr, block, ph);
            selectPreset (p, pidx);
            setParam (p, ids::radius, 0.8f);
            const auto out = render (p, loop, block, ph, sr);
            if (block == 512) { refOut = out; continue; }
            const float d = maxDiff (refOut, out, 0, 0, out.getNumSamples());
            ok = ok && d < 1e-3f;
            detail << block << ":" << juce::String (d, 5) << " ";
        }
        check (ok, (juce::String ("block size independence ") + pid).toRawUTF8(), detail);
    }

    // ------------------------------------------------------------ サンプルレート（オウム返し: 1 拍後のエコー位置と音量）
    {
        float levels[3] {};
        double echoPos[3] {};
        int k = 0;
        for (double rate : { 44100.0, 96000.0, 192000.0 })
        {
            TestPlayHead ph;
            PonProcessor p;
            prepare (p, rate, 512, ph);
            int parrot = 0;
            for (int i = 0; i < (int) presets.size(); ++i) if (presets[(size_t) i].id == "echo_parrot") parrot = i;
            selectPreset (p, parrot);
            setParam (p, ids::radius, 1.0f);
            setParam (p, ids::punch, 0.0f);
            setParam (p, ids::space, 0.0f);
            // 10ms のトーンバースト（1kHz）を 1 発
            juce::AudioBuffer<float> imp (2, (int) (rate * 1.5));
            imp.clear();
            const int at = (int) (rate * 0.25);   // 拍 0.5
            const int len = (int) (rate * 0.01);
            for (int i = 0; i < len; ++i)
            {
                const float w = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) len);
                const float v = 0.5f * w * (float) std::sin (6.2831853 * 1000.0 * i / rate);
                imp.setSample (0, at + i, v); imp.setSample (1, at + i, v);
            }
            const auto out = render (p, imp, 512, ph, rate);
            const int lat = p.getLatencySamples();
            // 1 拍（0.5 秒）後付近: エンベロープの重心で位置を、RMS で音量を測る
            double wsum = 0.0, tsum = 0.0;
            for (int i = at + lat + (int) (0.3 * rate); i < at + lat + (int) (0.7 * rate); ++i)
            {
                const double v = std::pow (out.getSample (0, i), 2.0) + std::pow (out.getSample (1, i), 2.0);
                wsum += v;
                tsum += v * (i - at - lat - len / 2);
            }
            echoPos[k] = wsum > 0.0 ? tsum / wsum / rate : -1.0;
            levels[k] = juce::Decibels::decibelsToGain (rmsDb (out, at + lat + (int) (0.45 * rate), at + lat + (int) (0.55 * rate)));
            ++k;
        }
        check (std::abs (echoPos[0] - 0.5) < 0.002 && std::abs (echoPos[1] - 0.5) < 0.002 && std::abs (echoPos[2] - 0.5) < 0.002,
               "sample rate: echo at 1 beat for 44.1/96/192k",
               juce::String (echoPos[0], 4) + " " + juce::String (echoPos[1], 4) + " " + juce::String (echoPos[2], 4));
        check (std::abs (juce::Decibels::gainToDecibels (levels[1] / levels[0])) < 1.5f
                   && std::abs (juce::Decibels::gainToDecibels (levels[2] / levels[0])) < 1.5f,
               "sample rate: echo level", juce::String (levels[0], 3) + " " + juce::String (levels[1], 3) + " " + juce::String (levels[2], 3));
    }

    // ------------------------------------------------------------ ストレス（全開・大音量ノイズ・全プリセット・SNAP オフでスイープ）
    {
        TestPlayHead ph;
        juce::Random rng (99);
        juce::AudioBuffer<float> noise (2, (int) (sr * 3));
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < noise.getNumSamples(); ++i)
                noise.setSample (c, i, (rng.nextFloat() * 2.0f - 1.0f) * 3.0f);
        bool allFinite = true;
        float worst = 0.0f;
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            PonProcessor p;
            prepare (p, sr, 256, ph);
            selectPreset (p, i);
            setParam (p, ids::radius, 1.0f);
            setParam (p, ids::chaos, 1.0f);
            setParam (p, ids::space, 1.0f);
            setParam (p, ids::punch, 0.0f);
            setParam (p, ids::output, 12.0f);
            setParam (p, ids::snap, 0.0f);
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            for (int pos = 0; pos + 256 <= noise.getNumSamples(); pos += 256)
            {
                setParam (p, ids::angle, (float) pos / (float) noise.getNumSamples());
                for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, noise, c, pos, 256);
                ph.ppq = pos / sr * 2.0;
                p.processBlock (buf, midi);
                bool f = true;
                worst = juce::jmax (worst, maxAbs (buf, f));
                allFinite = allFinite && f;
            }
        }
        check (allFinite, "stress: no NaN/Inf");
        check (worst <= 0.9891f, "stress: output never exceeds 0dBFS", juce::String (juce::Decibels::gainToDecibels (worst), 2) + " dBFS");
    }

    // ------------------------------------------------------------ モノ入力
    {
        TestPlayHead ph;
        PonProcessor p;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::mono());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.setBusesLayout (layout), "mono → stereo layout accepted");
        prepare (p, sr, 512, ph);
        setParam (p, ids::radius, 1.0f);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        bool f = true;
        for (int k = 0; k < 50; ++k)
        {
            buf.clear();
            for (int i = 0; i < 512; ++i) buf.setSample (0, i, loop.getSample (0, k * 512 + i));
            p.processBlock (buf, midi);
            maxAbs (buf, f);
        }
        check (f, "mono input works");
    }

    // ------------------------------------------------------------ 状態の保存 → 復元
    {
        TestPlayHead ph;
        PonProcessor a;
        prepare (a, sr, 512, ph);
        selectPreset (a, 31);
        setParam (a, ids::radius, 0.7f);
        setParam (a, ids::angle, 0.375f);
        setParam (a, ids::chaos, 0.33f);
        // ユーザープリセット（内部パラメーターを持つ）を経由
        const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pon_test.ponpreset");
        check (a.saveUserPreset (tmp), "user preset save");
        selectPreset (a, 3);
        check (a.loadUserPreset (tmp), "user preset load");
        tmp.deleteFile();
        juce::MemoryBlock state;
        a.getStateInformation (state);

        PonProcessor b;
        prepare (b, sr, 512, ph);
        b.setStateInformation (state.getData(), (int) state.getSize());
        // 同じ状態から作った 2 つのインスタンスの出力を比較
        PonProcessor a2;
        prepare (a2, sr, 512, ph);
        a2.setStateInformation (state.getData(), (int) state.getSize());
        const auto o1 = render (a2, loop, 512, ph, sr);
        const auto o2 = render (b, loop, 512, ph, sr);
        check (maxDiff (o1, o2, 0, 0, o1.getNumSamples()) < 1e-6f, "state round trip reproduces the sound");
        check (b.getCurrentPresetName() == "pon_test" && b.isUserPresetActive(), "state keeps user preset name",
               b.getCurrentPresetName());
        check (std::abs (b.apvts.getRawParameterValue (ids::chaos)->load() - 0.33f) < 1e-4f, "state keeps CHAOS");
    }

    // ------------------------------------------------------------ 決定論（同じ位置 = 同じ壊れ方）
    {
        TestPlayHead ph;
        juce::AudioBuffer<float> o[2];
        for (int k = 0; k < 2; ++k)
        {
            PonProcessor p;
            prepare (p, sr, k == 0 ? 512 : 480, ph);
            selectPreset (p, 14);  // 千鳥足
            setParam (p, ids::radius, 1.0f);
            o[k] = render (p, loop, k == 0 ? 512 : 480, ph, sr);
        }
        check (maxDiff (o[0], o[1], 0, 0, o[0].getNumSamples()) < 1e-4f, "CHAOS is deterministic per position");
    }

    // ------------------------------------------------------------ プリセット切替のクリック
    {
        TestPlayHead ph;
        PonProcessor p;
        prepare (p, sr, 256, ph);
        selectPreset (p, 0);
        setParam (p, ids::radius, 0.8f);
        juce::AudioBuffer<float> tone (2, (int) (sr * 2));
        for (int i = 0; i < tone.getNumSamples(); ++i)
        {
            const float v = 0.3f * (float) std::sin (6.2831853 * 330.0 * i / sr);
            tone.setSample (0, i, v); tone.setSample (1, i, v);
        }
        juce::AudioBuffer<float> out (2, tone.getNumSamples());
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        for (int pos = 0; pos + 256 <= tone.getNumSamples(); pos += 256)
        {
            if (pos == 256 * 150) selectPreset (p, 9);
            if (pos == 256 * 250) selectPreset (p, 22);
            for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, tone, c, pos, 256);
            ph.ppq = pos / sr * 2.0;
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, 256);
        }
        // 2 階差分の最大（クリックは大きなスパイクになる）。330Hz 0.3 振幅の正弦の 2 階差分は約 0.0057
        float worst = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 256 * 140; i < 256 * 300; ++i)
                worst = juce::jmax (worst, std::abs (out.getSample (c, i) - 2.0f * out.getSample (c, i - 1) + out.getSample (c, i - 2)));
        check (worst < 0.05f, "preset switch has no click", juce::String (worst, 4));
        writeWav (outDir.getChildFile ("zz_preset_switch.wav"), out, sr);
    }

    // ------------------------------------------------------------ 余韻と無音時の CPU
    {
        TestPlayHead ph;
        PonProcessor p;
        prepare (p, sr, 512, ph);
        int stairs = 0;
        for (int i = 0; i < (int) presets.size(); ++i) if (presets[(size_t) i].id == "echo_stairs") stairs = i;
        selectPreset (p, stairs);
        setParam (p, ids::radius, 1.0f);
        const auto out = render (p, loop, 512, ph, sr, (int) (sr * 30));
        const int end = loop.getNumSamples();
        check (rmsDb (out, end + (int) (sr * 1), end + (int) (sr * 2)) > -60.0f, "tail rings after input stops",
               juce::String (rmsDb (out, end + (int) (sr * 1), end + (int) (sr * 2)), 1) + " dB");
        check (rmsDb (out, out.getNumSamples() - (int) sr, out.getNumSamples()) < -100.0f,
               "tail eventually stops", juce::String (rmsDb (out, out.getNumSamples() - (int) sr, out.getNumSamples()), 1) + " dB");

        // CPU: 無音が続いた後と、鳴っている間の比較
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        auto timeBlocks = [&] (bool silent)
        {
            const auto t0 = std::chrono::high_resolution_clock::now();
            for (int k = 0; k < 400; ++k)
            {
                if (silent) buf.clear();
                else for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, loop, c, (k * 512) % (loop.getNumSamples() - 512), 512);
                p.processBlock (buf, midi);
            }
            return std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t0).count();
        };
        const double active = timeBlocks (false);
        timeBlocks (true);                      // 余韻を流し切る
        for (int k = 0; k < 4; ++k) timeBlocks (true);
        const double idle = timeBlocks (true);
        check (idle < active * 0.3, "CPU drops on silence",
               "active " + juce::String (active, 1) + "ms / idle " + juce::String (idle, 1) + "ms (400 blocks)");
        std::printf ("       realtime factor (active): %.1fx\n", (400.0 * 512.0 / sr * 1000.0) / active);
    }

    std::printf ("\n%s (%d failure%s)\nrenders: %s\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED",
                 failures, failures == 1 ? "" : "s", outDir.getFullPathName().toRawUTF8());
    return failures == 0 ? 0 : 1;
}
