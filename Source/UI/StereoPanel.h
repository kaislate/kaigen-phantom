#pragma once
#include <JuceHeader.h>
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class StereoPanel : public NeumorphicPanel
{
public:
    explicit StereoPanel(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

protected:
    void paintContent(juce::Graphics& g) override;

private:
    PhantomKnob widthKnob { "Width", PhantomKnob::Size::Large, 100.0f };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> widthAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoPanel)
};
