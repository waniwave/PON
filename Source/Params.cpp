#include "Params.h"
#include "FactoryPresets.h"

namespace pon
{
namespace
{
juce::String percent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

std::unique_ptr<juce::AudioParameterFloat> pct (const juce::String& id, const juce::String& name, float def)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percent)
                                             .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; }));
}
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    static const char* spokeNames[] { "1/2", "1/4", "1/8", "3連", "1/16", "5連", "6連", "7連" };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::angle, 1 }, "Angle", juce::NormalisableRange<float> (0.0f, 1.0f), 0.125f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const int k = (int) std::lround (v * 8.0f) % 8;
            return juce::String::fromUTF8 (spokeNames[k]) + " (" + juce::String (juce::roundToInt (v * 360.0f)) + juce::String::fromUTF8 ("°") + ")";
        })));

    layout.add (pct (ids::radius, "Radius", 0.0f));
    layout.add (pct (ids::mix, "Mix", 1.0f));
    layout.add (pct (ids::space, "Space", 0.25f));
    layout.add (pct (ids::chaos, "Chaos", 0.10f));
    layout.add (pct (ids::punch, "Punch", 0.60f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::output, 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::snap, 1 }, "Snap", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::anchor, 1 }, "Anchor", true));

    juce::StringArray presetNames;
    for (const auto& p : getFactoryPresets())
        presetNames.add (p.category + " > " + p.name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::preset, 1 }, "Preset", presetNames, getDefaultPresetIndex()));

    return layout;
}
} // namespace pon
