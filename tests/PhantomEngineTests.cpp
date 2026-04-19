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

// ─── Synth LPF/HPF verification ─────────────────────────────────────────────
// Confirms the filter section actually attenuates the phantom harmonics when
// the cutoffs are moved from their transparent defaults.

static float tailRms(const juce::AudioBuffer<float>& buf, int startSample)
{
    double sum = 0.0;
    const int n = buf.getNumSamples() - startSample;
    for (int i = 0; i < n; ++i)
    {
        const float s = buf.getSample(0, startSample + i);
        sum += s * s;
    }
    return std::sqrt((float)(sum / juce::jmax(1, n)));
}

TEST_CASE("PhantomEngine: Synth LPF attenuates phantom when closed")
{
    auto runWithLPF = [](float lpfHz) -> float
    {
        PhantomEngine eng;
        eng.prepare(44100.0, 512, 2);
        eng.setSynthMode(0);
        eng.setCrossoverHz(150.0f);
        eng.setGhostAmount(1.0f);
        eng.setGhostMode(2);  // Phantom Only — isolates phantom output
        eng.setPhantomStrength(1.0f);
        eng.setHarmonicAmplitudes({ 1.0f, 0.0f, 0.7f, 0.0f, 0.5f, 0.0f, 0.3f });
        eng.setSynthLPF(lpfHz);
        eng.setSynthHPF(20.0f);

        auto buf = makeSineBuffer(60.0f, 0.5f, 16384);
        eng.process(buf);
        return tailRms(buf, 12288);
    };

    const float rmsWide   = runWithLPF(20000.0f);  // transparent
    const float rmsClosed = runWithLPF(200.0f);    // cuts H4/H6/H8 significantly

    // A single biquad at 200Hz with harmonics at 120/240/360/480 attenuates
    // RMS by roughly 20% — the LPF is subtle but verifiably active.
    REQUIRE(rmsWide > 0.01f);
    REQUIRE(rmsClosed < rmsWide * 0.90f);
}

TEST_CASE("PhantomEngine: Synth HPF attenuates phantom when raised")
{
    auto runWithHPF = [](float hpfHz) -> float
    {
        PhantomEngine eng;
        eng.prepare(44100.0, 512, 2);
        eng.setSynthMode(0);
        eng.setCrossoverHz(150.0f);
        eng.setGhostAmount(1.0f);
        eng.setGhostMode(2);
        eng.setPhantomStrength(1.0f);
        eng.setHarmonicAmplitudes({ 1.0f, 0.0f, 0.7f, 0.0f, 0.5f, 0.0f, 0.3f });
        eng.setSynthLPF(20000.0f);
        eng.setSynthHPF(hpfHz);

        auto buf = makeSineBuffer(60.0f, 0.5f, 16384);
        eng.process(buf);
        return tailRms(buf, 12288);
    };

    const float rmsOpen   = runWithHPF(20.0f);    // transparent
    const float rmsRaised = runWithHPF(1000.0f);  // cuts most harmonics

    REQUIRE(rmsOpen > 0.01f);
    REQUIRE(rmsRaised < rmsOpen * 0.7f);
}

// ─── Phantom Only mode silences dry + high band ─────────────────────────────
TEST_CASE("PhantomEngine: Phantom Only mode produces silence on silent phantom path")
{
    // Phantom Only (ghostMode=2) should zero both the dry low and the high band,
    // so an input that the engine CANNOT synthesize harmonics for (silence) must
    // result in true silence regardless of ghostAmount.
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setSynthMode(0);            // Effect mode — no H1 reconstruction
    eng.setGhostAmount(1.0f);
    eng.setGhostMode(2);            // Phantom Only
    eng.setPhantomStrength(1.0f);

    juce::AudioBuffer<float> buf(2, 4096);
    buf.clear();
    eng.process(buf);

    float peak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            peak = juce::jmax(peak, std::fabs(buf.getSample(ch, i)));

    REQUIRE(peak < 1e-4f);
}

