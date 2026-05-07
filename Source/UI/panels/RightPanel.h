// Source/UI/panels/RightPanel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"

namespace kaigen::phantom
{

class RightPanel : public juce::Component
{
public:
    explicit RightPanel(juce::AudioProcessorValueTreeState& apvts);
    ~RightPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Harmonic Engine section
    PhantomKnob saturationKnob;
    PhantomKnob shapeKnob;
    PhantomKnob skipKnob;

    // Stereo section
    PhantomKnob widthKnob;

    // Levels section
    PhantomKnob inGainKnob;
    PhantomKnob outGainKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RightPanel)
};

} // namespace kaigen::phantom
