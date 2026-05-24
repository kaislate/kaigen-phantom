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
