// Source/UI/widgets/PitchDisplay.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Wide OLED-style status readout, segmented into fixed-width fields so
 *  changing one value doesn't reflow the others. Polls the processor on
 *  a 20 Hz timer and shows:
 *
 *      [ A4  +12¢ ]   [ 440 Hz ]   [ WARM ]   [ -12.4 dB ]
 *
 *  Pitch (smoothed via EMA in log-Hz space with a semitone-snap so new
 *  notes still appear instantly), cents offset from the nearest
 *  equal-tempered note, active engine's recipe preset name, and output
 *  peak in dB. Each field renders into its own fixed-width sub-rectangle
 *  so jitter in one field is visually contained. */
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
    float        displayedHz     { -1.0f };
    float        displayedDb     { -200.0f };
    int          displayedRecipe { -1 };

    // Current per-field text. Updated in timerCallback; read in paint.
    juce::String fieldPitch  { "---" };
    juce::String fieldHz     {};
    juce::String fieldRecipe { "---" };
    juce::String fieldDb     { "-inf dB" };

    // Card + per-field bounds computed in resized().
    juce::Rectangle<int> cardBounds;
    juce::Rectangle<int> pitchRect;
    juce::Rectangle<int> hzRect;
    juce::Rectangle<int> recipeRect;
    juce::Rectangle<int> dbRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchDisplay)
};

} // namespace kaigen::phantom
