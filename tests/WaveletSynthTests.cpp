#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/WaveletSynth.h"
#include <cmath>

TEST_CASE("WaveletSynth: setters and prepare/reset do not crash")
{
    WaveletSynth syn;
    syn.prepare(44100.0);
    syn.setWaveletLength(0.5f);
    syn.setGateThreshold(0.3f);
    syn.reset();
    syn.setWaveletLength(1.0f);
    syn.setGateThreshold(0.0f);
    SUCCEED();
}

TEST_CASE("WaveletSynth: default params produce same output as explicit defaults")
{
    // Explicit length=1.0 / gate=0.0 must be identical to factory default
    WaveletSynth a, b;
    a.prepare(44100.0);
    b.prepare(44100.0);
    b.setWaveletLength(1.0f);
    b.setGateThreshold(0.0f);

    const float w = 2.0f * juce::MathConstants<float>::pi * 200.0f / 44100.0f;
    for (int i = 0; i < 2048; ++i)
    {
        const float x = std::sin(w * (float)i);
        REQUIRE(a.process(x) == Catch::Approx(b.process(x)).margin(1e-5f));
    }
}

TEST_CASE("WaveletSynth: gate threshold blocks low-amplitude crossings")
{
    // Low-amplitude signal (0.2) should NOT update period when gate=0.5
    // because the negative swing (-0.2) never reaches -threshold (-0.5).
    // The same signal DOES update period when gate=0.0.
    WaveletSynth gated, ungated;
    gated.prepare(44100.0);
    ungated.prepare(44100.0);
    gated.setGateThreshold(0.5f);
    ungated.setGateThreshold(0.0f);

    const float freq = 200.0f;
    const float amp  = 0.2f;
    const float w    = 2.0f * juce::MathConstants<float>::pi * freq / 44100.0f;

    for (int i = 0; i < 8820; ++i)
    {
        const float x = amp * std::sin(w * (float)i);
        gated.process(x);
        ungated.process(x);
    }

    // Ungated: should have converged toward 200 Hz
    REQUIRE(ungated.getEstimatedHz() == Catch::Approx(200.0f).margin(15.0f));
    // Gated: period estimate should stay near default (100 Hz — no valid crossings)
    REQUIRE(gated.getEstimatedHz() == Catch::Approx(100.0f).margin(20.0f));
}

TEST_CASE("WaveletSynth: reduced length reduces output RMS")
{
    // length=0.1 should produce significantly less energy than length=1.0
    auto measureRMS = [](float length, float freq) -> float {
        WaveletSynth syn;
        syn.prepare(44100.0);
        syn.setWaveletLength(length);
        const float w = 2.0f * juce::MathConstants<float>::pi * freq / 44100.0f;
        // Warm-up: let period converge
        for (int i = 0; i < 4410; ++i)
            syn.process(std::sin(w * (float)i));
        // Measure RMS over 8820 samples (200 ms)
        float sum = 0.0f;
        for (int i = 0; i < 8820; ++i)
        {
            const float y = syn.process(std::sin(w * (float)(i + 4410)));
            sum += y * y;
        }
        return std::sqrt(sum / 8820.0f);
    };

    const float rms100 = measureRMS(1.0f, 200.0f);
    const float rms10  = measureRMS(0.1f, 200.0f);

    REQUIRE(rms100 > 0.01f);               // sanity: synth produced output
    REQUIRE(rms10 < rms100 * 0.5f);        // 10% length → < half the energy
}
