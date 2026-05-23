#include <catch2/catch_test_macros.hpp>
#include "../Source/PresetManager.h"

using namespace kaigen::phantom;

TEST_CASE("FactoryPackLoading: embedded TestPack is discovered", "[factory-pack]")
{
    PresetManager pm;
    pm.initialize();

    const auto packs = pm.getAllPacks();
    auto it = std::find_if(packs.begin(), packs.end(),
        [](const PackInfo& p) { return p.name == "TestPack"; });

    REQUIRE(it != packs.end());
    CHECK(it->displayName == "Test Pack");
    CHECK(it->description == "Embedded fixture for FactoryPackLoadingTests");
    CHECK(it->isReadOnly == true);
    CHECK(it->presetCount == 1);
}

TEST_CASE("FactoryPackLoading: embedded preset appears in getAllPresets", "[factory-pack]")
{
    PresetManager pm;
    pm.initialize();

    const auto all = pm.getAllPresets();
    auto packIt = all.find("TestPack");
    REQUIRE(packIt != all.end());
    REQUIRE(packIt->second.size() == 1);

    const auto& preset = packIt->second.front();
    CHECK(preset.metadata.name == "Empty");
    CHECK(preset.metadata.designer == "Kaigen Test");
    CHECK(preset.embeddedData.getSize() > 0);
    CHECK(preset.file.getFullPathName().isEmpty());
}
