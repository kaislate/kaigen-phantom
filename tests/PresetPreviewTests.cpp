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
    auto state = makeState({
        { ParamID::RECIPE_H2, 0.10f },
        { ParamID::RECIPE_H3, 0.20f },
        { ParamID::RECIPE_H4, 0.30f },
        { ParamID::RECIPE_H5, 0.40f },
        { ParamID::RECIPE_H6, 0.50f },
        { ParamID::RECIPE_H7, 0.60f },
        { ParamID::RECIPE_H8, 0.70f },
        { ParamID::PHANTOM_THRESHOLD, 120.0f },
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
}

TEST_CASE("readPreviewFromState returns defaults when params are missing")
{
    juce::ValueTree empty("STATE");
    auto preview = PresetManager::readPreviewFromState(empty);

    for (int i = 0; i < 7; ++i)
        REQUIRE(preview.h[i] == Catch::Approx(0.0f));
    REQUIRE(preview.crossover == Catch::Approx(120.0f));  // matches APVTS default
}

TEST_CASE("readPreviewFromState handles partial params (legacy preset)")
{
    auto state = makeState({
        { ParamID::RECIPE_H3, 0.75f },
        { ParamID::PHANTOM_THRESHOLD, 45.0f },
    });

    auto preview = PresetManager::readPreviewFromState(state);

    // Only H3 was set; all other harmonics should remain at their 0.0 default.
    REQUIRE(preview.h[0] == Catch::Approx(0.0f));   // H2
    REQUIRE(preview.h[1] == Catch::Approx(0.75f));  // H3
    REQUIRE(preview.h[2] == Catch::Approx(0.0f));   // H4
    REQUIRE(preview.h[3] == Catch::Approx(0.0f));   // H5
    REQUIRE(preview.h[4] == Catch::Approx(0.0f));   // H6
    REQUIRE(preview.h[5] == Catch::Approx(0.0f));   // H7
    REQUIRE(preview.h[6] == Catch::Approx(0.0f));   // H8
    REQUIRE(preview.crossover == Catch::Approx(45.0f));
}
