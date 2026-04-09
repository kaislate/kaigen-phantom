#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PhantomEditor : public juce::AudioProcessorEditor
{
public:
    explicit PhantomEditor(PhantomProcessor&);
    ~PhantomEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override {}
private:
    PhantomProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomEditor)
};
