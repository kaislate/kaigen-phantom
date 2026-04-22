#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PresetManager.h"
#include "Parameters.h"

using kaigen::phantom::PresetManager;
using kaigen::phantom::PreviewData;

// Helpers ────────────────────────────────────────────────────────────────

// Build a minimal APVTS-style state tree with an inner <PARAM id="..." value="..."/>
// per parameter. readPreviewFromState walks this shape directly.
static juce::ValueTree makeState(const std::map<juce::String, float>& params)
{
    juce::ValueTree state("STATE");
    for (const auto& [id, value] : params)
    {
        juce::ValueTree p("PARAM");
        p.setProperty("id",    id,    nullptr);
        p.setProperty("value", value, nullptr);
        state.appendChild(p, nullptr);
    }
    return state;
}

// Tests ──────────────────────────────────────────────────────────────────

TEST_CASE("readPreviewFromState extracts all seven harmonic weights")
{
    // Recipe params are stored in APVTS as 0..100 percentages;
    // readPreviewFromState normalizes them to 0..1.
    auto state = makeState({
        { ParamID::RECIPE_H2, 10.0f },
        { ParamID::RECIPE_H3, 20.0f },
        { ParamID::RECIPE_H4, 30.0f },
        { ParamID::RECIPE_H5, 40.0f },
        { ParamID::RECIPE_H6, 50.0f },
        { ParamID::RECIPE_H7, 60.0f },
        { ParamID::RECIPE_H8, 70.0f },
        { ParamID::PHANTOM_THRESHOLD, 120.0f },
        { ParamID::SYNTH_SKIP, 3.0f },
    });

    auto preview = PresetManager::readPreviewFromState(state);

    REQUIRE(preview.h[0] == Catch::Approx(0.10f));
    REQUIRE(preview.h[1] == Catch::Approx(0.20f));
    REQUIRE(preview.h[2] == Catch::Approx(0.30f));
    REQUIRE(preview.h[3] == Catch::Approx(0.40f));
    REQUIRE(preview.h[4] == Catch::Approx(0.50f));
    REQUIRE(preview.h[5] == Catch::Approx(0.60f));
    REQUIRE(preview.h[6] == Catch::Approx(0.70f));
    REQUIRE(preview.crossover == Catch::Approx(120.0f));
    REQUIRE(preview.skip == 3);
}

TEST_CASE("readPreviewFromState returns defaults when params are missing")
{
    juce::ValueTree empty("STATE");
    auto preview = PresetManager::readPreviewFromState(empty);

    for (int i = 0; i < 7; ++i)
        REQUIRE(preview.h[i] == Catch::Approx(0.0f));
    REQUIRE(preview.crossover == Catch::Approx(120.0f));  // matches APVTS default
    REQUIRE(preview.skip == 0);
}

TEST_CASE("readPreviewFromState handles partial params (legacy preset)")
{
    auto state = makeState({
        { ParamID::RECIPE_H3, 75.0f },   // 75% → 0.75 normalized
        { ParamID::PHANTOM_THRESHOLD, 45.0f },
        { ParamID::SYNTH_SKIP, 2.0f },
    });

    auto preview = PresetManager::readPreviewFromState(state);

    // Only H3 was set; all other harmonics should remain at their 0.0 default.
    REQUIRE(preview.h[0] == Catch::Approx(0.0f));   // H2
    REQUIRE(preview.h[1] == Catch::Approx(0.75f));  // H3 (75% normalized)
    REQUIRE(preview.h[2] == Catch::Approx(0.0f));   // H4
    REQUIRE(preview.h[3] == Catch::Approx(0.0f));   // H5
    REQUIRE(preview.h[4] == Catch::Approx(0.0f));   // H6
    REQUIRE(preview.h[5] == Catch::Approx(0.0f));   // H7
    REQUIRE(preview.h[6] == Catch::Approx(0.0f));   // H8
    REQUIRE(preview.crossover == Catch::Approx(45.0f));
    REQUIRE(preview.skip == 2);
}

TEST_CASE("readPreviewFromState clamps skip to valid range")
{
    auto tooHigh = makeState({ { ParamID::SYNTH_SKIP, 42.0f } });
    REQUIRE(PresetManager::readPreviewFromState(tooHigh).skip == 8);

    auto negative = makeState({ { ParamID::SYNTH_SKIP, -3.0f } });
    REQUIRE(PresetManager::readPreviewFromState(negative).skip == 0);

    // Fractional values should round to nearest int.
    auto fraction = makeState({ { ParamID::SYNTH_SKIP, 2.6f } });
    REQUIRE(PresetManager::readPreviewFromState(fraction).skip == 3);
}
