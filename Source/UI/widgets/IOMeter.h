// Source/UI/widgets/IOMeter.h
#pragma once
#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Stereo vertical bar meter — two channels rendered side by side.
 *  Converts linear amplitude to dB, normalizes to 0..1 over -60..0 dB range,
 *  paints fill from bottom up. Independent peak-hold per channel; clip
 *  indicator fires per channel when its peak >= -0.1 dBFS.
 *
 *  The 2 peak sources are referenced by const-ref to atomic, so the same
 *  widget works for any std::atomic<float> pair. The meter polls at 30 fps
 *  via Timer. */
class IOMeter : public juce::Component, private juce::Timer
{
public:
    /** Stereo constructor — left + right channel sources. */
    IOMeter(const std::atomic<float>& peakL,
            const std::atomic<float>& peakR);
    ~IOMeter() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    struct ChannelState
    {
        const std::atomic<float>& peakSource;
        float currentLevel { 0.0f };
        float peakHold     { 0.0f };
    };
    ChannelState chanL;
    ChannelState chanR;

    static constexpr float kAttackCoef  = 0.5f;
    static constexpr float kReleaseCoef = 0.08f;
    static constexpr float kPeakHoldDecayPerTick = 0.003f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOMeter)
};

} // namespace kaigen::phantom
