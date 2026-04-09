#pragma once
#include <JuceHeader.h>

// Applies ISO 226 A-weighting equal-loudness correction so phantom harmonics
// are perceived at the same loudness as the original fundamental.
class PerceptualOptimizer
{
public:
    void  prepare(double sampleRate, int maxBlockSize) noexcept;
    void  reset() noexcept {}

    void  setFundamental(float hz) noexcept { fundamentalHz = hz; }

    // Returns linear gain multiplier. Inverse of A-weighting at freq Hz.
    // 1.0f at 1 kHz (reference), > 1.0f at low frequencies.
    float getLoudnessGain(float freq) const noexcept;

    // Applies average harmonic gain to all samples in buffer.
    void  process(juce::AudioBuffer<float>& buffer) noexcept;

private:
    double sampleRate    = 44100.0;
    float  fundamentalHz = 80.0f;

    static float aWeightingDb(float freq) noexcept;
};
