// Source/UI/widgets/PitchDisplay.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** OLED-style pitch / note readout. Polls `processor.currentPitch` on a
 *  20 Hz timer and renders either "<note> · <hz>Hz" (e.g. "A4 · 440Hz")
 *  when a pitch is detected, or "---" when silent. A small "FUND" etched
 *  label sits below the OLED card.
 *
 *  Native equivalent of the WebView's wheel-OLED `#pitchDisplay`. */
class PitchDisplay : public juce::Component, private juce::Timer
{
public:
    explicit PitchDisplay(PhantomProcessor& processor);
    ~PitchDisplay() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    /** Hz → note name (e.g. 440 → "A4", 311.13 → "D#4"). Returns empty
     *  string for non-positive Hz. */
    static juce::String hzToNoteName(float hz);

    PhantomProcessor& processor;
    float lastHz { -1.0f };          // last value read; gates repaints
    juce::String currentText  { "---" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchDisplay)
};

} // namespace kaigen::phantom
