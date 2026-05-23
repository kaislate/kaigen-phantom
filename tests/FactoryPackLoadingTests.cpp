#include <catch2/catch_test_macros.hpp>
#include "../Source/PresetManager.h"
#include "../Source/Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

using namespace kaigen::phantom;

namespace {

// Minimal AudioProcessor subclass that exists solely to host an APVTS for
// testing purposes. Mirrors the StubProcessor pattern from DualEngineHostTests
// — none of the audio methods are exercised, we only need a valid host so
// AudioProcessorValueTreeState can be constructed.
class StubLoadProcessor : public juce::AudioProcessor
{
public:
    StubLoadProcessor()
        : AudioProcessor(BusesProperties()
            .withInput ("In",  juce::AudioChannelSet::stereo(), true)
            .withOutput("Out", juce::AudioChannelSet::stereo(), true))
    {}

    const juce::String getName() const override            { return "StubLoadProcessor"; }
    void prepareToPlay (double, int) override               {}
    void releaseResources() override                        {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override     { return nullptr; }
    bool hasEditor() const override                         { return false; }
    bool acceptsMidi() const override                       { return false; }
    bool producesMidi() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 0.0; }
    int  getNumPrograms() override                          { return 1; }
    int  getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override   {}
    void setStateInformation (const void*, int) override     {}
};

} // namespace

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

TEST_CASE("FactoryPackLoading: loadPreset reads from embedded data", "[factory-pack]")
{
    // Build a minimal APVTS matching the live state type name. The embedded
    // TestPack/Empty.fxp was authored with type "KaigenPhantomState" so the
    // tree-type check at the bottom of loadPreset will accept it.
    StubLoadProcessor dummyProc;
    juce::AudioProcessorValueTreeState apvts(dummyProc, nullptr, "KaigenPhantomState", {});

    PresetManager pm;
    pm.initialize();

    const bool ok = pm.loadPreset(apvts, "Empty", "TestPack");
    REQUIRE(ok);
    CHECK(apvts.state.getType().toString() == "KaigenPhantomState");
    CHECK(apvts.state.getChildWithName("Metadata").isValid());
}
