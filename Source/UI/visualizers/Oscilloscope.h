// Source/UI/visualizers/Oscilloscope.h
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Native oscilloscope visualizer. Reads PhantomEngine A's oscSynthBuf
 *  (atomic float ring buffer, 2048 samples) and draws the most recent
 *  1024 samples as a continuous waveform path across the component's
 *  width. 30fps Timer-driven repaint. */
class Oscilloscope : public juce::Component, private juce::Timer
{
public:
    explicit Oscilloscope(PhantomProcessor& processor);
    ~Oscilloscope() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    PhantomProcessor& processor;
    static constexpr int kDisplaySamples = 1024;
    std::array<float, kDisplaySamples> snapshot {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Oscilloscope)
};

} // namespace kaigen::phantom
