#pragma once
#include <JuceHeader.h>

/**
 * BassExtractor — Linkwitz-Riley 4th-order crossover.
 *
 * Splits an input signal into two bands around a given crossover frequency.
 * Uses a single LinkwitzRileyFilter in allpass mode producing both bands
 * from the same filter state — this guarantees perfect reconstruction
 * (low + high = input).
 */
class BassExtractor
{
public:
    void prepare(double sampleRate, int blockSize, int numChannels);
    void reset();

    /** Sets the crossover frequency in Hz. */
    void setCrossoverHz(float hz);

    /** Processes one channel: writes lowOut/highOut from input. */
    void process(int channel, const float* in, float* lowOut, float* highOut, int n);

private:
    juce::dsp::LinkwitzRileyFilter<float> filter;
    double sampleRate = 44100.0;
    float  crossoverHz = 80.0f;
};
