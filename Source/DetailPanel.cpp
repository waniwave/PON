#include "DetailPanel.h"
#include "PluginProcessor.h"
#include "PonLookAndFeel.h"

using namespace pon;
using namespace pon::ui;

namespace
{
using Desc   = DetailPanel::Desc;
using Choice = DetailPanel::Choice;
using Kind   = DetailPanel::Kind;
using Unit   = DetailPanel::Unit;

// ---------------------------------------------------------------- 選択肢
const Choice kRatios[] {
    { 0.25f, "1/4" }, { 1.0f / 3.0f, "1/3" }, { 0.375f, "3/8" }, { 0.5f, "1/2" }, { 2.0f / 3.0f, "2/3" },
    { 0.75f, "3/4" }, { 1.0f, "1" }, { 4.0f / 3.0f, "4/3" }, { 1.5f, "3/2" }, { 2.0f, "2" }, { 3.0f, "3" },
    { 4.0f, "4" }, { 5.0f, "5" }, { 6.0f, "6" }, { 7.0f, "7" }, { 8.0f, "8" } };
const Choice kOnOff[]    { { 0.0f, "Off" }, { 1.0f, "On" } };
const Choice kShapes[]   { { 0.0f, "Sine" }, { 1.0f, "Triangle" }, { 2.0f, "Random step" } };
const Choice kFbTypes[]  { { 0.0f, "Off" }, { 1.0f, "Low-pass" }, { 2.0f, "High-pass" }, { 3.0f, "Band-pass" } };
const Choice kDensity[]  { { 0.5f, "x0.5" }, { 1.0f, "x1" }, { 2.0f, "x2" }, { 4.0f, "x4" }, { 8.0f, "x8" } };
const Choice kBars[]     { { 1.0f, "1 bar" }, { 2.0f, "2 bars" }, { 3.0f, "3 bars" }, { 4.0f, "4 bars" }, { 6.0f, "6 bars" }, { 12.0f, "12 bars" } };
const Choice kSlowRate[] { { 1.0f / 32.0f, "1/32 beat" }, { 0.0625f, "1/16 beat" }, { 0.125f, "1/8 beat" }, { 0.25f, "1/4 beat" }, { 0.5f, "1/2 beat" }, { 1.0f, "1 beat" } };
const Choice kPreDelay[] { { 0.0f, "0" }, { 0.0625f, "1/64" }, { 0.125f, "1/32" }, { 0.25f, "1/16" }, { 0.375f, "1/16." },
                           { 0.5f, "1/8" }, { 0.75f, "1/8." }, { 1.0f, "1/4" } };

// ---------------------------------------------------------------- 行を作る小道具
Desc gF (const char* l, float PresetData::* m, float lo, float hi, float step, float mid, Unit u, const char* tip)
{ return { l, m, nullptr, Kind::Float, lo, hi, step, mid, u, nullptr, 0, tip }; }
Desc bF (const char* l, float BandParams::* m, float lo, float hi, float step, float mid, Unit u, const char* tip)
{ return { l, nullptr, m, Kind::Float, lo, hi, step, mid, u, nullptr, 0, tip }; }
template <size_t N> Desc gC (const char* l, float PresetData::* m, const Choice (&c)[N], const char* tip)
{ return { l, m, nullptr, N == 2 && c == kOnOff ? Kind::Toggle : Kind::Choice, 0, 0, 0, 0, Unit::None, c, (int) N, tip }; }
template <size_t N> Desc bC (const char* l, float BandParams::* m, const Choice (&c)[N], const char* tip)
{ return { l, nullptr, m, N == 2 && c == kOnOff ? Kind::Toggle : Kind::Choice, 0, 0, 0, 0, Unit::None, c, (int) N, tip }; }

std::vector<DetailPanel::Tab> makeTabs()
{
    using P = PresetData;
    using B = BandParams;
    std::vector<DetailPanel::Tab> t;

    t.push_back ({ "BANDS", {
        gF ("Radius curve", &P::curve, 0.5f, 8.0f, 0.1f, 3.0f, Unit::None, "RADIUS の効き方。大きいほど外周近くで急に効く"),
        bC ("Active", &B::on, kOnOff, "このバンドにエフェクトをかけるか（Off = 原音のまま）"),
        bC ("Rate x", &B::mul, kRatios, "R（パッドの角度）に掛ける倍率。ワウ・パン・グレイン・ディレイの基準"),
    } });

    t.push_back ({ "WAH", {
        gF ("Wah depth", &P::wah, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "RADIUS 最大のときのワウの効き"),
        bF ("Amount", &B::wahAmt, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "このバンドへの配分"),
        bF ("Freq low", &B::wahLo, 40.0f, 12000.0f, 1.0f, 700.0f, Unit::Hz, "スイープの下端"),
        bF ("Freq high", &B::wahHi, 80.0f, 16000.0f, 1.0f, 2000.0f, Unit::Hz, "スイープの上端"),
        bF ("Q", &B::wahQ, 0.3f, 12.0f, 0.05f, 2.5f, Unit::None, "レゾナンス"),
        bF ("LP blend", &B::wahLp, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "0% = バンドパス、100% = ローパス"),
        bC ("Shape", &B::wahShape, kShapes, "LFO の形"),
        bC ("Inverse", &B::wahInv, kOnOff, "逆ワウ: 音が大きいほど閉じる"),
        bF ("Phase", &B::wahPhase, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Degree, "LFO の位相のずれ"),
        bC ("Rate x", &B::wahMul, kRatios, "ワウだけ、さらに掛ける倍率"),
    } });

    t.push_back ({ "GRAIN", {
        gF ("Grain mix", &P::grn, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "RADIUS 最大のときのグレインの量"),
        gF ("Pitch drift", &P::grnDrift, 0.0f, 24.0f, 0.1f, 0.0f, Unit::Semitone, "何小節かかけてピッチが下がっていく量"),
        gC ("Drift length", &P::grnDriftBars, kBars, "下がりきるまでの長さ"),
        gF ("Wobble", &P::grnWob, 0.0f, 3.0f, 0.01f, 0.0f, Unit::Semitone, "ゆっくりしたピッチの揺れ"),
        gC ("Wobble rate", &P::grnWobRate, kSlowRate, "揺れの速さ（1拍あたり）"),
        bF ("Amount", &B::grnAmt, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "このバンドへの配分"),
        bF ("Size", &B::grnSize, 5.0f, 400.0f, 1.0f, 60.0f, Unit::Ms, "グレインの長さ"),
        bC ("Density", &B::grnSub, kDensity, "1 ステップあたりの発生回数（グリッド上）"),
        bF ("Voices", &B::grnVoices, 1.0f, 4.0f, 1.0f, 0.0f, Unit::None, "1 回に同時に出すグレインの数"),
        bF ("Pitch", &B::grnPitch, -24.0f, 24.0f, 0.1f, 0.0f, Unit::Semitone, "ピッチ"),
        bF ("Pitch spread", &B::grnSpread, 0.0f, 12.0f, 0.1f, 0.0f, Unit::Semitone, "ピッチの散らばり（±）"),
        bC ("Octave steps", &B::grnOct, kOnOff, "散らばりを -1 / 0 / +1 オクターブに限定"),
        bF ("Offset", &B::grnOffset, 0.0f, 4.0f, 0.01f, 0.5f, Unit::Beat, "どれだけ前の音を読むか"),
        bF ("Jitter", &B::grnJitter, 0.0f, 8.0f, 0.01f, 1.0f, Unit::Beat, "読み出し位置のランダムなずれ"),
        bF ("Reverse", &B::grnRev, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "逆再生になる確率"),
        bC ("Tail only", &B::grnTail, kOnOff, "アタック中はグレインを出さない（余韻のみ）"),
        bF ("Width", &B::grnWidth, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "グレインを左右に散らす幅"),
    } });

    t.push_back ({ "PAN", {
        gF ("Pan depth", &P::pan, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "RADIUS 最大のときのパンの振れ幅"),
        gC ("Spin", &P::rotate, kOnOff, "擬似スペクトル回転（3 バンドが 120° ずつずれて回る）"),
        gC ("Spin rate", &P::rotRate, kSlowRate, "回転の速さ（1拍あたり）"),
        bF ("Amount", &B::panAmt, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "このバンドへの配分"),
        bF ("Ratio L", &B::panL, 0.5f, 5.0f, 0.5f, 0.0f, Unit::Ratio, "1 周期のうち左にいる割合（L:R）"),
        bF ("Ratio R", &B::panR, 0.5f, 5.0f, 0.5f, 0.0f, Unit::Ratio, "1 周期のうち右にいる割合（L:R）"),
        bF ("Phase", &B::panPhase, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Degree, "位相のずれ（180° = 反転）"),
        bF ("Smooth", &B::panSoft, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "0% = 左右に切り替え、100% = なめらかに往復"),
        bF ("Haas", &B::panHaas, 0.0f, 30.0f, 0.1f, 0.0f, Unit::Ms, "振った反対側を遅らせる時間"),
    } });

    t.push_back ({ "DELAY", {
        gF ("Delay level", &P::dly, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "RADIUS 最大のときのディレイ音の量"),
        gF ("Feedback", &P::dlyFb, 0.0f, 0.95f, 0.01f, 0.0f, Unit::Percent, "繰り返しの量"),
        gC ("Ping-pong", &P::dlyPing, kOnOff, "左右交互に跳ねる"),
        gC ("FB filter", &P::dlyFbType, kFbTypes, "繰り返すたびにかかるフィルター"),
        gF ("FB freq", &P::dlyFbFreq, 100.0f, 12000.0f, 1.0f, 2000.0f, Unit::Hz, "フィルターの周波数"),
        gF ("FB Q", &P::dlyFbQ, 0.3f, 4.0f, 0.05f, 0.8f, Unit::None, "フィルターのレゾナンス"),
        gF ("Loop wah", &P::dlyWah, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "ディレイのループ内で動くワウ"),
        gF ("Reverse", &P::dlyRev, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "ディレイ音を逆再生にする量"),
        gF ("Out low cut", &P::dlyOutHP, 20.0f, 2000.0f, 1.0f, 200.0f, Unit::Hz, "ディレイ音のローカット（20Hz = なし）"),
        gF ("Out high cut", &P::dlyOutLP, 500.0f, 20000.0f, 1.0f, 4000.0f, Unit::Hz, "ディレイ音のハイカット（20kHz = なし）"),
        bF ("Send", &B::dlyAmt, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "このバンドからディレイへ送る量"),
        bC ("Division", &B::dlyMul, kRatios, "時間 = 1 ÷ (R × この値) 拍"),
    } });

    t.push_back ({ "SPACE", {
        gF ("Room size", &P::spSize, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "リバーブの大きさと長さ"),
        gF ("Damping", &P::spDamp, 0.0f, 1.0f, 0.01f, 0.0f, Unit::Percent, "高域の減衰"),
        gC ("Pre-delay", &P::spPre, kPreDelay, "プリディレイ（音符、テンポ同期）"),
        gF ("Haas", &P::spHaas, 1.0f, 30.0f, 0.1f, 0.0f, Unit::Ms, "Haas の広がりの時間"),
        gF ("Wet low cut", &P::wetHP, 20.0f, 2000.0f, 1.0f, 200.0f, Unit::Hz, "ウェット全体のローカット（20Hz = なし、RADIUS で効く）"),
        gF ("Wet high cut", &P::wetLP, 500.0f, 20000.0f, 1.0f, 4000.0f, Unit::Hz, "ウェット全体のハイカット（20kHz = なし、RADIUS で効く）"),
    } });
    return t;
}

juce::String formatValue (const Desc& d, float v)
{
    switch (d.unit)
    {
        case Unit::Percent:  return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        case Unit::Hz:       return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        case Unit::Ms:       return juce::String (v, v < 10.0f ? 1 : 0) + " ms";
        case Unit::Semitone: return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " st";
        case Unit::Beat:     return juce::String (v, 2) + " beat";
        case Unit::Degree:   return juce::String (juce::roundToInt (v * 360.0f)) + juce::String::fromUTF8 ("°");
        case Unit::Ratio:    return juce::String (v, 1);
        case Unit::Times:    return "x" + juce::String (v, 2);
        case Unit::None:     break;
    }
    return d.step >= 1.0f ? juce::String (juce::roundToInt (v)) : juce::String (v, 2);
}

int nearestChoice (const Desc& d, float v)
{
    int best = 0;
    for (int i = 1; i < d.numChoices; ++i)
        if (std::abs (d.choices[i].value - v) < std::abs (d.choices[best].value - v))
            best = i;
    return best;
}

const juce::Colour kBandCol[3] { col::low, col::mid, col::high };
constexpr int kLabelX = 12, kLabelW = 104, kColX = 120, kColGap = 5, kRowY = 74, kRowH = 24, kRowPitch = 30;
} // namespace

