#include "PadComponent.h"
#include "PluginProcessor.h"
#include "PonLookAndFeel.h"

using namespace pon::ui;

namespace
{
const char* kSpokeLabels[] { "1/2", "1/4", "1/8", "3連", "1/16", "5連", "6連", "7連" };

juce::Point<float> polar (juce::Point<float> c, float r, float turn)   // turn: 0 = 真上、時計回り
{
    const float a = turn * juce::MathConstants<float>::twoPi;
    return { c.x + std::sin (a) * r, c.y - std::cos (a) * r };
}
}

PadComponent::PadComponent (PonProcessor& p)
    : proc (p),
      angleAtt (*p.apvts.getParameter (pon::ids::angle), [this] (float v) { angle = v; repaint(); }, nullptr),
      radiusAtt (*p.apvts.getParameter (pon::ids::radius), [this] (float v) { radius = v; repaint(); }, nullptr)
{
    angleAtt.sendInitialUpdate();
    radiusAtt.sendInitialUpdate();
    setOpaque (false);
    startTimerHz (30);
}

PadComponent::~PadComponent()
{
    stopTimer();
}

juce::Point<float> PadComponent::centre() const
{
    return getLocalBounds().toFloat().getCentre();
}

float PadComponent::padRadius() const
{
    return juce::jmin (getWidth(), getHeight()) * 0.5f - 32.0f;
}

void PadComponent::timerCallback()
{
    snap = proc.apvts.getRawParameterValue (pon::ids::snap)->load() > 0.5f;
    auto& eng = proc.getEngine();
    for (int b = 0; b < 3; ++b)
        trails[(size_t) b][(size_t) trailPos] = eng.bandPan[b].load (std::memory_order_relaxed);
    trailPos = (trailPos + 1) % kTrail;
    time += 1.0 / 30.0;
    repaint();
}

