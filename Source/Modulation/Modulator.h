// Source/Modulation/Modulator.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** Abstract base for all modulators (Macro, LFO, Random in subsequent PRs).
 *
 *  A modulator is a source whose output value (typically [0,1] for unipolar
 *  sources like Macro, [-1,1] for bipolar like LFO) is read each audio block
 *  and combined with routing depth to modulate a target APVTS parameter.
 *
 *  Concrete subclasses implement `getCurrentValue()` and the persistence
 *  hooks. The base provides identity (an ID string used by routings to refer
 *  to this modulator). */
class Modulator
{
public:
    virtual ~Modulator() = default;

    /** Identity string. Routings reference modulators by this id. */
    const juce::String& getId() const noexcept { return id; }

    /** Per-block (or per-routing-evaluation) value query. Real-time-safe.
     *  Range depends on subclass — Macro returns [0,1], LFO returns [-1,1]. */
    virtual float getCurrentValue() const noexcept = 0;

    /** Lifecycle. Default no-op; subclasses with state override. */
    virtual void prepareToPlay(double /*sampleRate*/, int /*blockSize*/) {}
    virtual void reset() {}

    /** Serialize subclass-specific state into `parent`. Routings live in
     *  ModulationEngine, not here. */
    virtual void writeToTree(juce::ValueTree& parent) const {}
    virtual void readFromTree(const juce::ValueTree& parent) {}

protected:
    explicit Modulator(juce::String idStr) : id(std::move(idStr)) {}
    juce::String id;
};

} // namespace kaigen::phantom
