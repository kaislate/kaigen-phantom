#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Parameters.h"

// Minimal no-op processor to host APVTS for serialization tests
struct TestProcessor : public juce::AudioProcessor
{
    TestProcessor()
        : AudioProcessor(BusesProperties()
            .withInput("In", juce::AudioChannelSet::stereo(), true)
            .withOutput("Out", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, "STATE", createParameterLayout())
    {}
    const juce::String getName() const override { return "Test"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;
};

TEST_CASE("APVTS state round-trips correctly")
{
    TestProcessor proc;

    // Set some non-default values
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.5f);
    proc.apvts.getParameter(ParamID::PHANTOM_THRESHOLD)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::PHANTOM_THRESHOLD)->convertTo0to1(60.0f));

    // Serialize
    auto state = proc.apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    juce::MemoryBlock block;
    proc.copyXmlToBinary(*xml, block);

    // Restore to a fresh processor
    TestProcessor proc2;
    std::unique_ptr<juce::XmlElement> xml2(proc2.getXmlFromBinary(block.getData(), (int)block.getSize()));
    REQUIRE(xml2 != nullptr);
    proc2.apvts.replaceState(juce::ValueTree::fromXml(*xml2));

    float ghostVal = proc2.apvts.getParameter(ParamID::GHOST)->getValue();
    REQUIRE(ghostVal == Catch::Approx(0.5f).margin(0.01f));
}

TEST_CASE("Default parameter values match spec")
{
    TestProcessor proc;

    auto getFloat = [&](const char* id) -> float {
        auto* p = dynamic_cast<juce::RangedAudioParameter*>(
            proc.apvts.getParameter(id));
        return p->convertFrom0to1(p->getDefaultValue());
    };

    REQUIRE(getFloat(ParamID::GHOST)             == Catch::Approx(100.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::PHANTOM_THRESHOLD) == Catch::Approx(80.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::PHANTOM_STRENGTH)  == Catch::Approx(80.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::BINAURAL_WIDTH)    == Catch::Approx(50.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::TRACKING_GLIDE)    == Catch::Approx(20.0f).margin(0.1f));
    REQUIRE(getFloat(ParamID::STAGGER_DELAY)     == Catch::Approx(8.0f).margin(0.1f));
}
