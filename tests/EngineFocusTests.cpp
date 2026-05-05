#include <catch2/catch_test_macros.hpp>
#include "EngineFocus.h"

using namespace kaigen::phantom;

TEST_CASE("EngineFocus default is A + LINK off", "[focus]")
{
    EngineFocus f;
    REQUIRE(f.activeTab == ActiveTab::A);
    REQUIRE_FALSE(f.linkOn);
}

TEST_CASE("writeEngineFocusToTree + readEngineFocusFromTree round-trip", "[focus]")
{
    juce::ValueTree wrapper("PluginState");
    writeEngineFocusToTree(wrapper, { ActiveTab::B, true });
    auto restored = readEngineFocusFromTree(wrapper);
    REQUIRE(restored.activeTab == ActiveTab::B);
    REQUIRE(restored.linkOn);
}

TEST_CASE("readEngineFocusFromTree returns defaults when child absent", "[focus]")
{
    juce::ValueTree wrapper("PluginState");
    auto f = readEngineFocusFromTree(wrapper);
    REQUIRE(f.activeTab == ActiveTab::A);
    REQUIRE_FALSE(f.linkOn);
}

TEST_CASE("readEngineFocusFromTree default branch — activeTab A, linkOn false", "[focus]")
{
    juce::ValueTree wrapper("PluginState");
    writeEngineFocusToTree(wrapper, { ActiveTab::A, false });
    auto restored = readEngineFocusFromTree(wrapper);
    REQUIRE(restored.activeTab == ActiveTab::A);
    REQUIRE_FALSE(restored.linkOn);
}

TEST_CASE("readEngineFocusFromTree treats unknown activeTab as A", "[focus]")
{
    juce::ValueTree wrapper("PluginState");
    juce::ValueTree focus("EditorFocus");
    focus.setProperty("activeTab", "X", nullptr);
    focus.setProperty("linkOn", true, nullptr);
    wrapper.appendChild(focus, nullptr);
    auto restored = readEngineFocusFromTree(wrapper);
    REQUIRE(restored.activeTab == ActiveTab::A);
    REQUIRE(restored.linkOn);
}
