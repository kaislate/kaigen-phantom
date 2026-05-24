#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../Source/DSP/PhantomSampler.h"

using namespace kaigen::phantom;
using Catch::Approx;

namespace
{
    // Build a 1-second sine at 440 Hz, 44.1k mono, as a known-shape
    // sample for playback tests.
    juce::AudioBuffer<float> makeSine440(int lengthSamples = 44100)
    {
        juce::AudioBuffer<float> b(1, lengthSamples);
        auto* w = b.getWritePointer(0);
        const double twoPi = juce::MathConstants<double>::twoPi;
        for (int i = 0; i < lengthSamples; ++i)
            w[i] = (float) std::sin(twoPi * 440.0 * (double) i / 44100.0);
        return b;
    }
}

TEST_CASE("PhantomSampler: empty sampler renders silence", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> out(2, 512);
    out.clear();
    juce::MidiBuffer midi;
    s.renderNextBlock(out, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            REQUIRE(out.getSample(ch, i) == Approx(0.0f));
}

TEST_CASE("PhantomSampler: voice allocation caps at 8", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    REQUIRE(s.loadSample(makeSine440(), 44100.0));

    // Send 10 note-ons via the synthesiser's expected midi path.
    juce::MidiBuffer midi;
    for (int n = 0; n < 10; ++n)
        midi.addEvent(juce::MidiMessage::noteOn(1, 60 + n, (juce::uint8) 100), n * 4);

    juce::AudioBuffer<float> out(2, 64);
    out.clear();
    s.renderNextBlock(out, midi);

    CHECK(s.getActiveVoiceCount() == 8);
}

TEST_CASE("PhantomSampler: pitch ratio doubles for octave-up note", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    s.setRootNote(60);
    REQUIRE(s.loadSample(makeSine440(), 44100.0));

    // Note 72 (one octave above root 60) should consume source samples
    // at 2x rate. After N output samples we should have advanced ~2*N
    // through the source.
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8) 100), 0);

    juce::AudioBuffer<float> out(1, 1000);
    out.clear();
    s.renderNextBlock(out, midi);

    // Playhead should be near 2000 (2*1000), give or take a few from
    // the linear-interp + ADSR onset.
    const int playhead = s.getPlayheadPosition();
    CHECK(playhead > 1900);
    CHECK(playhead < 2100);
}

TEST_CASE("PhantomSampler: loop wraps playhead back to 0", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    s.setRootNote(60);
    s.setLoopEnabled(true);
    s.setEnvelope(0.001f, 0.001f, 1.0f, 0.001f);   // mostly bypass envelope

    // Short 100-sample sample so it loops fast.
    auto buf = juce::AudioBuffer<float>(1, 100);
    buf.clear();
    for (int i = 0; i < 100; ++i) buf.setSample(0, i, (i < 50) ? 0.5f : -0.5f);
    REQUIRE(s.loadSample(std::move(buf), 44100.0));

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 127), 0);

    juce::AudioBuffer<float> out(1, 1000);
    out.clear();
    s.renderNextBlock(out, midi);

    // After 1000 samples on a 100-sample loop, position should still
    // be within bounds (wrap worked at least once).
    const int playhead = s.getPlayheadPosition();
    REQUIRE(playhead >= 0);
    REQUIRE(playhead < 100);
}

TEST_CASE("PhantomSampler: note-off → ADSR release silences output", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    s.setRootNote(60);
    s.setEnvelope(0.001f, 0.001f, 1.0f, 0.010f);   // 10ms release
    REQUIRE(s.loadSample(makeSine440(), 44100.0));

    // Hold note for 100 samples, then release.
    juce::MidiBuffer noteOn;
    noteOn.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 127), 0);
    juce::AudioBuffer<float> hold(1, 100);
    hold.clear();
    s.renderNextBlock(hold, noteOn);
    CHECK(s.getActiveVoiceCount() == 1);

    // Release the note, then render long enough to fully decay
    // (10ms release at 44.1k = 441 samples). Render 2000 to be safe.
    juce::MidiBuffer noteOff;
    noteOff.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    juce::AudioBuffer<float> tail(1, 2000);
    tail.clear();
    s.renderNextBlock(tail, noteOff);

    CHECK(s.getActiveVoiceCount() == 0);

    // Last 500 samples should be silent.
    for (int i = 1500; i < 2000; ++i)
        CHECK(std::abs(tail.getSample(0, i)) < 0.001f);
}

