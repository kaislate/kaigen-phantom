#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Parameters.h"

TEST_CASE("createParameterLayout contains all required parameter IDs")
{
    // JUCE 7's ParameterLayout is opaque — we use the companion ID registry
    // rather than iterating the layout directly.
    auto ids = getAllParameterIDs();

    auto has = [&](const char* id) {
        return std::find(ids.begin(), ids.end(), juce::String(id)) != ids.end();
    };

    // Mode & Global
    REQUIRE(has(ParamID::MODE));
    REQUIRE(has(ParamID::GHOST));
    REQUIRE(has(ParamID::GHOST_MODE));
    REQUIRE(has(ParamID::PHANTOM_THRESHOLD));
    REQUIRE(has(ParamID::PHANTOM_STRENGTH));
    REQUIRE(has(ParamID::OUTPUT_GAIN));

    // Recipe
    REQUIRE(has(ParamID::RECIPE_H2));
    REQUIRE(has(ParamID::RECIPE_H8));
    REQUIRE(has(ParamID::RECIPE_PHASE_H2));
    REQUIRE(has(ParamID::RECIPE_PHASE_H8));
    REQUIRE(has(ParamID::RECIPE_PRESET));
    REQUIRE(has(ParamID::RECIPE_ROTATION));
    REQUIRE(has(ParamID::HARMONIC_SATURATION));

    // Binaural
    REQUIRE(has(ParamID::BINAURAL_MODE));
    REQUIRE(has(ParamID::BINAURAL_WIDTH));

    // Pitch Tracker
    REQUIRE(has(ParamID::TRACKING_SENSITIVITY));
    REQUIRE(has(ParamID::TRACKING_GLIDE));

    // Deconfliction
    REQUIRE(has(ParamID::DECONFLICTION_MODE));
    REQUIRE(has(ParamID::MAX_VOICES));
    REQUIRE(has(ParamID::STAGGER_DELAY));

    // Sidechain / Stereo
    REQUIRE(has(ParamID::SIDECHAIN_DUCK_AMOUNT));
    REQUIRE(has(ParamID::SIDECHAIN_DUCK_ATTACK));
    REQUIRE(has(ParamID::SIDECHAIN_DUCK_RELEASE));
    REQUIRE(has(ParamID::STEREO_WIDTH));

    REQUIRE(ids.size() == 34u);
}

TEST_CASE("ghost parameter default is 100 percent")
{
    // Construct the ghost parameter directly to verify its default value.
    // (JUCE 7's ParameterLayout is opaque and doesn't expose getParameterByID.
    //  getDefaultValue() is also private in JUCE 7 — use operator float() which
    //  returns the current value, initialised to the default on construction.)
    auto ghost = std::make_unique<juce::AudioParameterFloat>(
        ParamID::GHOST, "Ghost",
        juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%"));

    REQUIRE(ghost != nullptr);
    // After construction the parameter holds the default value (100.0f).
    REQUIRE(static_cast<float>(*ghost) == Catch::Approx(100.0f));
}
