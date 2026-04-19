#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Parameters.h"

TEST_CASE("createParameterLayout contains all required parameter IDs")
{
    auto ids = getAllParameterIDs();

    auto has = [&](const char* id) {
        return std::find(ids.begin(), ids.end(), juce::String(id)) != ids.end();
    };

    // Mode & Global
    REQUIRE(has(ParamID::MODE));
    REQUIRE(has(ParamID::BYPASS));
    REQUIRE(has(ParamID::GHOST));
    REQUIRE(has(ParamID::GHOST_MODE));
    REQUIRE(has(ParamID::PHANTOM_THRESHOLD));
    REQUIRE(has(ParamID::PHANTOM_STRENGTH));
    REQUIRE(has(ParamID::OUTPUT_GAIN));

    // Recipe Engine (Chebyshev H2..H8)
    REQUIRE(has(ParamID::RECIPE_H2));
    REQUIRE(has(ParamID::RECIPE_H3));
    REQUIRE(has(ParamID::RECIPE_H4));
    REQUIRE(has(ParamID::RECIPE_H5));
    REQUIRE(has(ParamID::RECIPE_H6));
    REQUIRE(has(ParamID::RECIPE_H7));
    REQUIRE(has(ParamID::RECIPE_H8));
    REQUIRE(has(ParamID::RECIPE_PRESET));
    REQUIRE(has(ParamID::HARMONIC_SATURATION));

    // Envelope
    REQUIRE(has(ParamID::ENV_ATTACK_MS));
    REQUIRE(has(ParamID::ENV_RELEASE_MS));

    // Binaural
    REQUIRE(has(ParamID::BINAURAL_MODE));
    REQUIRE(has(ParamID::BINAURAL_WIDTH));

    // Stereo
    REQUIRE(has(ParamID::STEREO_WIDTH));

    // Synth filters
    REQUIRE(has(ParamID::SYNTH_STEP));
    REQUIRE(has(ParamID::SYNTH_DUTY));
    REQUIRE(has(ParamID::SYNTH_SKIP));
    REQUIRE(has(ParamID::SYNTH_LPF_HZ));
    REQUIRE(has(ParamID::SYNTH_HPF_HZ));

    // RESYN controls
    REQUIRE(has(ParamID::SYNTH_WAVELET_LENGTH));
    REQUIRE(has(ParamID::SYNTH_GATE_THRESHOLD));
    REQUIRE(has(ParamID::SYNTH_H1));
    REQUIRE(has(ParamID::SYNTH_MIN_SAMPLES));
    REQUIRE(has(ParamID::TRACKING_SPEED));
    REQUIRE(has(ParamID::PUNCH_ENABLED));
    REQUIRE(has(ParamID::PUNCH_AMOUNT));
    REQUIRE(has(ParamID::SYNTH_MAX_SAMPLES));
    REQUIRE(has(ParamID::SYNTH_SUB));
    REQUIRE(has(ParamID::SYNTH_BOOST_THRESHOLD));
    REQUIRE(has(ParamID::SYNTH_BOOST_AMOUNT));

    REQUIRE(ids.size() == 44u);
}

TEST_CASE("ghost parameter default is 100 percent")
{
    auto ghost = std::make_unique<juce::AudioParameterFloat>(
        ParamID::GHOST, "Ghost",
        juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%"));

    REQUIRE(ghost != nullptr);
    REQUIRE(static_cast<float>(*ghost) == Catch::Approx(100.0f));
}
