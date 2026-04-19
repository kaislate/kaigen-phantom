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
    /** Minimum waveset length [2–500 samples]. Crossings closer than this are rejected.
     *  Low = allow high-frequency wavesets. High = bass-only. Default 11 (≈4 kHz at 44.1 kHz). */
    void setMinPeriodSamples(float samples) noexcept;
    /** Maximum waveset length [100–8000 samples]. Crossings further apart than this are rejected.
     *  Default 5513 (≈8 Hz at 44.1 kHz). */
    void setMaxPeriodSamples(float samples) noexcept;
    /** H1 amplitude [0–2]. Controls fundamental reconstruction in RESYN mode. Default 1.0. */
    void setH1Amplitude(float amp) noexcept;
    /** Sub-harmonic amplitude [0–2]. Synthesises one octave below the fundamental. Default 0. */
    void setSubAmplitude(float amp) noexcept;
    /** Upward expansion threshold [0–1]. Normalised to inputPeak. Default 0 (off). */
    void setBoostThreshold(float thr) noexcept;
    /** Upward expansion amount [0–2]. Additional gain multiplier. Default 0. */
    void setBoostAmount(float amt) noexcept;

    /** Free-run: when true, the wavelet keeps firing at the last latched period
     *  even after inputPeak drops below the amplitude floor — phase wraps
     *  automatically instead of silencing. Used during MIDI-gated release so
     *  the tail rings out for the full envelope release time. */
    void setFreeRun(bool on) noexcept { freeRun = on; }

    float process(float x) noexcept;
    float getEstimatedHz() const noexcept;
    /** Per-wavelet peak: amplitude of the last completed wavelet cycle. Updated at every
     *  valid crossing. Used by PhantomEngine Punch feature to blend envelope vs peak scaling. */
    float getWaveletPeak() const noexcept { return lastWaveletPeak; }
    /** Running peak of the bass-band input signal. Used by the UI to position gate threshold
     *  lines accurately relative to the actual signal the gate is operating on. */
    float getInputPeak() const noexcept { return inputPeak; }

private:
    double sampleRate = 44100.0;

    float lastSample               = 0.0f;
    float samplesSinceLastCrossing = 0.0f;
    float accumulatedSamples       = 0.0f;
    int   crossingsAccum           = 0;
    int   skipCount                = 0;
    float estimatedPeriod          = 441.0f;
    float trackingAlpha            = 0.15f;
    float minPeriodSamples         = 11.0f;    // default ≈ 4 kHz at 44.1 kHz
    float maxPeriodSamples         = 5513.0f;  // default ≈ 8 Hz at 44.1 kHz
    float h1Amp                    = 1.0f;
    float subAmp                   = 0.0f;  // sub-harmonic (half fundamental freq)

    // Per-wavelet peak: resets on each valid crossing, latches on completion.
    // Used by PhantomEngine to scale output per-wavelet rather than per-envelope.
    float currentWaveletPeak       = 0.0f;
    float lastWaveletPeak          = 0.0f;

    float currentPhase = 0.0f;   // resets to 0.0 at every positive zero crossing

    std::array<float, 7> amps {};   // H2..H8 amplitudes

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothStep   { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDuty   { 0.5f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothLength { 1.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothGate   { 0.0f };
    // Miya-style waveset gate: hard gating with hysteresis.
    // Measures RMS energy per waveset interval. Binary OPEN/CLOSED state.
    float currentWaveletSumSq      = 0.0f;   // running sum of x^2 for RMS
    int   currentWaveletSampleCount = 0;      // sample count for RMS
    float lastWaveletRMS           = 0.0f;    // RMS of last completed waveset
    bool  gateOpen                 = true;    // gate state with hysteresis

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothBoostThr { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothBoostAmt { 0.0f };
    // Smoothed gate gain: ramps 0↔1 over ~5 ms instead of hard-cutting.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateGainSmooth { 1.0f };
    // Boost gain one-pole smoother: target is set at each valid crossing,
    // smoothedBoostGain tracks it per-sample to avoid step discontinuities.
    float targetBoostGain   = 1.0f;
    float smoothedBoostGain = 1.0f;

    // Fast-attack / slow-release amplitude envelope.
    // Freezes period updates when input is too quiet to give reliable pitch info
    // (e.g. noise during a long envelope release tail).
    float inputPeak = 0.0f;

    bool freeRun = false;

    static float warpPhase(float phase, float duty) noexcept;
    static float shapedWave(float wp, float step) noexcept;
};
