// Source/UI/widgets/IOMeter.h
#pragma once
#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Vertical bar meter reading a single std::atomic<float> peak source.
 *  Converts linear amplitude to dB, normalizes to 0..1 over -60..0 dB range,
 *  paints fill from bottom up. Includes peak-hold marker (decays slowly)
 *  and a clip indicator (top edge flashes red when raw amplitude >= 0.989,
 *  i.e. peak >= -0.1 dBFS).
 *
 *  The peak source is referenced by const-ref to atomic, so the same widget
 *  works for any std::atomic<float>. The meter polls at 30 fps via Timer. */
class IOMeter : public juce::Component, private juce::Timer
{
public:
    explicit IOMeter(const std::atomic<float>& peakSource);
    ~IOMeter() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    const std::atomic<float>& peakSource;
    float currentLevel { 0.0f };  // smoothed (attack-fast, release-slow)
    float peakHold     { 0.0f };  // raw peak with decay
    static constexpr float kAttackCoef  = 0.5f;
    static constexpr float kReleaseCoef = 0.08f;
    static constexpr float kPeakHoldDecayPerTick = 0.003f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOMeter)
};

} // namespace kaigen::phantom