void PadComponent::paint (juce::Graphics& g)
{
    const auto c = centre();
    const float R = padRadius();
    const float twoPi = juce::MathConstants<float>::twoPi;

    // 背景
    {
        juce::ColourGradient grad (col::panel2.brighter (0.05f), c.x, c.y, col::bg, c.x + R * 1.1f, c.y, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    }

    // RADIUS に応じて同心円が荒れる
    const float t = (float) time;
    for (int k = 1; k <= 5; ++k)
    {
        const float baseR = R * (float) k / 6.0f;
        const float amp = radius * radius * 7.0f * (0.6f + 0.15f * (float) k);
        juce::Path ring;
        constexpr int seg = 72;
        for (int s = 0; s <= seg; ++s)
        {
            const float th = (float) s / (float) seg;
            const float a = th * twoPi;
            const float wob = amp * (std::sin (a * (float) (3 + k) + t * (1.0f + 0.3f * (float) k))
                                     + 0.5f * std::sin (a * (float) (7 + k) - t * 1.7f));
            const auto pt = polar (c, baseR + wob, th);
            if (s == 0) ring.startNewSubPath (pt); else ring.lineTo (pt);
        }
        ring.closeSubPath();
        g.setColour (col::accent2.withAlpha (0.10f + 0.12f * radius));
        g.strokePath (ring, juce::PathStrokeType (1.0f));
    }

    // 外周
    g.setColour (col::outline.brighter (0.3f));
    g.drawEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f, 1.5f);

    // スポークとラベル
    const int sel = pon::nearestSpoke (angle);
    for (int k = 0; k < pon::kNumSpokes; ++k)
    {
        const float turn = (float) k / (float) pon::kNumSpokes;
        const bool lit = snap && k == sel;
        const auto end = polar (c, R, turn);
        if (lit)
        {
            g.setColour (col::accent.withAlpha (0.25f));
            g.drawLine ({ c, end }, 7.0f);
        }
        g.setColour (lit ? col::accent : col::outline.brighter (0.15f));
        g.drawLine ({ c, end }, lit ? 2.0f : 1.0f);

        const auto lp = polar (c, R + 18.0f, turn);
        g.setColour (lit ? col::accent : col::dim);
        g.setFont (uiFont (14.0f, lit));
        g.drawText (juce::String::fromUTF8 (kSpokeLabels[k]),
                    juce::Rectangle<float> (lp.x - 24.0f, lp.y - 9.0f, 48.0f, 18.0f), juce::Justification::centred);
    }

    // バンドの点（横位置 = 今のパン）
    const juce::Colour bandCol[3] { col::low, col::mid, col::high };
    const char* bandName[3] { "LOW", "MID", "HIGH" };
    const float trackW = R * 0.55f;
    for (int b = 0; b < 3; ++b)
    {
        const float y = c.y + R * 0.42f + (float) (b - 1) * 20.0f;
        g.setColour (col::outline.withAlpha (0.8f));
        g.drawLine (c.x - trackW, y, c.x + trackW, y, 1.0f);
        g.setColour (bandCol[b].withAlpha (0.55f));
        g.setFont (uiFont (9.5f, true));
        g.drawText (bandName[b], juce::Rectangle<float> (c.x - trackW - 40.0f, y - 6.0f, 34.0f, 12.0f), juce::Justification::centredRight);

        for (int i = 1; i <= kTrail; ++i)
        {
            const int idx = (trailPos - i + kTrail * 2) % kTrail;
            const float x = c.x + trails[(size_t) b][(size_t) idx] * trackW;
            const float a = i == 1 ? 1.0f : 0.35f * (1.0f - (float) i / (float) kTrail);
            const float rr = i == 1 ? 5.5f : 3.5f;
            g.setColour (bandCol[b].withAlpha (a));
            g.fillEllipse (x - rr, y - rr, rr * 2.0f, rr * 2.0f);
        }
    }

    // 玉
    const float shownTurn = snap ? (float) sel / (float) pon::kNumSpokes : angle;
    const auto ball = polar (c, radius * R, shownTurn);
    g.setColour (col::accent.withAlpha (0.25f));
    g.drawLine ({ c, ball }, 1.5f);
    for (int i = 3; i >= 1; --i)
    {
        const float rr = 13.0f + (float) i * 5.0f * (0.5f + radius);
        g.setColour (col::accent.withAlpha (0.06f * (float) (4 - i)));
        g.fillEllipse (ball.x - rr, ball.y - rr, rr * 2.0f, rr * 2.0f);
    }
    g.setColour (col::accent);
    g.fillEllipse (ball.x - 12.0f, ball.y - 12.0f, 24.0f, 24.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillEllipse (ball.x - 4.5f, ball.y - 4.5f, 9.0f, 9.0f);

    // 中心の小さな表示
    g.setColour (col::dim);
    g.setFont (uiFont (11.0f));
    const double rate = pon::angleToRate (angle, snap);
    const juce::String rateText = snap ? juce::String::fromUTF8 (kSpokeLabels[sel])
                                       : (juce::String (rate, 2) + "/beat");
    g.drawText (rateText + "   RADIUS " + juce::String (juce::roundToInt (radius * 100.0f)) + "%",
                juce::Rectangle<float> (c.x - 110.0f, c.y - R * 0.42f - 8.0f, 220.0f, 16.0f), juce::Justification::centred);
}

void PadComponent::setFromMouse (juce::Point<float> pos)
{
    const auto c = centre();
    const float R = padRadius();
    const auto d = pos - c;
    const float r = juce::jlimit (0.0f, 1.0f, d.getDistanceFromOrigin() / R);   // 外周の外でも外周に留まる
    float turn = std::atan2 (d.x, -d.y) / juce::MathConstants<float>::twoPi;
    if (turn < 0.0f) turn += 1.0f;
    if (r > 0.02f)
        angleAtt.setValueAsPartOfGesture (turn);
    radiusAtt.setValueAsPartOfGesture (r);
}

void PadComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.getNumberOfClicks() > 1) return;
    dragging = true;
    angleAtt.beginGesture();
    radiusAtt.beginGesture();
    setFromMouse (e.position);
}

void PadComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging) setFromMouse (e.position);
}

void PadComponent::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    angleAtt.endGesture();
    radiusAtt.endGesture();
}

void PadComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    // 中心付近をダブルクリック → 玉が中心へ（エフェクトなし）
    if (e.position.getDistanceFrom (centre()) < padRadius() * 0.3f)
        radiusAtt.setValueAsCompleteGesture (0.0f);
}
