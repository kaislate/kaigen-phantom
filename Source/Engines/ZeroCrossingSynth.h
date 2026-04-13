#pragma once
#include <JuceHeader.h>
#include <array>

/**
 * ZeroCrossingSynth — period-detection re-synthesis harmonic generator.
 *
 * Detects positive-slope zero crossings in the input signal to estimate the
 * dominant instantaneous period. Synthesises harmonics H2-H8 as integer
 * multiples of that period using a parameterised waveform shape.
 *
 * Unlike Chebyshev waveshaping, this approach generates ONLY the requested
 * harmonics — the fundamental is never present in the output regardless of
 * input amplitude, recipe, or odd/even harmonic selection.
 *
 * Works on polyphonic and complex signals: zero crossings of the composite
 * waveform track whichever periodic component is dominant in the band,
 * reinforcing its harmonics musically across the full recipe.
 *
 * Signal chain per sample:
 *   normalisedInput → crossingDetection → phaseAdvance
 *   → Σ amps[n] * shapedWave(n * φ, step, duty) → harmonicsSum
 */
class ZeroCrossingSynth
{
public:
    // ── Setup ────────────────────────────────────────────────────────────────
    /** Must be called before processing. Initialises period bounds and smoothers. */
    void prepare(double sampleRate) noexcept;

    /** Clears all state without changing parameters. */
    void reset() noexcept;

    // ── Recipe ───────────────────────────────────────────────────────────────
    /** Per-harmonic amplitudes H2..H8, each [0, 1]. */
    void setHarmonicAmplitudes(const std::array<float, 7>& amps) noexcept;

    // ── Waveform shape ───────────────────────────────────────────────────────
    /** Morphs output waveform: 0 = pure sine, 1 = square.
     *  Implemented via tanh drive on the sine so the transition is smooth
     *  and amplitude stays ≈ 1 at all settings. */
    void setStep(float step) noexcept;

    /** Pulse width / duty cycle [0.05, 0.95]. 0.5 = symmetric (default).
     *  Values away from 0.5 create asymmetric waveforms that emphasise or
     *  suppress even/odd harmonics independently of the recipe wheel. */
    void setDutyCycle(float duty) noexcept;

    // ── Zero-crossing skip ────────────────────────────────────────────────────
    /** How many positive zero crossings to accumulate before updating the period
     *  estimate. [1, 8].
     *
     *  Skip=1 (default): each crossing = one period → normal harmonic synthesis.
     *  Skip=2: two crossings treated as one period → estimated period = 2T →
     *          H2 lands on the actual fundamental, H4 on its 2nd harmonic, etc.
     *          Creates sub-harmonic synthesis anchored one octave down.
     *  Skip=N: shifts all harmonics down by a factor of N, progressively
     *          mixing sub-harmonic content with the original pitch's harmonics.
     *
     *  Individual crossing intervals are still validated against min/max bounds
     *  so noise crossings don't corrupt the accumulation. */
    void setSkipCount(int n) noexcept;

    // ── Tracking ─────────────────────────────────────────────────────────────
    /** Period-tracking smoothing [0.01, 0.8].
     *  Low = stable but slow to track pitch changes.
     *  High = fast tracking but noisier on complex polyphonic input. */
    void setTrackingSpeed(float speed) noexcept;
    /** Maximum frequency to track [200–20000 Hz]. Crossings faster than this are rejected.
     *  Low = only deep/bass content synthesised. High = full-range / vocal mode. */
    void setMaxTrackHz(float hz) noexcept;

    // ── Processing ───────────────────────────────────────────────────────────
    /** Process one sample of the raw bass-band signal.
     *  Returns synthesised harmonics sum only — no fundamental component.
     *  Not const: advances per-sample parameter smoothers and crossing state. */
    float process(float x) noexcept;

    /** Estimated fundamental frequency in Hz at this moment. */
    float getEstimatedHz() const noexcept;
    /** Per-wavelet peak: amplitude of the last completed crossing interval.
     *  Updated at every valid crossing. Used by PhantomEngine Punch feature. */
    float getWaveletPeak() const noexcept { return lastWaveletPeak; }

private:
    double sampleRate = 44100.0;

    // ── Period detection ─────────────────────────────────────────────────────
    float lastSample              = 0.0f;
    float samplesSinceLastCrossing = 0.0f;  // resets each crossing — for validation
    float accumulatedSamples      = 0.0f;   // resets each measurement — actual period source
    int   crossingsAccum          = 0;      // how many valid crossings since last measurement
    int   skipCount               = 1;      // crossings needed per measurement
    float estimatedPeriod         = 441.0f; // default ~100 Hz
    float trackingAlpha           = 0.15f;
    float maxTrackHz              = 4000.0f;
    float minPeriodSamples        = 0.0f;   // set in prepare() — per individual crossing
    float maxPeriodSamples        = 0.0f;

    // Fast-attack / slow-release amplitude tracker.
    // Scales trackingAlpha proportionally so quiet signals drift the period
    // estimate less than loud ones. Freezes updates entirely below −40 dBFS.
    float inputPeak = 0.0f;

    // Per-crossing peak: max |x| within the current crossing interval.
    // Latched to lastWaveletPeak on each valid crossing for Punch feature.
    float currentWaveletPeak = 0.0f;
    float lastWaveletPeak    = 0.0f;

    // ── Synthesis phase ───────────────────────────────────────────────────────
    float fundamentalPhase = 0.0f;

    // ── Recipe ────────────────────────────────────────────────────────────────
    std::array<float, 7> amps {};

    // ── Shape parameters (per-sample smoothed to prevent clicks) ─────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothStep;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDuty;

    // ── Waveform helpers ──────────────────────────────────────────────────────
    /** Standard PWM phase warp. Duty 0.5 = identity. */
    static float warpPhase(float phase, float duty) noexcept;

    /** Sine → square morph via tanh saturation. step=0 → pure sine. */
    static float shapedWave(float warpedPhase, float step) noexcept;
};
