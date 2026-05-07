// Source/UI/visualizers/Spectrum.h
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Native spectrum analyzer. Reads input + output spectrum bins from the
 *  processor (precomputed on the audio thread, ~5.86 Hz update rate) and
 *  draws them as overlaid bar graphs. 30fps Timer-driven repaint. */
class Spectrum : public juce::Component, private juce::Timer
{
public:
    explicit Spectrum(PhantomProcessor& processor);
    ~Spectrum() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    PhantomProcessor& processor;
    static constexpr int kBins = 80;
    std::array<float, kBins> smoothedInput  {};
    std::array<float, kBins> smoothedOutput {};
    static constexpr float kSmoothUp   = 0.6f;
    static constexpr float kSmoothDown = 0.12f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Spectrum)
};

} // namespace kaigen::phantom
