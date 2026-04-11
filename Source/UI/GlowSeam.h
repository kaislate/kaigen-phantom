#pragma once
#include <JuceHeader.h>

class GlowSeam : public juce::Component
{
public:
    enum class Orientation { Horizontal, Vertical };
    explicit GlowSeam(Orientation o) : orientation(o) {}
    void paint(juce::Graphics& g) override;
private:
    Orientation orientation;
};
