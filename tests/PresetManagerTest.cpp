#include <catch2/catch_all.hpp>
#include "../Source/PresetManager.h"
#include "../Source/PresetTypes.h"

using namespace kaigen::phantom;

TEST_CASE("PresetManager - Directory Structure", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    auto userDir = mgr.getUserPresetsDirectory();
    auto factoryDir = mgr.getFactoryPresetsDirectory();

    REQUIRE(userDir.exists());
    REQUIRE(factoryDir.exists());
}

TEST_CASE("PresetManager - Save and Load Preset", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    // Create dummy .fxp data
    juce::MemoryBlock testData;
    testData.append("TEST", 4);

    // Save preset
    auto savePath = mgr.savePreset("Test Preset", "Piano", testData);
    REQUIRE(savePath.isNotEmpty());
    REQUIRE(juce::File(savePath).exists());

    // Verify file was written
    auto file = juce::File(savePath);
    REQUIRE(file.getSize() == 4);
}

TEST_CASE("PresetManager - Enumerate Presets", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    // Save a test preset
    juce::MemoryBlock testData;
    testData.append("TEST", 4);
    mgr.savePreset("Piano Test", "Piano", testData);

    // Load all presets
    auto allPresets = mgr.getAllPresets();
    REQUIRE(allPresets.find("User") != allPresets.end());
    REQUIRE(!allPresets["User"].empty());
}

TEST_CASE("PresetManager - Search by Name", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    juce::MemoryBlock testData;
    testData.append("TEST", 4);
    mgr.savePreset("Warm Bass", "Bass", testData);
    mgr.savePreset("Deep Drone", "Drone", testData);

    auto results = mgr.searchByName("Warm");
    REQUIRE(results.size() >= 1);
    REQUIRE(results[0].metadata.name == "Warm Bass");
}

TEST_CASE("PresetManager - Favorite Toggle", "[PresetManager]") {
    PresetManager mgr;
    mgr.initialize();

    juce::MemoryBlock testData;
    testData.append("TEST", 4);
    auto savedPath = mgr.savePreset("Fav Test", "Piano", testData);

    mgr.setFavorite(savedPath, true);
    // TODO: verify favorite flag persisted (Phase 2)
}
