#pragma once
#include <JuceHeader.h>
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class PitchTrackerPanel : public NeumorphicPanel
{
public:
    explicit PitchTrackerPanel(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

protected:
    void paintContent(juce::Graphics& g) override;

private:
    PhantomKnob sensitivityKnob { "Sensitivity", PhantomKnob::Size::Medium, 70.0f };
    PhantomKnob glideKnob       { "Glide",       PhantomKnob::Size::Medium, 20.0f };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> sensitivityAttachment;
    std::unique_ptr<SliderAttachment> glideAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchTrackerPanel)
};
