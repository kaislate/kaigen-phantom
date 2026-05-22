// Source/UI/widgets/LevelReadout.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Compact numeric dB readouts for input and output peak levels. Sits
 *  above the In/Out meter column. Reads `peakInL/R` and `peakOutL/R` on
 *  a 20 Hz timer, takes the per-side max, converts to dB, and renders:
 *
 *      IN   -10.4
 *      OUT   -8.2
 *
 *  in two stacked rows on the same OLED-style surface as PitchDisplay.
 *  Each numeric field renders into its own fixed-width sub-rectangle so
 *  digit-count changes don't shift the labels. */
class LevelReadout : public juce::Component, private juce::Timer
{
public:
    explicit LevelReadout(PhantomProcessor& processor);
    ~LevelReadout() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    static juce::String dbToText(float linearPeak);

    PhantomProcessor& processor;

    float displayedInDb  { -200.0f };
    float displayedOutDb { -200.0f };
    juce::String inText  { "-inf" };
    juce::String outText { "-inf" };

    juce::Rectangle<int> cardBounds;
    juce::Rectangle<int> inLabelRect;
    juce::Rectangle<int> inValueRect;
    juce::Rectangle<int> outLabelRect;
    juce::Rectangle<int> outValueRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelReadout)
};

} // namespace kaigen::phantom
