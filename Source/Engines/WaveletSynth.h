#pragma once
#include <JuceHeader.h>
#include <array>

/**
 * WaveletSynth — zero-crossing-triggered wavelet resynthesizer.
 *
 * Like ZeroCrossingSynth, detects positive-slope zero crossings to estimate
 * the instantaneous period. Unlike ZCS, the oscillator phase resets to 0 at
 * every valid crossing — each interval between crossings is a fresh wavelet
 * aligned to the source waveform at its boundaries.
 *
 * Synthesises H1 (fundamental, always 1.0) + H2-H8 (user-controlled recipe).
 * H1 ensures the output reconstructs the source interval; H2-H8 add harmonic
 * colour on top.
 */
class WaveletSynth
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    /** Per-harmonic amplitudes H2..H8 (H1 is always 1.0). */
    void setHarmonicAmplitudes(const std::array<float, 7>& amps) noexcept;
    void setStep(float step) noexcept;
    void setDutyCycle(float duty) noexcept;
    void setSkipCount(int n) noexcept;
    void setTrackingSpeed(float speed) noexcept;

    float process(float x) noexcept;
    float getEstimatedHz() const noexcept;

private:
    double sampleRate = 44100.0;

    float lastSample               = 0.0f;
    float samplesSinceLastCrossing = 0.0f;
    float accumulatedSamples       = 0.0f;
    int   crossingsAccum           = 0;
    int   skipCount                = 1;
    float estimatedPeriod          = 441.0f;
    float trackingAlpha            = 0.15f;
    float minPeriodSamples         = 0.0f;
    float maxPeriodSamples         = 0.0f;

    float currentPhase = 0.0f;   // resets to 0.0 at every positive zero crossing

    std::array<float, 7> amps {};   // H2..H8 amplitudes

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothStep;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDuty;

    static float warpPhase(float phase, float duty) noexcept;
    static float shapedWave(float wp, float step) noexcept;
};
