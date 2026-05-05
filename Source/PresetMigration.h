// Source/PresetMigration.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

namespace kaigen::phantom
{

class PresetMigration
{
public:
    /** Returns true if the state appears to be in the legacy (pre-dual-engine)
     *  format. A state is legacy if any per-engine param appears with no
     *  a_/b_ prefix, OR if <SlotB> / <MorphConfig> children are present.
     *
     *  Note: presence of <SlotB> or <MorphConfig> is treated as legacy
     *  regardless of their contents. An already-migrated state with an
     *  orphaned/empty <SlotB> child will be considered legacy and run
     *  through migrateInPlace; the migration converges to a valid new-
     *  format state in that case (the empty SlotB is simply removed). */
    static bool isLegacy(const juce::ValueTree& state);

    /** Migrates a legacy state to the new format in place. Idempotent on
     *  already-new states (returns immediately). */
    static void migrateInPlace(juce::ValueTree& state);

    /** Returns the list of param-id leaves that are per-engine.
     *  Used internally; exposed for testability. */
    static const std::vector<juce::String>& getPerEngineLeaves();
};

} // namespace kaigen::phantom
