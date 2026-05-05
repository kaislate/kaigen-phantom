#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Modulation/ModulationEngine.h"
#include "Modulation/Macro.h"
#include <juce_audio_processors/juce_audio_processors.h>

using namespace kaigen::phantom;
using Catch::Approx;

namespace {

class StubProc : public juce::AudioProcessor
{
public:
    StubProc()
        : AudioProcessor(BusesProperties().withInput("In",  juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Out", juce::AudioChannelSet::stereo(), true))
    {}
    const juce::String getName() const override { return "StubProc"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

juce::AudioProcessorValueTreeState::ParameterLayout makeRoutingMutationLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back(std::make_unique<AudioParameterFloat>(
        "macro1", "Macro 1", NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "a_ghost", "A. Ghost", NormalisableRange<float>(0.0f, 100.0f), 50.0f));
    return { params.begin(), params.end() };
}

} // namespace

TEST_CASE("RoutingsSnapshot independent of subsequent add", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    auto snapshotBefore = modA.getRoutingsSnapshot();
    REQUIRE(snapshotBefore->empty());

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    REQUIRE(modA.addRouting(r));

    // Snapshot taken before the add still sees the empty list.
    REQUIRE(snapshotBefore->empty());

    auto snapshotAfter = modA.getRoutingsSnapshot();
    REQUIRE(snapshotAfter->size() == 1);
}

TEST_CASE("setRoutingDepth changes existing routing", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    modA.addRouting(r);

    REQUIRE(modA.setRoutingDepth("macro1", "a_ghost", 0.75f));

    auto snapshot = modA.getRoutingsSnapshot();
    REQUIRE(snapshot->size() == 1);
    REQUIRE((*snapshot)[0].depth == 0.75f);
}

TEST_CASE("setRoutingDepth returns false for unknown routing", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    REQUIRE_FALSE(modA.setRoutingDepth("macro1", "a_ghost", 0.5f));
}

TEST_CASE("clearRoutings replaces snapshot atomically", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    modA.addRouting(r);

    auto before = modA.getRoutingsSnapshot();
    REQUIRE(before->size() == 1);

    modA.clearRoutings();

    REQUIRE(before->size() == 1);   // before-snapshot still sees the routing
    REQUIRE(modA.getRoutingsSnapshot()->empty());
}

TEST_CASE("getModulatedValue uses current snapshot, not stale", "[routing-mut]")
{
    StubProc proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "RM_TEST", makeRoutingMutationLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == 50.0f);

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    modA.addRouting(r);

    // macro1 default 0.5; depth 0.5; range 100; modulation = 0.5*0.5*100 = 25.
    // base 50 + 25 = 75.
    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == Approx(75.0f).margin(1.0e-3f));
}
