#pragma once
#include <JuceHeader.h>
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class GhostPanel : public NeumorphicPanel
{
public:
    explicit GhostPanel(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

protected:
    void paintContent(juce::Graphics& g) override;

private:
    PhantomKnob amountKnob    { "Amount",    PhantomKnob::Size::Large,  100.0f };
    PhantomKnob thresholdKnob { "Threshold", PhantomKnob::Size::Medium,  80.0f };

    juce::TextButton replaceBtn { "Rpl" };
    juce::TextButton addBtn     { "Add" };

    // Raw pointer kept for GHOST_MODE button callbacks (AudioParameterChoice)
    juce::AudioProcessorValueTreeState* apvtsPtr { nullptr };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<SliderAttachment> thresholdAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GhostPanel)
};
