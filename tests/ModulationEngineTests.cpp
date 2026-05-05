#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Modulation/ModulationEngine.h"
#include "Modulation/Macro.h"
#include <juce_audio_processors/juce_audio_processors.h>

using namespace kaigen::phantom;
using Catch::Approx;

namespace {

class StubModEngineHost : public juce::AudioProcessor
{
public:
    StubModEngineHost()
        : AudioProcessor(BusesProperties().withInput("In",  juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Out", juce::AudioChannelSet::stereo(), true))
    {}
    const juce::String getName() const override { return "StubModEngineHost"; }
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

juce::AudioProcessorValueTreeState::ParameterLayout makeModEngineTestLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back(std::make_unique<AudioParameterFloat>(
        "macro1", "Macro 1", NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "a_ghost", "A. Ghost", NormalisableRange<float>(0.0f, 100.0f), 50.0f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        "b_ghost", "B. Ghost", NormalisableRange<float>(0.0f, 100.0f), 50.0f));
    return { params.begin(), params.end() };
}

} // namespace

TEST_CASE("ModulationEngine: addRouting rejects wrong-prefix paramId", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r;
    r.sourceId = "macro1";
    r.paramId  = "b_ghost";   // wrong prefix
    r.depth    = 0.5f;
    REQUIRE_FALSE(modA.addRouting(r));
}

TEST_CASE("ModulationEngine: addRouting rejects unknown sourceId", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r;
    r.sourceId = "macro_doesnt_exist";
    r.paramId  = "a_ghost";
    r.depth    = 0.5f;
    REQUIRE_FALSE(modA.addRouting(r));
}

TEST_CASE("ModulationEngine: addRouting accepts valid routing", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r;
    r.sourceId = "macro1";
    r.paramId  = "a_ghost";
    r.depth    = 0.5f;
    REQUIRE(modA.addRouting(r));
    REQUIRE(modA.getRoutings().size() == 1);
}

TEST_CASE("ModulationEngine: getModulatedValue passes base through with no routing", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    REQUIRE(modA.getModulatedValue("a_ghost", 75.0f) == 75.0f);
}

TEST_CASE("ModulationEngine: getModulatedValue applies depth × macro × range", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r;
    r.sourceId = "macro1";
    r.paramId  = "a_ghost";
    r.depth    = 0.5f;
    REQUIRE(modA.addRouting(r));

    // a_ghost range 0..100, span 100. depth 0.5. macro 0 → modulation 0.
    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == Approx(50.0f));

    // Macro = 0.5 → modulation = 0.5 * 0.5 * 100 = 25. base 50 + 25 = 75.
    if (auto* p = apvts.getParameter("macro1"))
        p->setValueNotifyingHost(0.5f);
    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == Approx(75.0f).margin(1.0e-3f));

    // Macro = 1.0 → modulation = 0.5 * 1.0 * 100 = 50. base 50 + 50 = 100.
    if (auto* p = apvts.getParameter("macro1"))
        p->setValueNotifyingHost(1.0f);
    REQUIRE(modA.getModulatedValue("a_ghost", 50.0f) == Approx(100.0f).margin(1.0e-3f));
}

TEST_CASE("ModulationEngine: getModulatedValue clamps to param range", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA(apvts, "a_");
    modA.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));

    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 1.0f;
    modA.addRouting(r);

    if (auto* p = apvts.getParameter("macro1"))
        p->setValueNotifyingHost(1.0f);

    // base 80 + (1 * 1 * 100) = 180 → clamped to 100.
    REQUIRE(modA.getModulatedValue("a_ghost", 80.0f) == Approx(100.0f));
}

TEST_CASE("ModulationEngine: persistence round-trip", "[modengine]")
{
    StubModEngineHost proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MOD_TEST", makeModEngineTestLayout());
    ModulationEngine modA1(apvts, "a_");
    modA1.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));
    Routing r; r.sourceId = "macro1"; r.paramId = "a_ghost"; r.depth = 0.5f;
    modA1.addRouting(r);

    auto tree = modA1.toValueTree();

    ModulationEngine modA2(apvts, "a_");
    modA2.addModulator(std::make_unique<Macro>("macro1", apvts, "macro1"));
    modA2.fromValueTree(tree);

    REQUIRE(modA2.getRoutings().size() == 1);
    REQUIRE(modA2.getRoutings()[0].depth == 0.5f);
    REQUIRE(modA2.getRoutings()[0].sourceId == "macro1");
    REQUIRE(modA2.getRoutings()[0].paramId == "a_ghost");
}
