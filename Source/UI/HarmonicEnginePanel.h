#pragma once
#include <JuceHeader.h>
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class HarmonicEnginePanel : public NeumorphicPanel
{
public:
    explicit HarmonicEnginePanel(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

protected:
    void paintContent(juce::Graphics& g) override;

private:
    PhantomKnob saturationKnob { "Saturation", PhantomKnob::Size::Large,  0.0f };
    PhantomKnob strengthKnob   { "Strength",   PhantomKnob::Size::Medium, 80.0f };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> saturationAttachment;
    std::unique_ptr<SliderAttachment> strengthAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicEnginePanel)
};
