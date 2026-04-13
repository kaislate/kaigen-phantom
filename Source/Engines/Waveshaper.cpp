#include "Waveshaper.h"
#include <cmath>

void Waveshaper::setHarmonicAmplitudes(const std::array<float, 7>& newAmps) noexcept
{
    for (int i = 0; i < 7; ++i)
        amps[(size_t) i] = juce::jlimit(0.0f, 1.0f, newAmps[(size_t) i]);
}

void Waveshaper::prepare(double sampleRate) noexcept
{
    const double rampMs = 10.0;  // 10 ms ramp — imperceptible lag, eliminates clicks
    smoothDrive.reset(sampleRate, rampMs / 1000.0);
    smoothSat  .reset(sampleRate, rampMs / 1000.0);
    smoothDrive.setCurrentAndTargetValue(smoothDrive.getTargetValue());
    smoothSat  .setCurrentAndTargetValue(smoothSat  .getTargetValue());
}

void Waveshaper::setDrive(float d) noexcept
{
    smoothDrive.setTargetValue(juce::jlimit(0.0f, 1.0f, d));
}

void Waveshaper::setSaturation(float s) noexcept
{
    smoothSat.setTargetValue(juce::jlimit(0.0f, 1.0f, s));
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

float Waveshaper::shape(float x) noexcept
{
    // Advance smoothers one sample step — this is what makes knob moves artifact-free.
    const float drive      = smoothDrive.getNextValue();
    const float saturation = smoothSat  .getNextValue();

    // Apply drive: scale input into the nonlinearity. Drive 0 = unity, 1 = 5x.
    // tanh soft-clips instead of hard jlimit — eliminates click artifacts from
    // sudden transients (e.g., filter settling on note changes).
    const float driven = std::tanh(x * (1.0f + drive * 4.0f));

    // Evaluate Chebyshev polynomials
    float t[7];
    chebyshev(driven, t);

    // Weighted sum — each amp controls exactly one harmonic
    float y = 0.0f;
    for (int i = 0; i < 7; ++i)
        y += amps[(size_t) i] * t[i];

    // Blend in a tanh saturation curve for harmonic richness.
    // Apply tanh to the harmonic sum (y), NOT the input (driven), so that
    // the saturation never leaks the fundamental back into the output.
    if (saturation > 0.0f)
    {
        const float satCurve = std::tanh(y * 3.0f);
        y = y * (1.0f - saturation * 0.5f) + satCurve * saturation;
    }

    return y;
}

void Waveshaper::process(const float* in, float* out, int n) noexcept
{
    for (int i = 0; i < n; ++i)
        out[i] = shape(in[i]);
}
