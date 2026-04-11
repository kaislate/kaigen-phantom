#pragma once
#include <JuceHeader.h>
#include "RecipeWheel.h"

class RecipeWheelPanel : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit RecipeWheelPanel(juce::AudioProcessorValueTreeState& apvts);
    ~RecipeWheelPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Delegates to the inner RecipeWheel. */
    void updatePitch(float hz);

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    /** Apply preset amplitudes to the wheel and optionally to APVTS. */
    void applyPreset(int presetIndex);

    juce::AudioProcessorValueTreeState& apvts;

    RecipeWheel wheel;

    juce::TextButton warmBtn   { "Warm"   };
    juce::TextButton aggrBtn   { "Aggr"   };
    juce::TextButton hollowBtn { "Hollow" };
    juce::TextButton denseBtn  { "Dense"  };
    juce::TextButton customBtn { "Custom" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeWheelPanel)
};
