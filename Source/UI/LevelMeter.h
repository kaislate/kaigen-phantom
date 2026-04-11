#pragma once
#include <JuceHeader.h>

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter(const juce::String& label);

    void paint(juce::Graphics& g) override;
    void setLevel(float leftPeak, float rightPeak);

private:
    juce::String label;
    float level    = 0.0f;
    float smoothed = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
