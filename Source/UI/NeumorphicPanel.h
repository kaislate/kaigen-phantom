#pragma once
#include <JuceHeader.h>

class NeumorphicPanel : public juce::Component
{
public:
    NeumorphicPanel() = default;
    void paint(juce::Graphics& g) override;

protected:
    virtual void paintContent(juce::Graphics& g) { juce::ignoreUnused(g); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeumorphicPanel)
};
