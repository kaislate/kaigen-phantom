#if KAIGEN_PRO_BUILD

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "MorphEngine.h"
#include "ABSlotManager.h"
#include "Engines/PhantomEngine.h"
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

TEST_CASE("MorphEngine compiles and constructs")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    CHECK(morph.isEnabled() == false);
    CHECK(morph.armedKnobCount() == 0);
    CHECK(morph.isSceneCrossfadeEnabled() == false);
}

TEST_CASE("MorphEngine::getContinuousParamIDs excludes bool, choice, and morph params")
{
    TestProcessor proc;
    const auto ids = kaigen::phantom::MorphEngine::getContinuousParamIDs(proc.apvts);

    // Should include common continuous params.
    CHECK(std::find(ids.begin(), ids.end(), juce::String(ParamID::GHOST))            != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), juce::String(ParamID::PHANTOM_THRESHOLD)) != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), juce::String(ParamID::RECIPE_H2))        != ids.end());

    // Should exclude enums and bools.
    CHECK(std::find(ids.begin(), ids.end(), juce::String(ParamID::MODE))       == ids.end());
    CHECK(std::find(ids.begin(), ids.end(), juce::String(ParamID::BYPASS))     == ids.end());
    CHECK(std::find(ids.begin(), ids.end(), juce::String(ParamID::GHOST_MODE)) == ids.end());
    CHECK(std::find(ids.begin(), ids.end(), juce::String(ParamID::BINAURAL_MODE)) == ids.end());

    // Should exclude morph params themselves.
    CHECK(std::find(ids.begin(), ids.end(), juce::String(ParamID::MORPH_AMOUNT))   == ids.end());
    CHECK(std::find(ids.begin(), ids.end(), juce::String(ParamID::SCENE_POSITION)) == ids.end());
}

TEST_CASE("setArcDepth stores value and captures base")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    // Set GHOST to a non-default value; setArcDepth should capture it as base.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(0.42f);
    morph.setArcDepth(ParamID::GHOST, 0.35f);

    CHECK(morph.getArcDepth(ParamID::GHOST) == Catch::Approx(0.35f));
    CHECK(morph.hasNonZeroArc(ParamID::GHOST) == true);
    CHECK(morph.armedKnobCount() == 1);

    // Params without arcs return 0 and false.
    CHECK(morph.getArcDepth(ParamID::PHANTOM_THRESHOLD) == 0.0f);
    CHECK(morph.hasNonZeroArc(ParamID::PHANTOM_THRESHOLD) == false);
}

TEST_CASE("setArcDepth with 0.0 removes the entry (un-arms)")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    morph.setArcDepth(ParamID::GHOST, 0.50f);
    REQUIRE(morph.armedKnobCount() == 1);

    morph.setArcDepth(ParamID::GHOST, 0.0f);
    CHECK(morph.hasNonZeroArc(ParamID::GHOST) == false);
    CHECK(morph.armedKnobCount() == 0);
}

TEST_CASE("armedKnobCount counts multiple independent arcs")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    morph.setArcDepth(ParamID::GHOST, 0.20f);
    morph.setArcDepth(ParamID::PHANTOM_THRESHOLD, -0.50f);
    morph.setArcDepth(ParamID::RECIPE_H2, 0.80f);

    CHECK(morph.armedKnobCount() == 3);
    const auto armed = morph.getArmedParamIDs();
    CHECK(armed.size() == 3);
}

TEST_CASE("setEnabled toggles the enabled flag")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    REQUIRE(morph.isEnabled() == false);
    morph.setEnabled(true);
    CHECK(morph.isEnabled() == true);
    morph.setEnabled(false);
    CHECK(morph.isEnabled() == false);
}

TEST_CASE("setSceneCrossfadeEnabled toggles the scene flag")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    REQUIRE(morph.isSceneCrossfadeEnabled() == false);
    morph.setSceneCrossfadeEnabled(true);
    CHECK(morph.isSceneCrossfadeEnabled() == true);
    morph.setSceneCrossfadeEnabled(false);
    CHECK(morph.isSceneCrossfadeEnabled() == false);
}

TEST_CASE("MorphEngine responds to APVTS MORPH_AMOUNT changes")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    // Initially, raw morph is 0.
    CHECK(morph.getMorphAmount() == Catch::Approx(0.0f));

    // Set MORPH_AMOUNT via APVTS.
    proc.apvts.getParameter(ParamID::MORPH_AMOUNT)->setValueNotifyingHost(0.7f);

    // The listener should have bumped rawMorph. Smoothed catches up slowly —
    // this test only checks that smoothing converges after enough blocks.
    morph.prepareToPlay(44100.0, 512);
    for (int i = 0; i < 200; ++i) morph.preProcessBlock();   // ~2.3 seconds of audio
    CHECK(morph.getMorphAmount() == Catch::Approx(0.7f).epsilon(0.02));
}

TEST_CASE("preProcessBlock applies arc interpolation at morph > 0")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    morph.prepareToPlay(44100.0, 512);
    morph.setEnabled(true);

    // GHOST range: 0..100 (%); default 100.
    // Set base to 50, then arm arc of +0.40 (= +40 in range).
    // At morph = 1.0, live should be 50 + 0.40 * 100 = 90.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(50.0f));
    morph.setArcDepth(ParamID::GHOST, 0.40f);

    // Slam MORPH_AMOUNT to 1 and let smoothing converge.
    proc.apvts.getParameter(ParamID::MORPH_AMOUNT)->setValueNotifyingHost(1.0f);
    for (int i = 0; i < 200; ++i) morph.preProcessBlock();

    const float live = proc.apvts.getRawParameterValue(ParamID::GHOST)->load();
    CHECK(live == Catch::Approx(90.0f).epsilon(0.02));
}

TEST_CASE("preProcessBlock does nothing when morph disabled")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    morph.prepareToPlay(44100.0, 512);
    morph.setEnabled(false);  // explicitly disabled

    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(50.0f));
    morph.setArcDepth(ParamID::GHOST, 0.40f);
    proc.apvts.getParameter(ParamID::MORPH_AMOUNT)->setValueNotifyingHost(1.0f);

    for (int i = 0; i < 200; ++i) morph.preProcessBlock();

    // Live should be unchanged (still 50).
    const float live = proc.apvts.getRawParameterValue(ParamID::GHOST)->load();
    CHECK(live == Catch::Approx(50.0f).epsilon(0.01));
}

TEST_CASE("preProcessBlock clamps at parameter max (plateau behavior)")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    morph.prepareToPlay(44100.0, 512);
    morph.setEnabled(true);

    // GHOST base 80, arc +0.50 -> target math = 80 + 0.50 * 100 = 130.
    // Clamped at max 100.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(80.0f));
    morph.setArcDepth(ParamID::GHOST, 0.50f);

    proc.apvts.getParameter(ParamID::MORPH_AMOUNT)->setValueNotifyingHost(1.0f);
    for (int i = 0; i < 200; ++i) morph.preProcessBlock();

    const float live = proc.apvts.getRawParameterValue(ParamID::GHOST)->load();
    CHECK(live == Catch::Approx(100.0f).epsilon(0.01));   // clamped at max, NOT 130
}

#endif // KAIGEN_PRO_BUILD
