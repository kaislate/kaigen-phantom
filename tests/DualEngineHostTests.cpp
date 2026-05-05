#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DualEngineHost.h"
#include "Parameters.h"

namespace {

// Minimal AudioProcessor subclass that exists solely to host an APVTS
// for testing purposes. None of the audio methods are exercised — the
// DualEngineHost is the unit under test, and it pulls values from the
// APVTS we attach to this stub.
class StubProcessor : public juce::AudioProcessor
{
public:
    StubProcessor()
        : AudioProcessor(BusesProperties()
            .withInput ("In",  juce::AudioChannelSet::stereo(), true)
            .withOutput("Out", juce::AudioChannelSet::stereo(), true))
    {}

    const juce::String getName() const override            { return "StubProcessor"; }
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

// Helper: silence-in / silence-out at a given morph value.
static void runSilenceTest(float morphAmount)
{
    StubProcessor proc;
    juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PHANTOM_STATE",
                                             createParameterLayout());

    if (auto* p = apvts.getParameter(ParamID::MORPH_AMOUNT))
        p->setValueNotifyingHost(morphAmount);

    kaigen::phantom::DualEngineHost host(apvts);
    host.prepareToPlay(44100.0, 256, 2);

    juce::AudioBuffer<float> buf(2, 256);
    buf.clear();
    host.process(buf, nullptr);

    // Silence in, default per-engine params → silence out at any morph value.
    REQUIRE(buf.getMagnitude(0, 256) == Catch::Approx(0.0f).margin(1.0e-6f));
}

} // namespace

TEST_CASE("DualEngineHost: silence in -> silence out at morph=0", "[host]")
{
    runSilenceTest(0.0f);
}

TEST_CASE("DualEngineHost: silence in -> silence out at morph=0.5", "[host]")
{
    runSilenceTest(0.5f);
}

TEST_CASE("DualEngineHost: silence in -> silence out at morph=1", "[host]")
{
    runSilenceTest(1.0f);
}
