#include "PresetMigration.h"
#include <unordered_set>

namespace kaigen::phantom
{

const std::vector<juce::String>& PresetMigration::getPerEngineLeaves()
{
    static const std::vector<juce::String> kLeaves = {
        "mode", "ghost", "ghost_mode", "phantom_threshold", "phantom_strength",
        "output_gain", "recipe_h2", "recipe_h3", "recipe_h4", "recipe_h5",
        "recipe_h6", "recipe_h7", "recipe_h8", "recipe_preset",
        "harmonic_saturation", "synth_step", "synth_duty", "synth_skip",
        "env_attack_ms", "env_release_ms", "env_source",
        "midi_trigger_enabled", "midi_gate_release",
        "binaural_mode", "binaural_width", "stereo_width",
        "synth_lpf_hz", "synth_hpf_hz", "synth_filter_slope",
        "synth_wavelet_length", "synth_gate_threshold", "synth_h1", "synth_sub",
        "synth_min_samples", "synth_max_samples", "tracking_speed",
        "punch_enabled", "punch_amount",
        "synth_boost_threshold", "synth_boost_amount",
    };
    return kLeaves;
}

bool PresetMigration::isLegacy(const juce::ValueTree& state)
{
    if (state.getChildWithName("SlotB").isValid()) return true;
    if (state.getChildWithName("MorphConfig").isValid()) return true;

    auto apvts = state.getChildWithName("APVTSState");
    if (!apvts.isValid())
        // fallback: maybe state IS the APVTSState (older format).
        apvts = state;

    std::unordered_set<juce::String> perEngine;
    for (const auto& leaf : getPerEngineLeaves()) perEngine.insert(leaf);

    for (int i = 0; i < apvts.getNumChildren(); ++i)
    {
        auto child = apvts.getChild(i);
        if (!child.hasType("PARAM")) continue;
        const auto id = child.getProperty("id").toString();
        if (perEngine.count(id) > 0) return true;  // un-prefixed per-engine
    }
    return false;
}

void PresetMigration::migrateInPlace(juce::ValueTree& state)
{
    if (!isLegacy(state)) return;

    auto apvts = state.getChildWithName("APVTSState");
    if (!apvts.isValid()) return;

    std::unordered_set<juce::String> perEngine;
    for (const auto& leaf : getPerEngineLeaves()) perEngine.insert(leaf);

    // Step 1: rename un-prefixed per-engine params to a_*.
    // Index iteration is stable across setProperty() on children — no
    // listeners are attached to this detached ValueTree pre-replaceState,
    // and we're only mutating each child's id, not the child set.
    for (int i = 0; i < apvts.getNumChildren(); ++i)
    {
        auto child = apvts.getChild(i);
        if (!child.hasType("PARAM")) continue;
        const auto id = child.getProperty("id").toString();
        if (perEngine.count(id) > 0)
            child.setProperty("id", "a_" + id, nullptr);
    }

    // Step 2: collect a_* params (now present after rename).
    juce::Array<juce::ValueTree> aParams;
    for (int i = 0; i < apvts.getNumChildren(); ++i)
    {
        auto child = apvts.getChild(i);
        if (child.hasType("PARAM"))
        {
            const auto id = child.getProperty("id").toString();
            if (id.startsWith("a_")) aParams.add(child);
        }
    }

    // Step 3: handle <SlotB> if present.
    auto slotB = state.getChildWithName("SlotB");
    if (slotB.isValid())
    {
        for (int i = 0; i < slotB.getNumChildren(); ++i)
        {
            auto bChild = slotB.getChild(i);
            if (!bChild.hasType("PARAM")) continue;
            const auto id = bChild.getProperty("id").toString();
            if (perEngine.count(id) == 0) continue;

            juce::ValueTree p("PARAM");
            p.setProperty("id", "b_" + id, nullptr);
            p.setProperty("value", bChild.getProperty("value"), nullptr);
            apvts.appendChild(p, nullptr);
        }
        state.removeChild(slotB, nullptr);
    }
    else
    {
        // Step 3b: mirror A → B.
        for (auto a : aParams)
        {
            const auto aId = a.getProperty("id").toString();
            const auto leaf = aId.substring(2);   // strip "a_"
            juce::ValueTree p("PARAM");
            p.setProperty("id", "b_" + leaf, nullptr);
            p.setProperty("value", a.getProperty("value"), nullptr);
            apvts.appendChild(p, nullptr);
        }
    }

    // Step 4: drop legacy <MorphConfig>.
    // TODO(migration-log): when a project-wide juce::Logger policy lands,
    // emit a one-shot warning here so users notice that arc data was dropped.
    auto morphCfg = state.getChildWithName("MorphConfig");
    if (morphCfg.isValid())
        state.removeChild(morphCfg, nullptr);
}

} // namespace kaigen::phantom
