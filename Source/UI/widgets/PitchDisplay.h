// Source/UI/widgets/PitchDisplay.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Wide OLED-style pitch / note readout. Polls `processor.currentPitch`
 *  on a 20 Hz timer, runs a smoothing pass on the raw Hz (EMA in log-Hz
 *  space; snaps on big jumps so legitimate new notes still appear
 *  instantly), and renders:
 *
 *      A4  +12¢  ·  440 Hz
 *
 *  Note name + cents-offset from nearest equal-tempered note + Hz value.
 *  Renders "---" when the engine reports no detected pitch. */
class PitchDisplay : public juce::Component, private juce::Timer
{
public:
    explicit PitchDisplay(PhantomProcessor& processor);
    ~PitchDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    /** Hz → note name (e.g. 440 → "A4", 311.13 → "D#4"). Returns empty
     *  string for non-positive Hz. */
    static juce::String hzToNoteName(float hz);

    /** Cents offset from the nearest equal-tempered note. Range about
     *  [-50, +50]. Returns 0 for non-positive Hz. */
    static int hzToCents(float hz);

    PhantomProcessor& processor;

    // EMA-smoothed pitch in log-Hz space. -1 = no pitch detected yet.
    float smoothedHz   { -1.0f };
    float displayedHz  { -1.0f };       // last value we painted (gates repaints)
    juce::String currentText { "---" };

    // Card geometry computed in resized() so paint() can re-use it.
    juce::Rectangle<int> cardBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchDisplay)
};

} // namespace kaigen::phantom
