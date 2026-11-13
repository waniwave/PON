#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pon::ui
{
namespace col
{
    const juce::Colour bg       { 0xff0d0e12 };
    const juce::Colour panel    { 0xff16181e };
    const juce::Colour panel2   { 0xff1f222a };
    const juce::Colour outline  { 0xff2d313b };
    const juce::Colour text     { 0xffe4e6ec };
    const juce::Colour dim      { 0xff7c8290 };
    const juce::Colour accent   { 0xffff4f8b };   // ピンク
    const juce::Colour accent2  { 0xff7c6cff };   // 紫
    const juce::Colour low      { 0xffff6b4a };
    const juce::Colour mid      { 0xffffd34d };
    const juce::Colour high     { 0xff3fd8ff };
}

// 日本語フォント: Windows は Yu Gothic UI（なければ Meiryo）、macOS は Hiragino Sans
inline juce::String japaneseTypefaceName()
{
    static const juce::String name = []
    {
        const auto all = juce::Font::findAllTypefaceNames();
       #if JUCE_MAC
        const char* candidates[] { "Hiragino Sans", "Hiragino Kaku Gothic ProN", "Hiragino Kaku Gothic Pro" };
       #else
        const char* candidates[] { "Yu Gothic UI", "Meiryo", "Meiryo UI", "Yu Gothic" };
       #endif
        for (auto* c : candidates)
            if (all.contains (c))
                return juce::String (c);
        return juce::String();
    }();
    return name;
}

inline juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

class PonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PonLookAndFeel()
    {
        const auto jp = japaneseTypefaceName();
        if (jp.isNotEmpty())
            setDefaultSansSerifTypefaceName (jp);

        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::ComboBox::backgroundColourId, col::panel2);
        setColour (juce::ComboBox::outlineColourId, col::outline);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::arrowColourId, col::accent);
        setColour (juce::ComboBox::focusedOutlineColourId, col::accent);
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::PopupMenu::headerTextColourId, col::dim);
        setColour (juce::TextButton::buttonColourId, col::panel2);
        setColour (juce::TextButton::buttonOnColourId, col::accent);
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ToggleButton::textColourId, col::text);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::BubbleComponent::backgroundColourId, col::panel2);
        setColour (juce::BubbleComponent::outlineColourId, col::accent);
        setColour (juce::TooltipWindow::textColourId, col::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool hot = s.isMouseOverOrDragging();

        const float bodyR = radius - 7.0f;
        juce::ColourGradient grad (col::panel2.brighter (hot ? 0.25f : 0.12f), c.x, c.y - bodyR,
                                   col::panel.darker (0.2f), c.x, c.y + bodyR, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (col::outline.brighter (0.25f));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        const float arcR = radius - 2.5f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (col::outline);
        g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (col::accent);
        g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto p1 = c.getPointOnCircumference (bodyR * 0.3f, angle);
        const auto p2 = c.getPointOnCircumference (bodyR * 0.85f, angle);
        g.setColour (col::text);
        g.drawLine ({ p1, p2 }, 2.2f);
    }

    // 詳細パネルのバー型スライダー（値は中に表示）
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearBar)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, s);
            return;
        }
        auto r = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (0.5f);
        const bool hot = s.isMouseOverOrDragging();
        g.setColour (col::bg);
        g.fillRoundedRectangle (r, 4.0f);

        const auto track = s.findColour (juce::Slider::trackColourId);
        auto fill = r.reduced (1.5f);
        fill.setRight (juce::jlimit (fill.getX(), fill.getRight(), sliderPos));
        g.setColour (track.withAlpha (s.isEnabled() ? (hot ? 0.6f : 0.42f) : 0.15f));
        g.fillRoundedRectangle (fill, 3.0f);

        g.setColour (hot ? track : col::outline);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        g.setColour (s.isEnabled() ? col::text : col::dim);
        g.setFont (uiFont (11.5f, hot));
        g.drawFittedText (s.getTextFromValue (s.getValue()), r.toNearestInt().reduced (4, 0), juce::Justification::centred, 1);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::accent : col::panel2.brighter (highlighted ? 0.1f : 0.0f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? col::accent.brighter (0.3f) : col::outline);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (on ? juce::Colours::black : col::dim);
        g.setFont (uiFont (11.5f, true));
        g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::accent.brighter (highlighted ? 0.15f : 0.0f)
                        : col::panel2.brighter (down ? 0.25f : (highlighted ? 0.12f : 0.0f)));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? col::accent.brighter (0.3f) : (highlighted ? col::accent : col::outline));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override       { return uiFont (12.5f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override                 { return uiFont (14.0f); }
    juce::Font getPopupMenuFont() override                                { return uiFont (14.0f); }
    juce::Font getSliderPopupFont (juce::Slider&) override                { return uiFont (12.5f, true); }
    int getSliderPopupPlacement (juce::Slider&) override                  { return juce::BubbleComponent::above; }
};
} // namespace pon::ui
