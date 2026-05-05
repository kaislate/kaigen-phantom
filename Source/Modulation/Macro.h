// Source/Modulation/Macro.h
#pragma once
#include "Modulator.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace kaigen::phantom
{

/** A user-controllable macro modulator. Backed by an APVTS parameter so
 *  the macro value is automatable by the host. Returns [0, 1].
 *
 *  Constructor takes the APVTS instance + the param ID it's tied to.
 *  getCurrentValue() reads the live APVTS value (real-time-safe atomic load).
 *
 *  PR3b adds: name, destinations list edited via UI, "expose to host
 *  automation" toggle (PR3a always exposes — that's the whole point of
 *  using an APVTS param). */
class Macro : public Modulator
{
public:
    Macro(juce::String idStr, juce::AudioProcessorValueTreeState& apvts, juce::String apvtsParamId);

    float getCurrentValue() const noexcept override;

    /** Display name (set via UI in PR3b; defaults to id). */
    void setName(juce::String n) { name = std::move(n); }
    const juce::String& getName() const noexcept { return name; }

    void writeToTree(juce::ValueTree& parent) const override;
    void readFromTree(const juce::ValueTree& parent) override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String apvtsParamId;
    std::atomic<float>* cachedValuePtr { nullptr };  // resolved at construction
    juce::String name;
};

} // namespace kaigen::phantom
