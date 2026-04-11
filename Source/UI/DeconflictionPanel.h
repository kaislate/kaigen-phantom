#pragma once
#include <JuceHeader.h>
#include "NeumorphicPanel.h"
#include "PhantomKnob.h"

class DeconflictionPanel : public NeumorphicPanel
{
public:
    explicit DeconflictionPanel(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

protected:
    void paintContent(juce::Graphics& g) override;

private:
    juce::ComboBox modeSelector;

    PhantomKnob voicesKnob  { "Voices",  PhantomKnob::Size::Medium, 4.0f };
    PhantomKnob staggerKnob { "Stagger", PhantomKnob::Size::Medium, 8.0f };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboBoxAttachment> modeAttachment;
    std::unique_ptr<SliderAttachment>   voicesAttachment;
    std::unique_ptr<SliderAttachment>   staggerAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeconflictionPanel)
};
