// Source/UI/visualizers/Oscilloscope.h
//
// Full canvas port of Source/WebUI/oscilloscope.js.
// 3-layer triggered waveform display: input (gray), synth (blue, glowing),
// output (white). Zero-crossing trigger, auto-scale, gate threshold lines,
// zero-crossing markers, and legend.  30 Hz Timer-driven repaint.
//
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

class Oscilloscope : public juce::Component, private juce::Timer
{
public:
    explicit Oscilloscope(PhantomProcessor& processor);
    ~Oscilloscope() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    /** Find the first positive-slope zero-crossing in the linearized buffer.
     *  Searches i = 8 to OSC_BUF_SIZE - OSC_DISPLAY_SAMPLES - 8,
     *  matching the JS oscilloscope.js findTrigger() exactly.
     *  Returns the crossing index, or 8 as a free-run fallback. */
    int findTriggerIndex(const std::array<float, 2048>& linear) const noexcept;

    /** Build a juce::Path waveform from a linearized buffer snapshot.
     *  @param linear   Linearized (oldest-first) buffer of kBufSize samples.
     *  @param trigStart  Index of trigger point (start of display window).
     *  @param w          Component width in pixels.
     *  @param h          Component height in pixels.
     *  @param scaleY     h * 0.38 * normScale  (amplitude → pixel scale). */
    juce::Path buildTracePath(const std::array<float, 2048>& linear,
                              int trigStart, float w, float h, float scaleY) const;

    PhantomProcessor& processor;

    static constexpr int kBufSize       = 2048;
    static constexpr int kDisplaySamples = 1024;

    // Linearized snapshots filled each timer tick (oldest-first, length kBufSize).
    std::array<float, kBufSize> linearIn  {};
    std::array<float, kBufSize> linearSyn {};
    std::array<float, kBufSize> linearOut {};

    bool autoScale { false };

    // Smoothed normScale so auto-scale transitions don't pop.
    float currentNormScale { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Oscilloscope)
};

} // namespace kaigen::phantom