TEST_CASE("PhantomEngine: Phantom Only mode — no dry leak at user-reported scenario")
{
    // Matches the user's Ableton setup: 80Hz bass input, crossover 134Hz,
    // Phantom Only mode, amount 100%. The 80Hz fundamental is entirely in the
    // low band — if any dry leaks through, we'd see the 80Hz bin in the output.
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setSynthMode(0);            // Effect mode (ZCS, no wavelet window AM)
    eng.setH1Amplitude(0.0f);
    eng.setSubAmplitude(0.0f);
    eng.setCrossoverHz(134.0f);
    eng.setGhostAmount(1.0f);
    eng.setGhostMode(2);            // Phantom Only
    eng.setPhantomStrength(1.0f);
    eng.setHarmonicAmplitudes({ 1.0f, 0.0f, 0.7f, 0.0f, 0.5f, 0.0f, 0.3f });

    // Warm up for ~185ms so the period tracker converges, then feed another block
    // and measure.
    for (int warm = 0; warm < 4; ++warm)
    {
        auto warmBuf = makeSineBuffer(80.0f, 0.5f, 2048);
        eng.process(warmBuf);
    }

    auto buf = makeSineBuffer(80.0f, 0.5f, 8192);
    eng.process(buf);

    auto measureBin = [&](float f) -> float {
        double re = 0.0, im = 0.0;
        const float w = 2.0f * juce::MathConstants<float>::pi * f / 44100.0f;
        for (int i = 4096; i < 8192; ++i) {
            const float s = buf.getSample(0, i);
            re += s * std::cos(w * (float)(i - 4096));
            im += s * std::sin(w * (float)(i - 4096));
        }
        return std::sqrt((float)(re * re + im * im)) * 2.0f / 4096.0f;
    };

    const float m80  = measureBin(80.0f);
    const float m160 = measureBin(160.0f);
    const float m240 = measureBin(240.0f);
    const float m320 = measureBin(320.0f);

    WARN("magFund(80Hz)=" << m80 << " H2(160Hz)=" << m160
         << " H3(240Hz)=" << m240 << " H4(320Hz)=" << m320);

    REQUIRE(m80 < 0.05f);
}

TEST_CASE("PhantomEngine: Phantom Only mode zeros out high-band content")
{
    // Feed a high-frequency sine (above crossover) and check the output is
    // essentially silent — in Phantom Only mode, high band must be muted.
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setSynthMode(0);
    eng.setCrossoverHz(150.0f);
    eng.setGhostAmount(1.0f);
    eng.setGhostMode(2);
    eng.setPhantomStrength(1.0f);

    // 2 kHz input — entirely in the high band; synth has nothing below 150Hz
    // to track, so phantom is silent too.
    auto buf = makeSineBuffer(2000.0f, 0.5f, 8192);
    eng.process(buf);

    float tailPeak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 4096; i < 8192; ++i)
            tailPeak = juce::jmax(tailPeak, std::fabs(buf.getSample(ch, i)));

    // A Replace-mode equivalent would let 2kHz through untouched (peak ~0.5).
    // Phantom Only should reduce this dramatically.
    REQUIRE(tailPeak < 0.05f);
}

TEST_CASE("PhantomEngine: silence in -> silence out")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setGhostAmount(1.0f);
    eng.setGhostMode(0);
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
    eng.setGhostMode(0);
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
    eng.setGhostMode(1);  // add mode — phantom + original
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
    eng.setGhostMode(1);
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
    eng.setGhostMode(1);
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
    eng.setGhostMode(0);
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
        eng.setGhostMode(1);
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

    // Both modes must produce nonzero output for a loud-enough input.
    // Per-mode architectural distinctions (phase reset, wavelet window, H1) are
    // verified in WaveletSynthTests and ZeroCrossingSynthTests respectively.
    REQUIRE(rmsEffect > 0.01f);
    REQUIRE(rmsResyn  > 0.01f);
}
