#include <catch2/catch_test_macros.hpp>
#include "../Source/PresetManager.h"

using namespace kaigen::phantom;

TEST_CASE("PresetManager DEV API: createPack creates a folder + manifest", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String packName = "AuthTest-Create-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());

    REQUIRE(pm.createPack(packName, "Some description", "Me"));

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == packName; });

    REQUIRE(it != packs.end());
    CHECK(it->description == "Some description");
    CHECK(it->designer == "Me");

    pm.deletePack(packName);  // teardown
}

TEST_CASE("PresetManager DEV API: createPack refuses reserved names", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    CHECK_FALSE(pm.createPack("Factory", "x", "x"));
    CHECK_FALSE(pm.createPack("User",    "x", "x"));
    CHECK_FALSE(pm.createPack("",        "x", "x"));
}

TEST_CASE("PresetManager DEV API: renamePack moves folder + updates manifest", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String oldName = "AuthTest-Old-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    const juce::String newName = oldName + "-Renamed";

    REQUIRE(pm.createPack(oldName, "d", "me"));
    REQUIRE(pm.renamePack(oldName, newName));

    const auto packs = pm.getAllPacks();
    auto oldIt = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == oldName; });
    auto newIt = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == newName; });

    CHECK(oldIt == packs.end());
    REQUIRE(newIt != packs.end());

    pm.deletePack(newName);  // teardown
}

TEST_CASE("PresetManager DEV API: renamePack refuses readonly packs", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    CHECK_FALSE(pm.renamePack("Factory", "Renamed"));
}

TEST_CASE("PresetManager DEV API: setPackMetadata updates pack.json", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String packName = "AuthTest-Meta-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    REQUIRE(pm.createPack(packName, "old desc", "old designer"));

    REQUIRE(pm.setPackMetadata(packName, "new desc", "new designer"));

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == packName; });
    REQUIRE(it != packs.end());
    CHECK(it->description == "new desc");
    CHECK(it->designer == "new designer");

    pm.deletePack(packName);  // teardown
}

TEST_CASE("PresetManager DEV API: deletePack removes folder", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const juce::String packName = "AuthTest-Del-"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    REQUIRE(pm.createPack(packName, "d", "me"));
    REQUIRE(pm.deletePack(packName));

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == packName; });
    CHECK(it == packs.end());
}

TEST_CASE("PresetManager DEV API: deletePack refuses Factory/User/embedded", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    CHECK_FALSE(pm.deletePack("Factory"));
    CHECK_FALSE(pm.deletePack("User"));
    CHECK_FALSE(pm.deletePack("TestPack"));  // embedded fixture from Task 2
}
