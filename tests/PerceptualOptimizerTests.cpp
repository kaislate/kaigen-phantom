#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/PerceptualOptimizer.h"

TEST_CASE("PerceptualOptimizer: gain at 80 Hz is greater than at 1000 Hz")
{
    PerceptualOptimizer opt;
    float gain80   = opt.getLoudnessGain(80.0f);
    float gain1000 = opt.getLoudnessGain(1000.0f);
    // Equal-loudness: ear requires more SPL at 80 Hz, so we boost it
    REQUIRE(gain80 > gain1000);
}

TEST_CASE("PerceptualOptimizer: gain is positive for all harmonic frequencies")
{
    PerceptualOptimizer opt;
    for (int h = 2; h <= 8; ++h)
    {
        float freq = 40.0f * (float)h;
        REQUIRE(opt.getLoudnessGain(freq) > 0.0f);
    }
}

TEST_CASE("PerceptualOptimizer: process boosts signal at low fundamental")
{
    PerceptualOptimizer opt;
    opt.prepare(44100.0, 512);

    juce::AudioBuffer<float> buf(2, 512);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            buf.setSample(ch, i, 0.5f);

    opt.setFundamental(40.0f);
    opt.process(buf);

    float peakAfter = 0.0f;
    for (int i = 0; i < 512; ++i)
        peakAfter = std::max(peakAfter, std::abs(buf.getSample(0, i)));

    REQUIRE(peakAfter > 0.5f);  // 40 Hz harmonics are in boosted region
}
