#include "Waveshaper.h"
#include <cmath>

void Waveshaper::setHarmonicAmplitudes(const std::array<float, 7>& newAmps) noexcept
{
    for (int i = 0; i < 7; ++i)
        amps[(size_t) i] = juce::jlimit(0.0f, 1.0f, newAmps[(size_t) i]);
}

void Waveshaper::setDrive(float d) noexcept
{
    drive = juce::jlimit(0.0f, 1.0f, d);
}

void Waveshaper::setSaturation(float s) noexcept
{
    saturation = juce::jlimit(0.0f, 1.0f, s);
}

void Waveshaper::chebyshev(float x, float* t) noexcept
{
    // T_0 = 1, T_1 = x. Recurrence: T_{n+1} = 2x·T_n - T_{n-1}
    // We only need T_2..T_8.
    const float t0 = 1.0f;
    const float t1 = x;
    t[0] = 2.0f * x * t1 - t0;              // T_2 = 2x^2 - 1
    t[1] = 2.0f * x * t[0] - t1;            // T_3
    t[2] = 2.0f * x * t[1] - t[0];          // T_4
    t[3] = 2.0f * x * t[2] - t[1];          // T_5
    t[4] = 2.0f * x * t[3] - t[2];          // T_6
    t[5] = 2.0f * x * t[4] - t[3];          // T_7
    t[6] = 2.0f * x * t[5] - t[4];          // T_8
}

float Waveshaper::shape(float x) const noexcept
{
    // Apply drive: scale input into the nonlinearity. Drive 0 = unity, 1 = 5x.
    const float driven = juce::jlimit(-1.0f, 1.0f, x * (1.0f + drive * 4.0f));

    // Evaluate Chebyshev polynomials
    float t[7];
    chebyshev(driven, t);

    // Weighted sum — each amp controls exactly one harmonic
    float y = 0.0f;
    for (int i = 0; i < 7; ++i)
        y += amps[(size_t) i] * t[i];

    // Blend in a tanh saturation curve for harmonic richness
    if (saturation > 0.0f)
    {
        const float satCurve = std::tanh(driven * 3.0f);
        y = y * (1.0f - saturation * 0.5f) + satCurve * saturation;
    }

    return y;
}

void Waveshaper::process(const float* in, float* out, int n) noexcept
{
    for (int i = 0; i < n; ++i)
        out[i] = shape(in[i]);
}