// ================================================================================================
DetailPanel::DetailPanel (PonProcessor& p) : proc (p)
{
    tabs = makeTabs();
    for (int i = 0; i < (int) tabs.size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (tabs[(size_t) i].name);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4711);
        b->setColour (juce::TextButton::buttonOnColourId, col::accent);
        b->onClick = [this, i] { if (currentTab != i) buildTab (i); };
        addAndMakeVisible (*b);
        tabButtons.push_back (std::move (b));
    }
    revertButton.setTooltip ("Revert all detail edits to the loaded preset");
    revertButton.onClick = [this] { proc.revertEdits(); refresh(); };
    addAndMakeVisible (revertButton);

    buildTab (currentTab);
}

DetailPanel::~DetailPanel() = default;

float DetailPanel::readValue (const Desc& d, int band, const PresetData& data) const
{
    return d.g != nullptr ? data.*(d.g) : data.band[juce::jlimit (0, 2, band)].*(d.b);
}

void DetailPanel::writeValue (const Desc& d, int band, float v)
{
    const Desc* dp = &d;
    proc.editCurrentData ([dp, band, v] (PresetData& data)
    {
        if (dp->g != nullptr) data.*(dp->g) = v;
        else                  data.band[band].*(dp->b) = v;
    });
}

DetailPanel::Cell DetailPanel::makeCell (const Desc& d, int band)
{
    Cell c;
    const auto colour = band < 0 ? col::accent2 : kBandCol[band];
    const Desc* dp = &d;
    if (d.kind == Kind::Float)
    {
        auto s = std::make_unique<juce::Slider> (juce::Slider::LinearBar, juce::Slider::NoTextBox);
        juce::NormalisableRange<double> range (d.min, d.max, d.step);
        if (d.mid > d.min && d.mid < d.max) range.setSkewForCentre (d.mid);
        s->setNormalisableRange (range);
        s->setColour (juce::Slider::trackColourId, colour);
        s->setScrollWheelEnabled (true);
        s->setVelocityBasedMode (false);
        s->textFromValueFunction = [dp] (double v) { return formatValue (*dp, (float) v); };
        auto* raw = s.get();
        s->onValueChange = [this, dp, band, raw]
        {
            if (! updating) writeValue (*dp, juce::jmax (0, band), (float) raw->getValue());
        };
        s->setTooltip (juce::String::fromUTF8 (d.tip));
        addAndMakeVisible (*s);
        c.slider = std::move (s);
    }
    else
    {
        auto cb = std::make_unique<juce::ComboBox>();
        for (int i = 0; i < d.numChoices; ++i)
            cb->addItem (d.choices[i].text, i + 1);
        cb->setColour (juce::ComboBox::arrowColourId, colour);
        cb->setJustificationType (juce::Justification::centred);
        auto* raw = cb.get();
        cb->onChange = [this, dp, band, raw]
        {
            const int id = raw->getSelectedId();
            if (! updating && id > 0)
            {
                writeValue (*dp, juce::jmax (0, band), dp->choices[id - 1].value);
                if (dp->b == &BandParams::on) refresh();   // バンドのオン/オフで他の行の有効表示が変わる
            }
        };
        cb->setTooltip (juce::String::fromUTF8 (d.tip));
        addAndMakeVisible (*cb);
        c.combo = std::move (cb);
    }
    return c;
}

