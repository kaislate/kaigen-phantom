#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
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

TEST_CASE("snapTo to already-active slot is a no-op")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Change live state after construction (slot A has original, slot B too)
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.9f);

    // Active is A. snapTo(A) should NOT commit live to A.
    const auto beforeA = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString();
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::A);
    const auto afterA  = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString();

    CHECK(beforeA == afterA);
    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::A);
}

TEST_CASE("snapTo commits live to active slot, then loads target slot")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Edit live — should be captured into slot A on snapTo(B).
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.3f);

    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::B);

    // Slot A now has the live edit baked in.
    auto slotATree = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A);
    auto ghostNode = slotATree.getChildWithProperty("id", juce::var(ParamID::GHOST));
    REQUIRE(ghostNode.isValid());
    const float ghostValue = (float) ghostNode.getProperty("value");
    CHECK(ghostValue == Catch::Approx(30.0f).epsilon(0.01));
    // GHOST is a 0..100 % parameter; setValueNotifyingHost takes 0..1 normalized → 30.0f.
}
