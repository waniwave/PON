#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class PonProcessor;

// 円形パッド: 角度 = 拍の割り方（8 本のスポーク）、中心からの距離 = カオス量
class PadComponent : public juce::Component,
                     private juce::Timer
{
public:
    explicit PadComponent (PonProcessor& p);
    ~PadComponent() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void setFromMouse (juce::Point<float> pos);
    juce::Point<float> centre() const;
    float padRadius() const;

    PonProcessor& proc;
    juce::ParameterAttachment angleAtt, radiusAtt;
    float angle = 0.125f, radius = 0.0f;
    bool snap = true;
    bool dragging = false;

    static constexpr int kTrail = 10;
    std::array<std::array<float, kTrail>, 3> trails {};
    int trailPos = 0;
    double time = 0.0;
};
