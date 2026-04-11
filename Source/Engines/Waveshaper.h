#pragma once
#include <JuceHeader.h>
#include <array>

/**
 * Waveshaper — nonlinear harmonic generator using Chebyshev polynomials.
 *
 * For input x in [-1, 1], the Chebyshev polynomial T_n has the property
 * T_n(cos θ) = cos(nθ). So if you feed a unit-amplitude sine wave through
 * T_n, the output is a unit-amplitude cosine at n × the input frequency.
 *
 * Summing a_2·T_2(x) + a_3·T_3(x) + ... + a_8·T_8(x) gives direct,
 * independent control over the amplitudes of harmonics H2 through H8
 * relative to the fundamental.
 *
 * This is frequency-agnostic: it generates harmonics for ANY input
 * frequency including sub-100Hz bass content, with zero tracking latency
 * and zero voice management.
 */
class Waveshaper
{
public:
    /** Sets per-harmonic amplitudes for H2..H8, each in [0, 1]. */
    void setHarmonicAmplitudes(const std::array<float, 7>& amps) noexcept;

    /** Drive multiplies the input into the nonlinearity. [0, 1] maps to 1x..5x. */
    void setDrive(float drive) noexcept;

    /** Saturation blends a tanh-shaped curve on top of the Chebyshev output. [0, 1]. */
    void setSaturation(float sat) noexcept;

    /** Processes a block of samples (mono). Output is unnormalised harmonic signal. */
    void process(const float* in, float* out, int n) noexcept;

    /** Evaluates the waveshaping function on a single sample. */
    float shape(float x) const noexcept;

private:
    std::array<float, 7> amps { 0.80f, 0.70f, 0.50f, 0.35f, 0.20f, 0.12f, 0.07f };
    float drive      = 0.5f;
    float saturation = 0.0f;

    /** Evaluates T2..T8 Chebyshev polynomials for a single sample in [-1, 1]. */
    static void chebyshev(float x, float* t /* size 7 */) noexcept;
};
