// Source/Modulation/ModulationEngine.h
#pragma once
#include "Modulator.h"
#include "Routing.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace kaigen::phantom
{

/** Per-engine modulation container. Owns a set of Modulator instances
 *  scoped to one PhantomEngine (e.g., engine A's instance owns Macro 1
 *  and Macro 2; engine B's owns Macro 3 and Macro 4). Holds the routing
 *  table for that scope. Provides per-param value lookup that combines
 *  the APVTS base with all matching routings.
 *
 *  Per-engine scope is enforced structurally: addRouting() rejects any
 *  routing whose paramId doesn't start with the engine's prefix
 *  (constructor parameter, e.g., "a_" or "b_").
 */
class ModulationEngine
{
public:
    /** Construct.
     *  @param apvtsRef  the APVTS instance (for reading the modulated param's range)
     *  @param prefix    "a_" or "b_" — the engine's APVTS prefix; routings to params
     *                   not starting with this prefix are rejected.
     */
    ModulationEngine(juce::AudioProcessorValueTreeState& apvtsRef, juce::String prefix);

    void addModulator(std::unique_ptr<Modulator> m);

    /** Returns false if the routing's paramId doesn't match the engine's
     *  prefix, or if its sourceId doesn't match a known modulator. */
    bool addRouting(const Routing& r);

    void removeRouting(const juce::String& sourceId, const juce::String& paramId);
    void clearRoutings();

    const std::vector<Routing>& getRoutings() const noexcept { return routings; }

    /** Lookup the modulated value for a given param. Returns base + Σ
     *  (depth × modulator_value × range) clamped to [min, max]. If no
     *  routings match, returns base unchanged. */
    float getModulatedValue(const juce::String& paramId, float base) const;

    /** Persistence: writes <Engine prefix="a_">[modulators...][routings...]</Engine>. */
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& engineNode);

    /** For testing + debug. */
    int  getNumModulators() const noexcept { return (int) modulators.size(); }
    Modulator* findModulator(const juce::String& sourceId) const;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String prefix;
    std::vector<std::unique_ptr<Modulator>> modulators;
    std::vector<Routing> routings;
};

} // namespace kaigen::phantom
