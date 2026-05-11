// Source/UI/panels/LeftPanel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/ToggleGroup.h"
#include "../widgets/WordSelector.h"
#include "../widgets/LinkButton.h"
#include "../widgets/RecipeWheel.h"

namespace kaigen::phantom
{

class LeftPanel : public juce::Component, private juce::Slider::Listener
{
public:
    explicit LeftPanel(juce::AudioProcessorValueTreeState& apvts);
    ~LeftPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void sliderValueChanged(juce::Slider* s) override;

    bool filterLinkUpdating { false };  // recursion guard

    juce::AudioProcessorValueTreeState& apvts;

    // Sub-section inset card bounds — computed in resized(), used in paint().
    juce::Rectangle<int> recipeCardBounds;
    juce::Rectangle<int> ghostCardBounds;
    juce::Rectangle<int> filterCardBounds;

    // Recipe wheel + preset selector
    RecipeWheel  recipeWheel;
    WordSelector recipePresetSelector;

    // Ghost section
    PhantomKnob  ghostAmountKnob;
    PhantomKnob  crossoverKnob;
    PhantomKnob  strengthKnob;
    WordSelector ghostModeToggle;

    // Filter section
    PhantomKnob lpfKnob;
    PhantomKnob hpfKnob;
    LinkButton   filterLinkBtn;
    WordSelector filterSlopeToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LeftPanel)
};

} // namespace kaigen::phantom
