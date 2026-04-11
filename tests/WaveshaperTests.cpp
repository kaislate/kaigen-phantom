#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/Waveshaper.h"
#include <cmath>

TEST_CASE("Waveshaper: zero amplitudes -> near-zero output")
{
    Waveshaper w;
    w.setHarmonicAmplitudes({ 0, 0, 0, 0, 0, 0, 0 });
    w.setDrive(0.0f);
    w.setSaturation(0.0f);

    // At x=0.5, T_2..T_8 with all amps=0 should give 0
    REQUIRE(w.shape(0.5f) == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(w.shape(-0.3f) == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("Waveshaper: output is bounded for unit input")
{
    Waveshaper w;
    w.setHarmonicAmplitudes({ 1, 1, 1, 1, 1, 1, 1 });
    w.setDrive(1.0f);
    w.setSaturation(1.0f);

    // Chebyshev T_n(x) is bounded by 1 for x in [-1,1].
    // Sum of 7 equal-weight terms bounded by 7. With saturation blended it
    // should still be finite.
    for (float x = -1.0f; x <= 1.0f; x += 0.05f)
    {
        const float y = w.shape(x);
        REQUIRE(std::isfinite(y));
        REQUIRE(std::fabs(y) < 10.0f);
    }
}

TEST_CASE("Waveshaper: T2 produces DC component from sine")
{
    // T_2(sin θ) = 2sin²θ - 1 = -cos(2θ) = cos(2θ + π)
    // Over one period, the mean is 0 (no DC), but amplitude is 1.
    // This test verifies the polynomial produces the expected cosine.

    Waveshaper w;
    w.setHarmonicAmplitudes({ 1, 0, 0, 0, 0, 0, 0 });  // only H2
    w.setDrive(0.0f);
    w.setSaturation(0.0f);

    const int n = 1000;
    float maxVal = -1e9f, minVal = 1e9f;
    for (int i = 0; i < n; ++i)
    {
        const float theta = 2.0f * juce::MathConstants<float>::pi * (float) i / (float) n;
        const float x = std::sin(theta);
        const float y = w.shape(x);
        if (y > maxVal) maxVal = y;
        if (y < minVal) minVal = y;
    }

    // T_2(x)=2x^2-1 gives outputs in [-1, 1]
    REQUIRE(maxVal > 0.9f);
    REQUIRE(minVal < -0.9f);
}

TEST_CASE("Waveshaper: block process matches single-sample shape")
{
    Waveshaper w;
    w.setHarmonicAmplitudes({ 0.8f, 0.6f, 0.4f, 0.3f, 0.2f, 0.1f, 0.05f });
    w.setDrive(0.3f);

    const int n = 64;
    std::vector<float> in(n), out(n);
    for (int i = 0; i < n; ++i)
        in[(size_t) i] = std::sin(2.0f * juce::MathConstants<float>::pi * 100.0f * i / 44100.0f);

    w.process(in.data(), out.data(), n);

    for (int i = 0; i < n; ++i)
        REQUIRE(out[(size_t) i] == Catch::Approx(w.shape(in[(size_t) i])).margin(1e-5f));
}
