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

TEST_CASE("snapTo with includeDiscreteInSnap=false preserves discrete params")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Configure two slots with differing Ghost Mode via a sequence of snaps:
    // Slot A: Ghost Mode = 0 (Replace)
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.0f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    // Slot B: Ghost Mode = 1 (Combine)   [mapping: 0/0.5/1.0 → 0/1/2]
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.5f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::A);
    // Now live = slot A's value = 0 (Replace). Good.

    REQUIRE(abSlots.getIncludeDiscreteInSnap() == false);

    // Snap to B. With include-discrete OFF, the live Ghost Mode should
    // remain at slot A's value (0), NOT flip to slot B's (1).
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);

    const int liveGhostMode = (int) proc.apvts.getRawParameterValue(ParamID::GHOST_MODE)->load();
    CHECK(liveGhostMode == 0);
}

TEST_CASE("snapTo with includeDiscreteInSnap=true flips discrete params")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    abSlots.setIncludeDiscreteInSnap(true);

    // Same setup as the previous test.
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.0f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.5f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::A);

    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);

    const int liveGhostMode = (int) proc.apvts.getRawParameterValue(ParamID::GHOST_MODE)->load();
    CHECK(liveGhostMode == 1);
}

TEST_CASE("copy A to B overwrites slot B with slot A contents")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Make slot A and slot B differ.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.3f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.7f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::A);
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.3f);  // live = slot A state
    // Now slot A's stored ghost value is ≠ slot B's. Active = A.

    abSlots.copy(kaigen::phantom::ABSlotManager::Slot::A,
                 kaigen::phantom::ABSlotManager::Slot::B);

    auto slotA = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A);
    auto slotB = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B);
    CHECK(slotA.toXmlString() == slotB.toXmlString());

    // Active did not change.
    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::A);
}

TEST_CASE("modified flag set on APVTS change for active slot only")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    REQUIRE(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::A) == false);
    REQUIRE(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::B) == false);

    // Change a param while on slot A → modified[A] should flip to true.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.4f);

    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::A) == true);
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::B) == false);
}

TEST_CASE("modified flag NOT set during internal snap/copy/load")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // First make slots differ so snapTo actually does something.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.4f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    // snapTo fires replaceState which, in the absence of suppression, would
    // flip modified flags. Verify it did not.
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::B) == false);
}

TEST_CASE("toStateTree / fromStateTree round-trips slots, active, and setting")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager src { proc.apvts };

    // Populate slots with distinct data.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.25f);
    src.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.75f);
    src.syncActiveSlotFromLive();   // make sure slot B is current
    src.setIncludeDiscreteInSnap(true);

    const auto tree = src.toStateTree();

    // New manager in a separate processor, restore.
    TestProcessor proc2;
    kaigen::phantom::ABSlotManager dst { proc2.apvts };
    dst.fromStateTree(tree);

    CHECK(dst.getActive() == kaigen::phantom::ABSlotManager::Slot::B);
    CHECK(dst.getIncludeDiscreteInSnap() == true);
    CHECK(dst.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString()
          == src.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString());
    CHECK(dst.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString()
          == src.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString());
}

TEST_CASE("fromStateTree with invalid/empty tree initializes slots from live APVTS")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    abSlots.fromStateTree(juce::ValueTree{});

    CHECK(abSlots.getActive() == kaigen::phantom::ABSlotManager::Slot::A);
    CHECK(abSlots.getIncludeDiscreteInSnap() == false);
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString()
          == proc.apvts.copyState().toXmlString());
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString()
          == proc.apvts.copyState().toXmlString());
}

TEST_CASE("loadSinglePresetIntoActive writes only the active slot")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Snapshot slot B's initial content so we can verify it's untouched.
    const auto slotBBefore = abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString();

    // Build a preset tree that differs from live state.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.9f);
    auto presetTree = proc.apvts.copyState();
    // Reset live to default so we can see the load took effect.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.0f);

    abSlots.loadSinglePresetIntoActive(presetTree, "Factory/Warm Bass");

    // Slot A should be the preset.
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::A).toXmlString()
          == presetTree.toXmlString());
    // Slot B untouched.
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString()
          == slotBBefore);
    // Live state is now the preset.
    const float liveGhost = proc.apvts.getRawParameterValue(ParamID::GHOST)->load();
    CHECK(liveGhost == Catch::Approx(90.0f).epsilon(0.01));  // 0.9 normalized × 100 range
    // Preset ref set for slot A only, modified cleared.
    CHECK(abSlots.getPresetRef(kaigen::phantom::ABSlotManager::Slot::A) == "Factory/Warm Bass");
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::A) == false);
}

