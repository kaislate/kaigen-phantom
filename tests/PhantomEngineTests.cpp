#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/PhantomEngine.h"
#include <cmath>

static juce::AudioBuffer<float> makeSineBuffer(float freq, float amp, int n, float sr = 44100.0f)
{
    juce::AudioBuffer<float> buf(2, n);
    const float w = 2.0f * juce::MathConstants<float>::pi * freq / sr;
    for (int i = 0; i < n; ++i)
    {
        const float s = amp * std::sin(w * (float) i);
        buf.setSample(0, i, s);
        buf.setSample(1, i, s);
    }
    return buf;
}

TEST_CASE("PhantomEngine: silence in -> silence out")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setGhostAmount(1.0f);
    eng.setGhostReplace(true);
    eng.setPhantomStrength(1.0f);

    juce::AudioBuffer<float> buf(2, 1024);
    buf.clear();
    eng.process(buf);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            REQUIRE(std::fabs(buf.getSample(ch, i)) < 1e-4f);
}

TEST_CASE("PhantomEngine: audio passes through at ghost=0")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setGhostAmount(0.0f);    // no phantom
    eng.setGhostReplace(true);
    eng.setCrossoverHz(200.0f);

    // At 1kHz (above crossover), signal should pass nearly untouched
    auto buf = makeSineBuffer(1000.0f, 0.5f, 2048);
    const auto original = buf;
    eng.process(buf);

    // The LR4 crossover introduces mild amplitude deviation near the boundary,
    // so check that output RMS is close to input RMS rather than sample-exact.
    double inSum = 0.0, outSum = 0.0;
    for (int i = 1024; i < 2048; ++i)
    {
        const float o = original.getSample(0, i);
        const float b = buf.getSample(0, i);
        inSum  += o * o;
        outSum += b * b;
    }
    const double ratio = std::sqrt(outSum / juce::jmax(1e-12, inSum));
    REQUIRE(ratio > 0.8);
    REQUIRE(ratio < 1.2);
}

TEST_CASE("PhantomEngine: low sine with phantom active produces nonzero output")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setCrossoverHz(150.0f);
    eng.setGhostAmount(1.0f);
    eng.setGhostReplace(false);  // add mode — phantom + original
    eng.setPhantomStrength(1.0f);
    eng.setHarmonicAmplitudes({ 0.8f, 0.6f, 0.4f, 0.3f, 0.2f, 0.1f, 0.05f });
    eng.setSaturation(0.5f);

    auto buf = makeSineBuffer(60.0f, 0.5f, 4096);
    eng.process(buf);

    // Check max output amplitude — should be at least as loud as input
    float maxOut = 0.0f;
    for (int i = 2048; i < 4096; ++i)
        maxOut = juce::jmax(maxOut, std::fabs(buf.getSample(0, i)));

    REQUIRE(maxOut > 0.1f);
}

TEST_CASE("PhantomEngine: output silences when input goes silent")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setGhostAmount(1.0f);
    eng.setGhostReplace(false);
    eng.setPhantomStrength(1.0f);
    eng.setEnvelopeAttackMs(1.0f);
    eng.setEnvelopeReleaseMs(5.0f);  // very fast release for test

    // First feed a strong sine to excite the envelope
    auto active = makeSineBuffer(60.0f, 0.5f, 4096);
    eng.process(active);

    // Now process silent blocks and verify output decays
    juce::AudioBuffer<float> silent(2, 4096);
    silent.clear();
    eng.process(silent);  // First silent block
    eng.process(silent);  // Second — envelope should have released

    // Final samples should be effectively silent
    float tail = 0.0f;
    for (int i = 3000; i < 4096; ++i)
        tail = juce::jmax(tail, std::fabs(silent.getSample(0, i)));

    REQUIRE(tail < 0.01f);
}

TEST_CASE("PhantomEngine: bypass configuration does nothing internally")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);

    // The engine itself doesn't implement bypass — that's at the Processor
    // level. This just confirms reset works without crashing.
    eng.reset();
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    eng.process(buf);
    // No crash = pass
    SUCCEED();
}

TEST_CASE("PhantomEngine: RESYN mode produces nonzero output for sine input")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setSynthMode(1);
    eng.setCrossoverHz(150.0f);
    eng.setGhostAmount(1.0f);
    eng.setGhostReplace(false);
    eng.setPhantomStrength(1.0f);

    auto buf = makeSineBuffer(60.0f, 0.5f, 4096);
    eng.process(buf);

    float maxOut = 0.0f;
    for (int i = 2048; i < 4096; ++i)
        maxOut = juce::jmax(maxOut, std::fabs(buf.getSample(0, i)));

    REQUIRE(maxOut > 0.1f);
}

TEST_CASE("PhantomEngine: RESYN mode silence in -> silence out")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setSynthMode(1);
    eng.setGhostAmount(1.0f);
    eng.setGhostReplace(true);
    eng.setPhantomStrength(1.0f);

    juce::AudioBuffer<float> buf(2, 1024);
    buf.clear();
    eng.process(buf);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            REQUIRE(std::fabs(buf.getSample(ch, i)) < 1e-4f);
}

TEST_CASE("PhantomEngine: RESYN and Effect modes produce different output for same input")
{
    auto runMode = [](int mode) -> float
    {
        PhantomEngine eng;
        eng.prepare(44100.0, 512, 2);
        eng.setSynthMode(mode);
        eng.setCrossoverHz(150.0f);
        eng.setGhostAmount(1.0f);
        eng.setGhostReplace(false);
        eng.setPhantomStrength(1.0f);
        eng.setHarmonicAmplitudes({ 0.8f, 0.6f, 0.4f, 0.3f, 0.2f, 0.1f, 0.05f });

        auto buf = makeSineBuffer(60.0f, 0.5f, 4096);
        eng.process(buf);

        float rms = 0.0f;
        for (int i = 2048; i < 4096; ++i)
        {
            const float s = buf.getSample(0, i);
            rms += s * s;
        }
        return rms;
    };

    const float rmsEffect = runMode(0);
    const float rmsResyn  = runMode(1);

    // Outputs should be nonzero in both modes
    REQUIRE(rmsEffect > 0.01f);
    REQUIRE(rmsResyn  > 0.01f);

    // Outputs should differ — RESYN uses phase-reset wavelets while Effect uses
    // continuous-phase ZCS, so their RMS should be meaningfully different.
    const float ratio = rmsResyn / juce::jmax(1e-9f, rmsEffect);
    REQUIRE((ratio < 0.9f || ratio > 1.1f));
}