void DetailPanel::buildTab (int index)
{
    rows.clear();
    currentTab = juce::jlimit (0, (int) tabs.size() - 1, index);
    tabButtons[(size_t) currentTab]->setToggleState (true, juce::dontSendNotification);

    for (const auto& d : tabs[(size_t) currentTab].rows)
    {
        Row r;
        r.desc = &d;
        r.label = std::make_unique<juce::Label> (juce::String(), d.label);
        r.label->setFont (uiFont (12.0f));
        r.label->setColour (juce::Label::textColourId, d.g != nullptr ? col::text : col::dim);
        r.label->setTooltip (juce::String::fromUTF8 (d.tip));
        addAndMakeVisible (*r.label);
        if (d.g != nullptr)
            r.cells.push_back (makeCell (d, -1));
        else
            for (int b = 0; b < 3; ++b)
                r.cells.push_back (makeCell (d, b));
        rows.push_back (std::move (r));
    }
    refresh();
    resized();
    repaint();
}

void DetailPanel::setCellValue (Cell& c, const Desc& d, float v, float base)
{
    if (c.slider != nullptr)
    {
        c.slider->setValue (v, juce::dontSendNotification);
        c.slider->setDoubleClickReturnValue (true, base);   // ダブルクリック = 読み込んだプリセットの値
    }
    else if (c.combo != nullptr)
    {
        c.combo->setSelectedId (nearestChoice (d, v) + 1, juce::dontSendNotification);
    }
}

