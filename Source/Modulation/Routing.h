// Source/Modulation/Routing.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** A single source → param routing with a bipolar depth.
 *
 *  sourceId    — the Modulator's id (e.g., "macro1")
 *  paramId     — the APVTS parameter id (e.g., "a_ghost")
 *  depth       — bipolar [-1, +1]; multiplied with modulator value × param range
 *  polarityInv — optional flag to flip the modulator's contribution sign
 *                (cosmetic; same effect could be achieved by negating depth) */
struct Routing
{
    juce::String sourceId;
    juce::String paramId;
    float        depth { 0.0f };
    bool         polarityInverted { false };

    juce::ValueTree toValueTree() const;
    static Routing  fromValueTree(const juce::ValueTree& tree);

    bool operator==(const Routing& other) const noexcept;
};

} // namespace kaigen::phantom
