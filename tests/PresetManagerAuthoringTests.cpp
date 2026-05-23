#include <catch2/catch_test_macros.hpp>
#include "../Source/PresetManager.h"

using namespace kaigen::phantom;

namespace
{
    // Tests run against the user's REAL preset directory. This RAII helper
    // guarantees the pack folder is removed even if a REQUIRE/CHECK aborts
    // the test before reaching an explicit teardown line.
    struct ScopedAuthPack
    {
        PresetManager& pm;
        juce::String name;
        ~ScopedAuthPack() { pm.deletePack(name); }
    };

    juce::String uniquePackName(const char* purpose)
    {
        return juce::String("AuthTest-") + purpose + "-"
            + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    }
}

TEST_CASE("PresetManager DEV API: createPack creates a folder + manifest", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const auto packName = uniquePackName("Create");

    REQUIRE(pm.createPack(packName, "Some description", "Me"));
    ScopedAuthPack cleanup{pm, packName};

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == packName; });

    REQUIRE(it != packs.end());
    CHECK(it->description == "Some description");
    CHECK(it->designer == "Me");
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

    const auto oldName = uniquePackName("Old");
    const juce::String newName = oldName + "-Renamed";

    REQUIRE(pm.createPack(oldName, "d", "me"));
    REQUIRE(pm.renamePack(oldName, newName));
    ScopedAuthPack cleanup{pm, newName};

    const auto packs = pm.getAllPacks();
    auto oldIt = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == oldName; });
    auto newIt = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == newName; });

    CHECK(oldIt == packs.end());
    REQUIRE(newIt != packs.end());
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

    const auto packName = uniquePackName("Meta");
    REQUIRE(pm.createPack(packName, "old desc", "old designer"));
    ScopedAuthPack cleanup{pm, packName};

    REQUIRE(pm.setPackMetadata(packName, "new desc", "new designer"));

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [&](const PackInfo& p) { return p.name == packName; });
    REQUIRE(it != packs.end());
    CHECK(it->description == "new desc");
    CHECK(it->designer == "new designer");
}

TEST_CASE("PresetManager DEV API: deletePack removes folder", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const auto packName = uniquePackName("Del");
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
