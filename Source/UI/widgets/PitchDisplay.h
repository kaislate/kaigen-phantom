// Source/UI/widgets/PitchDisplay.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Wide OLED-style status readout. Polls the processor on a 20 Hz timer
 *  and renders, when a pitch is detected:
 *
 *      A4  +12¢  ·  440 Hz  ·  WARM  ·  -12.4 dB
 *
 *  Pitch (smoothed via EMA in log-Hz space with a semitone-snap so new
 *  notes appear instantly), cents offset from the nearest equal-tempered
 *  note, active engine's recipe preset name, and output peak in dB.
 *  Renders just "---  ·  WARM  ·  -inf dB" when the engine reports no
 *  detected pitch (recipe and dB stay live). */
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

    // Last values we composed text from — gates repaints on no-op ticks.
    float displayedHz     { -1.0f };
    float displayedDb     { -200.0f };
    int   displayedRecipe { -1 };

    juce::String currentText { "---" };

    // Card geometry computed in resized() so paint() can re-use it.
    juce::Rectangle<int> cardBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchDisplay)
};

} // namespace kaigen::phantom