TEST_CASE("loadABPreset populates both slots and sets designerAuthored")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    // Build two distinct state trees for A and B.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.2f);
    const auto stateA = proc.apvts.copyState();
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.8f);
    const auto stateB = proc.apvts.copyState();

    // Compose a preset root: root = stateA, with SlotB child containing stateB.
    auto presetRoot = stateA.createCopy();
    juce::ValueTree slotBChild { "SlotB" };
    slotBChild.appendChild(stateB.createCopy(), nullptr);
    presetRoot.appendChild(slotBChild, nullptr);

    abSlots.loadABPreset(presetRoot, "Factory/Bright vs Dark");

    // Slot A gets the root state, MINUS the SlotB child (stripping covered later).
    // For now, just check slot B equals the SlotB child's payload.
    CHECK(abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString()
          == stateB.toXmlString());
    CHECK(abSlots.getPresetRef(kaigen::phantom::ABSlotManager::Slot::A) == "Factory/Bright vs Dark");
    CHECK(abSlots.getPresetRef(kaigen::phantom::ABSlotManager::Slot::B) == "Factory/Bright vs Dark");
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::A) == false);
    CHECK(abSlots.isModified(kaigen::phantom::ABSlotManager::Slot::B) == false);
}

TEST_CASE("designerAuthored overrides includeDiscreteInSnap=false")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    abSlots.setIncludeDiscreteInSnap(false);

    // Slot A: Ghost Mode = 0 (Replace); Slot B: Ghost Mode = 2 (Phantom Only).
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.0f);
    const auto stateA = proc.apvts.copyState();
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(1.0f);
    const auto stateB = proc.apvts.copyState();

    auto presetRoot = stateA.createCopy();
    juce::ValueTree slotBChild { "SlotB" };
    slotBChild.appendChild(stateB.createCopy(), nullptr);
    presetRoot.appendChild(slotBChild, nullptr);

    abSlots.loadABPreset(presetRoot, "Factory/Test");

    // Active is still A (load doesn't change it). Snap to B: discrete should flip
    // because designer-authored bit overrides the off-by-default user setting.
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    const int liveGhostMode = (int) proc.apvts.getRawParameterValue(ParamID::GHOST_MODE)->load();
    CHECK(liveGhostMode == 2);
}

TEST_CASE("designerAuthored cleared on any parameter edit")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    abSlots.setIncludeDiscreteInSnap(false);

    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(0.0f);
    const auto stateA = proc.apvts.copyState();
    proc.apvts.getParameter(ParamID::GHOST_MODE)->setValueNotifyingHost(1.0f);
    const auto stateB = proc.apvts.copyState();

    auto presetRoot = stateA.createCopy();
    juce::ValueTree slotBChild { "SlotB" };
    slotBChild.appendChild(stateB.createCopy(), nullptr);
    presetRoot.appendChild(slotBChild, nullptr);

    abSlots.loadABPreset(presetRoot, "Factory/Test");

    // User tweaks any param → designerAuthored cleared.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.5f);

    // Now snap to B → discrete stays at slot A's value because the setting is off.
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    const int liveGhostMode = (int) proc.apvts.getRawParameterValue(ParamID::GHOST_MODE)->load();
    CHECK(liveGhostMode == 0);
}

TEST_CASE("buildPresetSlotBChild wraps slot B in a <SlotB> node")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };

    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.33f);
    abSlots.snapTo(kaigen::phantom::ABSlotManager::Slot::B);
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.77f);
    abSlots.syncActiveSlotFromLive();

    const auto child = abSlots.buildPresetSlotBChild();
    REQUIRE(child.isValid());
    CHECK(child.getType().toString() == "SlotB");
    REQUIRE(child.getNumChildren() == 1);
    const auto slotBState = child.getChild(0);
    CHECK(slotBState.toXmlString()
          == abSlots.getSlot(kaigen::phantom::ABSlotManager::Slot::B).toXmlString());
}