TEST_CASE("PhantomSampler: setGainDb persists across note-ons (no progressive decay)", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    s.setRootNote(60);
    s.setLoopEnabled(true);
    s.setGainDb(0.0f);   // unity
    s.setEnvelope(0.001f, 0.001f, 1.0f, 0.001f);
    REQUIRE(s.loadSample(makeSine440(), 44100.0));

    auto peakOf = [&]() -> float
    {
        // Fire note-on, render, then note-off so the voice releases
        // for the next iteration to reuse the same voice slot.
        juce::MidiBuffer onBuf;
        onBuf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 127), 0);
        juce::AudioBuffer<float> out(1, 512);
        out.clear();
        s.renderNextBlock(out, onBuf);

        float peak = 0.0f;
        for (int i = 0; i < 512; ++i)
            peak = juce::jmax(peak, std::abs(out.getSample(0, i)));

        juce::MidiBuffer offBuf;
        offBuf.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        juce::AudioBuffer<float> tail(1, 256);
        tail.clear();
        s.renderNextBlock(tail, offBuf);

        return peak;
    };

    const float p1 = peakOf();
    const float p2 = peakOf();
    const float p3 = peakOf();

    REQUIRE(p1 > 0.5f);             // some actual audio
    CHECK(std::abs(p1 - p2) < 0.01f);   // second note ~= first
    CHECK(std::abs(p1 - p3) < 0.01f);   // third note ~= first
}

TEST_CASE("PhantomSampler: non-loop overrun does not crash and ends silent", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    s.setRootNote(60);
    s.setLoopEnabled(false);                              // <-- key: no loop
    s.setEnvelope(0.001f, 0.001f, 1.0f, 0.001f);          // quick release

    // 200-sample buffer played at note 60 (rate 1.0). 4000 output
    // samples means we run 3800 samples past the end of the source.
    auto buf = juce::AudioBuffer<float>(1, 200);
    for (int i = 0; i < 200; ++i) buf.setSample(0, i, 0.5f);
    REQUIRE(s.loadSample(std::move(buf), 44100.0));

    juce::MidiBuffer onBuf;
    onBuf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 127), 0);
    juce::AudioBuffer<float> out(1, 4000);
    out.clear();
    REQUIRE_NOTHROW(s.renderNextBlock(out, onBuf));

    // Last 1000 samples should be silent (well past release end).
    for (int i = 3000; i < 4000; ++i)
        CHECK(std::abs(out.getSample(0, i)) < 0.001f);
}

TEST_CASE("PhantomSampler: high pitch ratio on short loop wraps cleanly", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    s.setRootNote(60);
    s.setLoopEnabled(true);
    s.setEnvelope(0.001f, 0.001f, 1.0f, 0.001f);

    // 20-sample loop, note 5 octaves above root => pitch ratio ~32.
    // Each output sample advances position by ~32; single-subtract
    // wrap would leave position above srcLen.
    auto buf = juce::AudioBuffer<float>(1, 20);
    for (int i = 0; i < 20; ++i) buf.setSample(0, i, 0.5f);
    REQUIRE(s.loadSample(std::move(buf), 44100.0));

    juce::MidiBuffer onBuf;
    onBuf.addEvent(juce::MidiMessage::noteOn(1, 120, (juce::uint8) 127), 0);
    juce::AudioBuffer<float> out(1, 1000);
    out.clear();
    REQUIRE_NOTHROW(s.renderNextBlock(out, onBuf));

    const int playhead = s.getPlayheadPosition();
    REQUIRE(playhead >= 0);
    REQUIRE(playhead < 20);   // wrapped within bounds
}

TEST_CASE("PhantomSampler: load via loadSample then read back via hasSample", "[sampler]")
{
    // Verifies that loadSample-then-hasSample increments correctly. The
    // round-trip-via-state test would require constructing a full
    // PhantomProcessor instance and exercising get/setState, which is
    // heavy. The state-side persistence is covered by a manual test
    // in Task 8's walkthrough (load preset -> reopen project).
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    REQUIRE_FALSE(s.hasSample());
    REQUIRE(s.loadSample(makeSine440(), 44100.0));
    REQUIRE(s.hasSample());
    s.clearSample();
    REQUIRE_FALSE(s.hasSample());
}
