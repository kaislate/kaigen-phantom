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
    void setWaveletLength(float len) noexcept;    // 0.05–1.0: fraction of period to synthesise
    void setGateThreshold(float thr) noexcept;   // 0.0–1.0: min negative-peak amplitude for valid crossing
    /** Period-tracking EMA speed [0.01, 0.8]. Low = stable/glide, high = fast/responsive. */
    void setTrackingSpeed(float speed) noexcept;
    /** Maximum frequency to track [200–20000 Hz]. Crossings faster than this are rejected.
     *  Low = only deep/bass content synthesised. High = full-range / vocal mode. */
    void setMaxTrackHz(float hz) noexcept;
    /** H1 amplitude [0–1]. Controls fundamental reconstruction in RESYN mode. Default 1.0. */
    void setH1Amplitude(float amp) noexcept;

    float process(float x) noexcept;
    float getEstimatedHz() const noexcept;
    /** Per-wavelet peak: amplitude of the last completed wavelet cycle. Updated at every
     *  valid crossing. Used by PhantomEngine Punch feature to blend envelope vs peak scaling. */
    float getWaveletPeak() const noexcept { return lastWaveletPeak; }

private:
    double sampleRate = 44100.0;

    float lastSample               = 0.0f;
    float samplesSinceLastCrossing = 0.0f;
    float accumulatedSamples       = 0.0f;
    int   crossingsAccum           = 0;
    int   skipCount                = 1;
    float estimatedPeriod          = 441.0f;
    float trackingAlpha            = 0.15f;
    float maxTrackHz               = 4000.0f;
    float minPeriodSamples         = 0.0f;
    float maxPeriodSamples         = 0.0f;
    float h1Amp                    = 1.0f;

    // Per-wavelet peak: resets on each valid crossing, latches on completion.
    // Used by PhantomEngine to scale output per-wavelet rather than per-envelope.
    float currentWaveletPeak       = 0.0f;
    float lastWaveletPeak          = 0.0f;

    float currentPhase = 0.0f;   // resets to 0.0 at every positive zero crossing

    std::array<float, 7> amps {};   // H2..H8 amplitudes

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothStep;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDuty;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothLength;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothGate;
    float lastNegativePeak = 0.0f;

    // Fast-attack / slow-release amplitude envelope.
    // Freezes period updates when input is too quiet to give reliable pitch info
    // (e.g. noise during a long envelope release tail).
    float inputPeak = 0.0f;

    static float warpPhase(float phase, float duty) noexcept;
    static float shapedWave(float wp, float step) noexcept;
};
