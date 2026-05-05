#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Modulation/Modulator.h"
#include "Modulation/Macro.h"
#include <juce_audio_processors/juce_audio_processors.h>

using namespace kaigen::phantom;
using Catch::Approx;

namespace {

class StubModulator : public Modulator
{
public:
    StubModulator() : Modulator("stub1") {}
    float getCurrentValue() const noexcept override { return 0.5f; }
};

class StubMacroHostProcessor : public juce::AudioProcessor
{
public:
    StubMacroHostProcessor()
        : AudioProcessor(BusesProperties().withInput("In",  juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Out", juce::AudioChannelSet::stereo(), true))
    {}
    const juce::String getName() const override { return "StubMacroHostProcessor"; }
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

juce::AudioProcessorValueTreeState::ParameterLayout makeMacroOnlyLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back(std::make_unique<AudioParameterFloat>(
        "macro1", "Macro 1", NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    return { params.begin(), params.end() };
}

} // namespace

TEST_CASE("Modulator id is preserved", "[modulator]")
{
    StubModulator m;
    REQUIRE(m.getId() == "stub1");
}

TEST_CASE("Modulator getCurrentValue returns subclass value", "[modulator]")
{
    StubModulator m;
    REQUIRE(m.getCurrentValue() == 0.5f);
}

TEST_CASE("Macro reads value from APVTS", "[modulator]")
{
    StubMacroHostProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MACRO_TEST", makeMacroOnlyLayout());

    Macro m("macro1", apvts, "macro1");

    // Default value is 0.0
    REQUIRE(m.getCurrentValue() == 0.0f);

    // Set via host
    if (auto* p = apvts.getParameter("macro1"))
        p->setValueNotifyingHost(0.7f);

    REQUIRE(m.getCurrentValue() == Approx(0.7f).margin(1.0e-5f));
}

TEST_CASE("Macro id is preserved (concrete class)", "[modulator]")
{
    StubMacroHostProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MACRO_TEST", makeMacroOnlyLayout());
    Macro m("macro1", apvts, "macro1");
    REQUIRE(m.getId() == "macro1");
}

TEST_CASE("Macro name defaults to id and can be set", "[modulator]")
{
    StubMacroHostProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MACRO_TEST", makeMacroOnlyLayout());
    Macro m("macro1", apvts, "macro1");
    REQUIRE(m.getName() == "macro1");
    m.setName("Motion");
    REQUIRE(m.getName() == "Motion");
}

TEST_CASE("Macro persistence round-trip preserves name", "[modulator]")
{
    StubMacroHostProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "MACRO_TEST", makeMacroOnlyLayout());

    Macro m1("macro1", apvts, "macro1");
    m1.setName("Motion");

    juce::ValueTree wrapper("ModConfig");
    m1.writeToTree(wrapper);

    Macro m2("macro1", apvts, "macro1");
    REQUIRE(m2.getName() == "macro1");   // pre-restore default
    m2.readFromTree(wrapper);
    REQUIRE(m2.getName() == "Motion");
}
