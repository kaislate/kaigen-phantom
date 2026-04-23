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

TEST_CASE("preProcessBlock handles negative arc depth")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    morph.prepareToPlay(44100.0, 512);
    morph.setEnabled(true);

    // GHOST base 50, arc -0.30 → target = 50 - 30 = 20.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(50.0f));
    morph.setArcDepth(ParamID::GHOST, -0.30f);

    proc.apvts.getParameter(ParamID::MORPH_AMOUNT)->setValueNotifyingHost(1.0f);
    for (int i = 0; i < 200; ++i) morph.preProcessBlock();

    const float live = proc.apvts.getRawParameterValue(ParamID::GHOST)->load();
    CHECK(live == Catch::Approx(20.0f).epsilon(0.02));
}

TEST_CASE("preProcessBlock clamps at parameter min (lower plateau)")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    morph.prepareToPlay(44100.0, 512);
    morph.setEnabled(true);

    // GHOST base 20, arc -0.50 → target math = 20 - 50 = -30, clamped at 0.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(20.0f));
    morph.setArcDepth(ParamID::GHOST, -0.50f);

    proc.apvts.getParameter(ParamID::MORPH_AMOUNT)->setValueNotifyingHost(1.0f);
    for (int i = 0; i < 200; ++i) morph.preProcessBlock();

    const float live = proc.apvts.getRawParameterValue(ParamID::GHOST)->load();
    CHECK(live == Catch::Approx(0.0f).epsilon(0.01));
}

TEST_CASE("beginCapture + knob edits + endCapture(commit) sets arcs from delta")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    morph.prepareToPlay(44100.0, 512);
    morph.setEnabled(true);

    // Start: GHOST = 50, PHANTOM_THRESHOLD = 100.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(50.0f));
    proc.apvts.getParameter(ParamID::PHANTOM_THRESHOLD)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::PHANTOM_THRESHOLD)->convertTo0to1(100.0f));

    morph.beginCapture();
    CHECK(morph.isInCapture() == true);

    // Simulate user dragging knobs: GHOST -> 80, PHANTOM_THRESHOLD -> 50.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(80.0f));
    proc.apvts.getParameter(ParamID::PHANTOM_THRESHOLD)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::PHANTOM_THRESHOLD)->convertTo0to1(50.0f));

    const auto modified = morph.endCapture(true);
    CHECK(morph.isInCapture() == false);

    // GHOST delta 30 in range 100 → arc depth = 0.30.
    CHECK(morph.getArcDepth(ParamID::GHOST) == Catch::Approx(0.30f).epsilon(0.01));

    // PHANTOM_THRESHOLD has a skewed range (20..20000); check just that it was detected as modified.
    CHECK(modified.size() >= 1);
    CHECK(std::find(modified.begin(), modified.end(), juce::String(ParamID::GHOST)) != modified.end());
}

TEST_CASE("beginCapture + knob edits + endCapture(cancel) restores baselines")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };

    morph.prepareToPlay(44100.0, 512);
    morph.setEnabled(true);

    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(50.0f));

    morph.beginCapture();

    // "User" changes GHOST to 80.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(80.0f));

    const auto modified = morph.endCapture(false);   // cancel

    CHECK(modified.empty());                         // nothing committed
    CHECK(morph.hasNonZeroArc(ParamID::GHOST) == false);

    // Live GHOST should be back at 50.
    const float live = proc.apvts.getRawParameterValue(ParamID::GHOST)->load();
    CHECK(live == Catch::Approx(50.0f).epsilon(0.01));
}

TEST_CASE("toMorphConfigTree / fromMorphConfigTree round-trips arc data")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots1 { proc.apvts };
    PhantomEngine engine1;
    kaigen::phantom::MorphEngine src { proc.apvts, abSlots1, engine1 };

    // Arm a few arcs.
    proc.apvts.getParameter(ParamID::GHOST)->setValueNotifyingHost(
        proc.apvts.getParameter(ParamID::GHOST)->convertTo0to1(50.0f));
    src.setArcDepth(ParamID::GHOST, 0.35f);
    src.setArcDepth(ParamID::RECIPE_H2, -0.20f);

    const auto tree = src.toMorphConfigTree();
    CHECK(tree.getType().toString() == "MorphConfig");

    // Restore in a fresh processor.
    TestProcessor proc2;
    kaigen::phantom::ABSlotManager abSlots2 { proc2.apvts };
    PhantomEngine engine2;
    kaigen::phantom::MorphEngine dst { proc2.apvts, abSlots2, engine2 };

    dst.fromMorphConfigTree(tree);
    CHECK(dst.getArcDepth(ParamID::GHOST)     == Catch::Approx(0.35f));
    CHECK(dst.getArcDepth(ParamID::RECIPE_H2) == Catch::Approx(-0.20f));
    CHECK(dst.armedKnobCount() == 2);
}

TEST_CASE("toStateTree / fromStateTree round-trips full live state")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots1 { proc.apvts };
    PhantomEngine engine1;
    kaigen::phantom::MorphEngine src { proc.apvts, abSlots1, engine1 };
    src.prepareToPlay(44100.0, 512);

    src.setEnabled(true);
    src.setArcDepth(ParamID::GHOST, 0.35f);
    proc.apvts.getParameter(ParamID::MORPH_AMOUNT)->setValueNotifyingHost(0.60f);
    for (int i = 0; i < 100; ++i) src.preProcessBlock();  // let smoothing settle

    const auto tree = src.toStateTree();
    CHECK(tree.getType().toString() == "MorphState");

    TestProcessor proc2;
    kaigen::phantom::ABSlotManager abSlots2 { proc2.apvts };
    PhantomEngine engine2;
    kaigen::phantom::MorphEngine dst { proc2.apvts, abSlots2, engine2 };
    dst.prepareToPlay(44100.0, 512);
    dst.fromStateTree(tree);

    CHECK(dst.isEnabled() == true);
    CHECK(dst.getArcDepth(ParamID::GHOST) == Catch::Approx(0.35f));
    CHECK(dst.getMorphAmount() == Catch::Approx(0.60f).epsilon(0.02));
}

TEST_CASE("setSceneCrossfadeEnabled(true) lazily constructs secondary engine")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };
    morph.prepareToPlay(44100.0, 512);

    REQUIRE(morph.isSceneCrossfadeEnabled() == false);

    morph.setSceneCrossfadeEnabled(true);
    CHECK(morph.isSceneCrossfadeEnabled() == true);

    // After enabling, calling postProcessBlock (even on a dry buffer) should
    // not crash — implies secondary engine exists and is ready.
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    morph.postProcessBlock(buf, nullptr);
    // No assertions here beyond "didn't crash".
    CHECK(true);
}

TEST_CASE("postProcessBlock with scene disabled is a no-op")
{
    TestProcessor proc;
    kaigen::phantom::ABSlotManager abSlots { proc.apvts };
    PhantomEngine engine;
    kaigen::phantom::MorphEngine morph { proc.apvts, abSlots, engine };
    morph.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> buf(2, 512);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            buf.setSample(ch, i, 0.5f);

    morph.setSceneCrossfadeEnabled(false);
    morph.postProcessBlock(buf, nullptr);

    CHECK(buf.getSample(0, 0) == Catch::Approx(0.5f));
    CHECK(buf.getSample(0, 511) == Catch::Approx(0.5f));
}

#endif // KAIGEN_PRO_BUILD
