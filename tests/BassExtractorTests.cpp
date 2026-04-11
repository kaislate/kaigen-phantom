#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/BassExtractor.h"
#include <cmath>

static void fillSine(std::vector<float>& buf, float freq, float sampleRate, float amplitude = 1.0f)
{
    const float w = 2.0f * juce::MathConstants<float>::pi * freq / sampleRate;
    for (size_t i = 0; i < buf.size(); ++i)
        buf[i] = amplitude * std::sin(w * (float) i);
}

static float rms(const std::vector<float>& buf, int skipFirst)
{
    double sum = 0.0;
    int count = 0;
    for (size_t i = (size_t) skipFirst; i < buf.size(); ++i)
    {
        sum += buf[i] * buf[i];
        ++count;
    }
    return std::sqrt((float) (sum / juce::jmax(1, count)));
}

TEST_CASE("BassExtractor: low sine passes through low band")
{
    BassExtractor be;
    be.prepare(44100.0, 2048, 1);
    be.setCrossoverHz(200.0f);

    const int n = 2048;
    std::vector<float> in(n), low(n), high(n);
    fillSine(in, 50.0f, 44100.0f);  // well below crossover

    be.process(0, in.data(), low.data(), high.data(), n);

    // Most energy should be in the low band (skip first 256 samples for filter to settle)
    const float lowRms  = rms(low,  256);
    const float highRms = rms(high, 256);
    REQUIRE(lowRms > highRms * 5.0f);
}

TEST_CASE("BassExtractor: high sine passes through high band")
{
    BassExtractor be;
    be.prepare(44100.0, 2048, 1);
    be.setCrossoverHz(200.0f);

    const int n = 2048;
    std::vector<float> in(n), low(n), high(n);
    fillSine(in, 2000.0f, 44100.0f);  // well above crossover

    be.process(0, in.data(), low.data(), high.data(), n);

    const float lowRms  = rms(low,  256);
    const float highRms = rms(high, 256);
    REQUIRE(highRms > lowRms * 5.0f);
}

TEST_CASE("BassExtractor: magnitude preservation — low+high RMS matches input RMS")
{
    BassExtractor be;
    be.prepare(44100.0, 4096, 1);
    be.setCrossoverHz(200.0f);

    const int n = 4096;
    std::vector<float> in(n), low(n), high(n), sum(n);
    fillSine(in, 200.0f, 44100.0f);

    be.process(0, in.data(), low.data(), high.data(), n);

    for (int i = 0; i < n; ++i)
        sum[(size_t) i] = low[(size_t) i] + high[(size_t) i];

    // LR4: low + high = allpass(input) — same magnitude, shifted phase.
    // Test RMS instead of sample-exact reconstruction.
    const float inRms  = rms(in,  2048);
    const float sumRms = rms(sum, 2048);
    REQUIRE(sumRms == Catch::Approx(inRms).epsilon(0.05));
}
