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

TEST_CASE("Waveshaper: T2 output range with drive=0 tanh preprocessing")
{
    // shape() always runs tanh(x * (1 + drive*4)) before Chebyshev evaluation.
    // At drive=0: driven = tanh(sin θ).  tanh(1) ≈ 0.7616, so the peak input
    // to T_2 is ~0.76 rather than 1.0, giving T_2(0.76) = 2*0.76^2-1 ≈ 0.16.
    // This compression is by design — drive controls how much of the Chebyshev
    // non-linearity is reached.  Higher drive pushes toward full ±1 output.

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

    // drive=0: T_2(tanh(sin θ)):
    //   min = T_2(tanh(0)) = T_2(0) = -1  (at every zero crossing of the input)
    //   max = T_2(tanh(1)) = 2*tanh(1)^2 - 1 ≈ 0.16  (at the positive peak)
    // The range is asymmetric — T_2's -1 floor is always hit at zero crossings.
    const float expectedMax = 2.0f * std::tanh(1.0f) * std::tanh(1.0f) - 1.0f;
    REQUIRE(maxVal == Catch::Approx(expectedMax).margin(0.01f));
    REQUIRE(minVal == Catch::Approx(-1.0f).margin(0.01f));
}

TEST_CASE("Waveshaper: T2 approaches full range at high drive")
{
    // At drive=0.8: driven = tanh(sin θ * 4.2) ≈ tanh(4.2) ≈ 0.9993 at peak.
    // T_2(0.9993) ≈ 2*0.9993^2 - 1 ≈ 0.9986 — close to ±1.
    Waveshaper w;
    w.prepare(44100.0);
    w.setHarmonicAmplitudes({ 1, 0, 0, 0, 0, 0, 0 });
    w.setDrive(0.8f);
    w.setSaturation(0.0f);

    // Let smoother settle
    for (int i = 0; i < 2000; ++i)
        w.shape(0.0f);

    const int n = 1000;
    float maxVal = -1e9f, minVal = 1e9f;
    for (int i = 0; i < n; ++i)
    {
        const float theta = 2.0f * juce::MathConstants<float>::pi * (float) i / (float) n;
        const float y = w.shape(std::sin(theta));
        if (y > maxVal) maxVal = y;
        if (y < minVal) minVal = y;
    }

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
