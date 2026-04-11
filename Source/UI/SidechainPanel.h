#pragma once
#include <JuceHeader.h>
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class SidechainPanel : public NeumorphicPanel
{
public:
    explicit SidechainPanel(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

protected:
    void paintContent(juce::Graphics& g) override;

private:
    PhantomKnob amountKnob  { "Amount",  PhantomKnob::Size::Medium,  0.0f };
    PhantomKnob attackKnob  { "Attack",  PhantomKnob::Size::Medium,  5.0f };
    PhantomKnob releaseKnob { "Release", PhantomKnob::Size::Medium, 80.0f };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidechainPanel)
};
