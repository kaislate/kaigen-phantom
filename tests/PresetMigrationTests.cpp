#include <catch2/catch_test_macros.hpp>
#include "PresetMigration.h"

using namespace kaigen::phantom;

namespace {
    juce::ValueTree makeLegacyState() {
        // Mimics what a pre-PR1 saved state looks like: APVTSState child
        // with un-prefixed PARAM nodes plus an optional SlotB.
        juce::ValueTree state("PluginState");
        juce::ValueTree apvts("APVTSState");

        auto addParam = [&](const juce::String& id, float value) {
            juce::ValueTree p("PARAM");
            p.setProperty("id", id, nullptr);
            p.setProperty("value", value, nullptr);
            apvts.appendChild(p, nullptr);
        };

        addParam("mode", 1.0f);          // per-engine, no prefix → legacy
        addParam("ghost", 50.0f);
        addParam("recipe_h2", 80.0f);
        addParam("bypass", 0.0f);        // global; should remain unprefixed
        addParam("input_gain", 0.0f);    // global

        state.appendChild(apvts, nullptr);
        return state;
    }
}

TEST_CASE("PresetMigration::isLegacy detects un-prefixed per-engine params", "[migration]")
{
    auto state = makeLegacyState();
    REQUIRE(PresetMigration::isLegacy(state));
}

TEST_CASE("PresetMigration::isLegacy returns false for a_/b_-prefixed state", "[migration]")
{
    juce::ValueTree state("PluginState");
    juce::ValueTree apvts("APVTSState");
    juce::ValueTree p("PARAM");
    p.setProperty("id", "a_mode", nullptr);
    p.setProperty("value", 1.0f, nullptr);
    apvts.appendChild(p, nullptr);
    state.appendChild(apvts, nullptr);

    REQUIRE_FALSE(PresetMigration::isLegacy(state));
}

TEST_CASE("PresetMigration: legacy without SlotB mirrors A to B", "[migration]")
{
    auto state = makeLegacyState();
    PresetMigration::migrateInPlace(state);

    auto apvts = state.getChildWithName("APVTSState");
    REQUIRE(apvts.isValid());

    auto findParam = [&](const juce::String& id) -> juce::ValueTree {
        for (int i = 0; i < apvts.getNumChildren(); ++i) {
            auto c = apvts.getChild(i);
            if (c.hasType("PARAM") && c.getProperty("id").toString() == id) return c;
        }
        return {};
    };

    REQUIRE(findParam("a_ghost").isValid());
    REQUIRE((float) findParam("a_ghost").getProperty("value") == 50.0f);
    REQUIRE(findParam("b_ghost").isValid());
    REQUIRE((float) findParam("b_ghost").getProperty("value") == 50.0f);

    REQUIRE(findParam("bypass").isValid());
    REQUIRE_FALSE(findParam("a_bypass").isValid());
}

TEST_CASE("PresetMigration: legacy with SlotB populates b_ from SlotB values", "[migration]")
{
    auto state = makeLegacyState();
    juce::ValueTree slotB("SlotB");
    {
        juce::ValueTree p("PARAM");
        p.setProperty("id", "ghost", nullptr);
        p.setProperty("value", 75.0f, nullptr);
        slotB.appendChild(p, nullptr);
    }
    state.appendChild(slotB, nullptr);

    PresetMigration::migrateInPlace(state);

    auto apvts = state.getChildWithName("APVTSState");
    auto findParam = [&](const juce::String& id) -> juce::ValueTree {
        for (int i = 0; i < apvts.getNumChildren(); ++i) {
            auto c = apvts.getChild(i);
            if (c.hasType("PARAM") && c.getProperty("id").toString() == id) return c;
        }
        return {};
    };

    REQUIRE((float) findParam("a_ghost").getProperty("value") == 50.0f);
    REQUIRE((float) findParam("b_ghost").getProperty("value") == 75.0f);
    REQUIRE_FALSE(state.getChildWithName("SlotB").isValid());
}

TEST_CASE("PresetMigration: legacy with MorphConfig drops it silently", "[migration]")
{
    auto state = makeLegacyState();
    juce::ValueTree morphCfg("MorphConfig");
    state.appendChild(morphCfg, nullptr);

    PresetMigration::migrateInPlace(state);
    REQUIRE_FALSE(state.getChildWithName("MorphConfig").isValid());
}

TEST_CASE("PresetMigration: idempotent on already-new state", "[migration]")
{
    juce::ValueTree state("PluginState");
    juce::ValueTree apvts("APVTSState");
    juce::ValueTree p("PARAM");
    p.setProperty("id", "a_ghost", nullptr);
    p.setProperty("value", 30.0f, nullptr);
    apvts.appendChild(p, nullptr);
    state.appendChild(apvts, nullptr);

    PresetMigration::migrateInPlace(state);
    REQUIRE(apvts.getNumChildren() == 1);  // unchanged
    REQUIRE((float) apvts.getChild(0).getProperty("value") == 30.0f);
}

TEST_CASE("PresetMigration: legacy SlotB with non-per-engine id silently drops the unknown", "[migration]")
{
    auto state = makeLegacyState();
    juce::ValueTree slotB("SlotB");
    {
        // One per-engine PARAM (should materialize as b_ghost):
        juce::ValueTree p1("PARAM");
        p1.setProperty("id", "ghost", nullptr);
        p1.setProperty("value", 60.0f, nullptr);
        slotB.appendChild(p1, nullptr);

        // One unknown PARAM (should be silently dropped, not copied as b_obsolete_field):
        juce::ValueTree p2("PARAM");
        p2.setProperty("id", "obsolete_field", nullptr);
        p2.setProperty("value", 99.0f, nullptr);
        slotB.appendChild(p2, nullptr);
    }
    state.appendChild(slotB, nullptr);

    PresetMigration::migrateInPlace(state);

    auto apvts = state.getChildWithName("APVTSState");
    auto findParam = [&](const juce::String& id) -> juce::ValueTree {
        for (int i = 0; i < apvts.getNumChildren(); ++i) {
            auto c = apvts.getChild(i);
            if (c.hasType("PARAM") && c.getProperty("id").toString() == id) return c;
        }
        return {};
    };

    REQUIRE(findParam("b_ghost").isValid());
    REQUIRE((float) findParam("b_ghost").getProperty("value") == 60.0f);
    REQUIRE_FALSE(findParam("b_obsolete_field").isValid());
    REQUIRE_FALSE(findParam("obsolete_field").isValid());
}

TEST_CASE("PresetMigration: legacy state with both SlotB and MorphConfig clears both", "[migration]")
{
    auto state = makeLegacyState();
    juce::ValueTree slotB("SlotB");
    {
        juce::ValueTree p("PARAM");
        p.setProperty("id", "ghost", nullptr);
        p.setProperty("value", 70.0f, nullptr);
        slotB.appendChild(p, nullptr);
    }
    state.appendChild(slotB, nullptr);
    juce::ValueTree morphCfg("MorphConfig");
    state.appendChild(morphCfg, nullptr);

    PresetMigration::migrateInPlace(state);

    REQUIRE_FALSE(state.getChildWithName("SlotB").isValid());
    REQUIRE_FALSE(state.getChildWithName("MorphConfig").isValid());
}
