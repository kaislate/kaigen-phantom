#include <catch2/catch_test_macros.hpp>
#include "SpectrumViewMode.h"

using namespace kaigen::phantom;

TEST_CASE("SpectrumViewMode default is Split", "[spectrum-view]")
{
    juce::ValueTree wrapper("PluginState");
    auto m = readSpectrumViewModeFromTree(wrapper);
    REQUIRE(m == SpectrumViewMode::Split);
}

TEST_CASE("SpectrumViewMode round-trip via helpers (Combined)", "[spectrum-view]")
{
    juce::ValueTree wrapper("PluginState");
    writeSpectrumViewModeToTree(wrapper, SpectrumViewMode::Combined);
    auto m = readSpectrumViewModeFromTree(wrapper);
    REQUIRE(m == SpectrumViewMode::Combined);
}

TEST_CASE("SpectrumViewMode round-trip via helpers (Split)", "[spectrum-view]")
{
    juce::ValueTree wrapper("PluginState");
    writeSpectrumViewModeToTree(wrapper, SpectrumViewMode::Split);
    auto m = readSpectrumViewModeFromTree(wrapper);
    REQUIRE(m == SpectrumViewMode::Split);
}

TEST_CASE("SpectrumViewMode treats unknown mode string as Split", "[spectrum-view]")
{
    juce::ValueTree wrapper("PluginState");
    juce::ValueTree node("SpectrumView");
    node.setProperty("mode", "BogusValue", nullptr);
    wrapper.appendChild(node, nullptr);
    auto m = readSpectrumViewModeFromTree(wrapper);
    REQUIRE(m == SpectrumViewMode::Split);
}
