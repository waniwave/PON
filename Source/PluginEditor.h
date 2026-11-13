#pragma once

#include "PluginProcessor.h"
#include "PonLookAndFeel.h"
#include "PadComponent.h"
#include "DetailPanel.h"
#include <array>

class PonEditor : public juce::AudioProcessorEditor,
                  private juce::Timer
{
public:
    explicit PonEditor (PonProcessor&);
    ~PonEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetDisplay();
    void savePreset();
    void loadPreset();

    PonProcessor& proc;
    pon::ui::PonLookAndFeel lnf;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { juce::String::fromUTF8 ("◀") }, nextButton { juce::String::fromUTF8 ("▶") };
    juce::TextButton saveButton { "Save" }, loadButton { "Load" };
    juce::ToggleButton snapButton { "SNAP" }, anchorButton { "ANCHOR" };
    juce::Label descLabel;
    juce::TextButton detailButton;

    PadComponent pad;
    DetailPanel detail;
    juce::TooltipWindow tooltips { this, 700 };
    void setDetailOpen (bool open);
    int lastDataVersion = -1;

    static constexpr int kMainW = 640, kDetailW = 460, kH = 780;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    std::array<Knob, 5> knobs;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> snapAtt, anchorAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    int lastUiVersion = -1;
    int lastPresetIndex = -1;
    float meterLevel = 0.0f;
    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PonEditor)
};
