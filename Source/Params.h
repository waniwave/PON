#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace pon::ids
{
inline const juce::String angle  { "angle" };
inline const juce::String radius { "radius" };
inline const juce::String mix    { "mix" };
inline const juce::String space  { "space" };
inline const juce::String chaos  { "chaos" };
inline const juce::String punch  { "punch" };
inline const juce::String output { "output" };
inline const juce::String snap   { "snap" };
inline const juce::String anchor { "anchor" };
inline const juce::String preset { "preset" };
}

namespace pon
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
