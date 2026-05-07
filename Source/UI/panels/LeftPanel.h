// Source/UI/panels/LeftPanel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/ToggleGroup.h"
#include "../widgets/LinkButton.h"

namespace kaigen::phantom
{

class LeftPanel : public juce::Component
{
public:
    explicit LeftPanel(juce::AudioProcessorValueTreeState& apvts);
    ~LeftPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Recipe wheel area is a placeholder rectangle until Phase 3.

    // Ghost section
    PhantomKnob ghostAmountKnob;
    PhantomKnob crossoverKnob;
    PhantomKnob strengthKnob;
    ToggleGroup ghostModeToggle;

    // Filter section
    PhantomKnob lpfKnob;
    PhantomKnob hpfKnob;
    LinkButton  filterLinkBtn;
    ToggleGroup filterSlopeToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LeftPanel)
};

} // namespace kaigen::phantom
