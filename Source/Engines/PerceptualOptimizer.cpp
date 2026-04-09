#include "PerceptualOptimizer.h"
#include <cmath>

void PerceptualOptimizer::prepare(double sr, int /*maxBlockSize*/) noexcept
{
    sampleRate = sr;
}

float PerceptualOptimizer::getLoudnessGain(float freq) const noexcept
{
    // Inverse of A-weighting: boost frequencies where the ear is less sensitive.
    // At 1 kHz (reference), A-weighting ≈ 0 dB → gain = 1.0.
    // At 80 Hz, A-weighting ≈ -22 dB → inverse gain ≈ +22 dB → linear gain > 1.
    const float dbCorrection = -aWeightingDb(freq);
    return std::pow(10.0f, dbCorrection / 20.0f);
}

float PerceptualOptimizer::aWeightingDb(float freq) noexcept
{
    // IEC 61672 A-weighting:
    // A(f) = 2.0 + 20*log10( f^4 * 12194^2 /
    //          ((f^2 + 20.6^2) * sqrt((f^2+107.7^2)*(f^2+737.9^2)) * (f^2+12194^2)) )
    if (freq < 1.0f) freq = 1.0f;
    const double f  = (double)freq;
    const double f2 = f * f;
    const double f4 = f2 * f2;

    const double num = f4 * (12194.0 * 12194.0);
    const double d1  = f2 + 20.6  * 20.6;
    const double d2  = std::sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9));
    const double d3  = f2 + 12194.0 * 12194.0;
    const double den = d1 * d2 * d3;

    if (den < 1e-30) return -100.0f;
    return (float)(2.0 + 20.0 * std::log10(num / den));
}

void PerceptualOptimizer::process(juce::AudioBuffer<float>& buffer) noexcept
{
    if (fundamentalHz <= 0.0f) return;

    // Average gain over harmonics H2..H8
    float avgGain = 0.0f;
    for (int h = 2; h <= 8; ++h)
        avgGain += getLoudnessGain(fundamentalHz * (float)h);
    avgGain /= 7.0f;

    // Clamp to avoid extreme boosts at very low fundamentals
    avgGain = juce::jlimit(0.5f, 4.0f, avgGain);

    const int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= avgGain;
    }
}