void DetailPanel::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    const auto& data = proc.getCurrentData();
    const auto& base = proc.getBaseData();
    for (auto& r : rows)
    {
        const auto& d = *r.desc;
        for (int i = 0; i < (int) r.cells.size(); ++i)
        {
            const int band = d.g != nullptr ? -1 : i;
            setCellValue (r.cells[(size_t) i], d, readValue (d, band, data), readValue (d, band, base));
            // オフのバンドは薄く表示（On/倍率の行は除く）
            const bool enabled = band < 0 || d.b == &BandParams::on || data.band[band].on > 0.5f;
            r.cells[(size_t) i].comp()->setEnabled (enabled);
            r.cells[(size_t) i].comp()->setAlpha (enabled ? 1.0f : 0.4f);
        }
    }
    revertButton.setEnabled (proc.isModified());
}

void DetailPanel::paint (juce::Graphics& g)
{
    g.fillAll (col::panel);
    g.setColour (col::outline);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());

    const int colW = (getWidth() - kColX - 12 - 2 * kColGap) / 3;
    const char* names[3] { "LOW", "MID", "HIGH" };
    g.setFont (uiFont (11.0f, true));
    for (int b = 0; b < 3; ++b)
    {
        g.setColour (kBandCol[b]);
        g.drawText (names[b], kColX + b * (colW + kColGap), 46, colW, 14, juce::Justification::centred);
    }
    g.setColour (col::dim);
    g.drawText ("120-250Hz", kColX, 62, colW, 10, juce::Justification::centred);
    g.drawText ("250Hz-3kHz", kColX + colW + kColGap, 62, colW, 10, juce::Justification::centred);
    g.drawText ("3kHz-", kColX + 2 * (colW + kColGap), 62, colW, 10, juce::Justification::centred);

    g.setFont (uiFont (10.5f));
    g.drawText (juce::String::fromUTF8 ("ダブルクリック = プリセットの値に戻す"), 12, getHeight() - 24, getWidth() - 24, 16,
                juce::Justification::centredLeft);
}

void DetailPanel::resized()
{
    const int tabW = 56;
    for (int i = 0; i < (int) tabButtons.size(); ++i)
        tabButtons[(size_t) i]->setBounds (12 + i * (tabW + 3), 12, tabW, 28);
    revertButton.setBounds (getWidth() - 12 - 70, 12, 70, 28);

    const int colW = (getWidth() - kColX - 12 - 2 * kColGap) / 3;
    int y = kRowY;
    bool prevGlobal = true;
    for (auto& r : rows)
    {
        const bool global = r.desc->g != nullptr;
        if (! global && prevGlobal && &r != &rows.front()) y += 8;   // 全体の項目とバンドの項目の間を少し空ける
        prevGlobal = global;

        r.label->setBounds (kLabelX, y, kLabelW, kRowH);
        if (global)
            r.cells[0].comp()->setBounds (kColX, y, 3 * colW + 2 * kColGap, kRowH);
        else
            for (int b = 0; b < 3; ++b)
                r.cells[(size_t) b].comp()->setBounds (kColX + b * (colW + kColGap), y, colW, kRowH);
        y += kRowPitch;
    }
}
