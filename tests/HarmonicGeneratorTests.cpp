#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/HarmonicGenerator.h"
#include "Parameters.h"
#include <vector>
#include <cmath>

TEST_CASE("HarmonicGenerator produces non-zero output for active voice")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);
    gen.setEffectModePitch(40.0f);
    gen.setPhantomStrength(1.0f);

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    gen.process(buf);

    float rms = 0.0f;
    for (int i = 0; i < 512; ++i)
        rms += buf.getSample(0, i) * buf.getSample(0, i);
    rms = std::sqrt(rms / 512.0f);

    REQUIRE(rms > 0.001f);
}

TEST_CASE("HarmonicGenerator silence when phantom strength is 0")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);
    gen.setEffectModePitch(80.0f);
    gen.setPhantomStrength(0.0f);

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    gen.process(buf);

    float peak = 0.0f;
    for (int i = 0; i < 512; ++i)
        peak = std::max(peak, std::abs(buf.getSample(0, i)));

    REQUIRE(peak < 1e-6f);
}

TEST_CASE("HarmonicGenerator Warm preset has H2 louder than H8")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);
    gen.setPreset(RecipePreset::Warm);

    auto amps = gen.getHarmonicAmplitudes();
    REQUIRE(amps[0] > amps[6]);  // H2 > H8
}

TEST_CASE("HarmonicGenerator Hollow preset has odd harmonics louder than even")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);
    gen.setPreset(RecipePreset::Hollow);

    auto amps = gen.getHarmonicAmplitudes();
    // H3 (index 1) > H2 (index 0), H5 (index 3) > H4 (index 2)
    REQUIRE(amps[1] > amps[0]);
    REQUIRE(amps[3] > amps[2]);
}

TEST_CASE("MIDI voice activation adds a voice to the pool")
{
    HarmonicGenerator gen;
    gen.prepare(44100.0, 512);
    gen.setMaxVoices(4);

    gen.noteOn(60, 100);
    REQUIRE(gen.getActiveVoiceCount() == 1);

    gen.noteOn(64, 100);
    REQUIRE(gen.getActiveVoiceCount() == 2);

    gen.noteOff(60);
    REQUIRE(gen.getActiveVoiceCount() == 1);
}
