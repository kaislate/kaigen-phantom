#include <catch2/catch_test_macros.hpp>
#include "../Source/PresetManager.h"
#include "../Source/Pack/PackArchive.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>

using namespace kaigen::phantom;

namespace
{
    // Tests run against the user's REAL preset directory. This RAII helper
    // guarantees the pack folder is removed even if a REQUIRE/CHECK aborts
    // the test before reaching an explicit teardown line.
    //
    // Non-copyable, non-movable: a temporary's destructor would delete the
    // pack out from under the in-place guard (cf. the roundtrip test which
    // wraps this in std::make_unique).
    struct ScopedAuthPack
    {
        PresetManager& pm;
        juce::String name;

        ScopedAuthPack(PresetManager& p, juce::String n)
            : pm(p), name(std::move(n)) {}
        ~ScopedAuthPack() { pm.deletePack(name); }

        ScopedAuthPack(const ScopedAuthPack&) = delete;
        ScopedAuthPack& operator=(const ScopedAuthPack&) = delete;
        ScopedAuthPack(ScopedAuthPack&&) = delete;
        ScopedAuthPack& operator=(ScopedAuthPack&&) = delete;
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

TEST_CASE("PresetManager DEV API: setPackCover writes cover.png", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const auto packName = uniquePackName("Cover");
    REQUIRE(pm.createPack(packName, "d", "me"));
    ScopedAuthPack cleanup{pm, packName};

    // Build a tiny PNG on disk to use as the source.
    auto tmpDir = juce::File::createTempFile("kp-cover-src");
    tmpDir.deleteFile();
    tmpDir.createDirectory();
    auto src = tmpDir.getChildFile("art.png");
    {
        juce::Image img(juce::Image::RGB, 32, 32, true);
        juce::FileOutputStream out(src);
        juce::PNGImageFormat fmt;
        REQUIRE(out.openedOk());
        REQUIRE(fmt.writeImageToStream(img, out));
    }

    REQUIRE(pm.setPackCover(packName, src));
    CHECK(pm.getPackCoverFile(packName).existsAsFile());

    tmpDir.deleteRecursively();
}

TEST_CASE("PresetManager DEV API: savePresetIntoPack writes into the target pack", "[pm-dev]")
{
    // StubAuthProcessor — bare juce::AudioProcessor subclass for APVTS construction.
    // Mirror the StubLoadProcessor pattern from tests/FactoryPackLoadingTests.cpp.
    struct StubAuthProcessor : public juce::AudioProcessor
    {
        StubAuthProcessor() : juce::AudioProcessor(juce::AudioProcessor::BusesProperties{}) {}
        const juce::String getName() const override { return "StubAuthProcessor"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
    };

    StubAuthProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "KaigenPhantomState", {});

    PresetManager pm;
    pm.initialize();

    const auto packName = uniquePackName("Save");
    REQUIRE(pm.createPack(packName, "d", "me"));
    ScopedAuthPack cleanup{pm, packName};

    const auto saved = pm.savePresetIntoPack(apvts, packName,
                                              "MyPreset", "Synth", "me", "");
    REQUIRE(saved == "MyPreset");

    const auto all = pm.getAllPresets();
    auto it = all.find(packName);
    REQUIRE(it != all.end());
    CHECK(it->second.size() == 1);
    CHECK(it->second.front().metadata.name == "MyPreset");
}

TEST_CASE("PresetManager DEV API: exportPack + importPack roundtrip", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    const auto packName = uniquePackName("RT");
    REQUIRE(pm.createPack(packName, "d", "me"));
    // Guard the live pack with a unique_ptr so we can release it explicitly
    // after the in-test deletePack, while still cleaning up on a REQUIRE abort
    // between createPack and deletePack. In-place construction (no temporary)
    // because ScopedAuthPack's destructor mutates global filesystem state.
    auto cleanup = std::make_unique<ScopedAuthPack>(pm, packName);

    auto tmp = juce::File::createTempFile("kp-export");
    tmp.deleteFile();
    tmp.createDirectory();
    const auto zip = tmp.getChildFile(packName + ".kaipack");
    REQUIRE(pm.exportPack(packName, zip));
    REQUIRE(zip.existsAsFile());

    // Delete the live pack, then import the .kaipack back.
    REQUIRE(pm.deletePack(packName));
    cleanup.reset();  // pack now gone — drop the original guard

    const auto imported = pm.importPack(zip, /*overwrite*/ false);
    ScopedAuthPack cleanup2{pm, imported};  // guards the re-imported copy
    CHECK(imported == packName);

    tmp.deleteRecursively();
}

TEST_CASE("PresetManager DEV API: importPack refuses overwriting embedded packs", "[pm-dev]")
{
    PresetManager pm;
    pm.initialize();

    // Manufacture a fake .kaipack whose top folder is "TestPack" (the
    // embedded fixture name) — peekPackName should match and importPack
    // should refuse.
    auto tmp = juce::File::createTempFile("kp-conflict");
    tmp.deleteFile();
    tmp.createDirectory();
    auto fakePack = tmp.getChildFile("TestPack");
    fakePack.createDirectory();
    fakePack.getChildFile("pack.json").replaceWithText(R"({"name":"TestPack"})");

    auto zip = tmp.getChildFile("TestPack.kaipack");
    REQUIRE(PackArchive::exportPack(fakePack, zip));

    CHECK(pm.importPack(zip, /*overwrite*/ true).isEmpty());
    tmp.deleteRecursively();
}
