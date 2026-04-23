#include <catch2/catch_test_macros.hpp>
#include "ABSlotManager.h"
#include "Parameters.h"

namespace
{
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
}

TEST_CASE("ABSlotManager compiles and constructs")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::A);
}

TEST_CASE("ABSlotManager constructor initializes both slots from live APVTS")
{
    TestProcessor proc;
    // Set a non-default value so we can check both slots capture it.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.42f);

    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    auto slotA = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A);
    auto slotB = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B);

    REQUIRE(slotA.isValid());
    REQUIRE(slotB.isValid());
    // Both slots should match live state (copyState of same APVTS).
    CHECK(slotA.toXmlString() == slotB.toXmlString());
    CHECK(slotA.toXmlString() == proc.apvts.copyState().toXmlString());
}
