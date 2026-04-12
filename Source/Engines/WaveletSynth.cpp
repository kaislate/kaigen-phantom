#include "WaveletSynth.h"
#include <cmath>

static constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
static constexpr float kPi    = juce::MathConstants<float>::pi;

// ── Setup ──────────────────────────────────────────────────────────────────

void WaveletSynth::prepare(double sr) noexcept
{
    sampleRate = sr;

    // Valid period range: 16 Hz (sub) to 4 kHz (high harmonics)
    minPeriodSamples = (float)(sr / 4000.0);
    maxPeriodSamples = (float)(sr / 16.0);

    const double rampSec = 0.010; // 10 ms parameter smoothing
    smoothStep.reset(sr, rampSec);
    smoothDuty.reset(sr, rampSec);
    smoothStep.setCurrentAndTargetValue(0.0f);
    smoothDuty.setCurrentAndTargetValue(0.5f);
    smoothLength.reset(sr, rampSec);
    smoothLength.setCurrentAndTargetValue(1.0f);
    smoothGate.reset(sr, rampSec);
    smoothGate.setCurrentAndTargetValue(0.0f);

    reset();
}

void WaveletSynth::reset() noexcept
{
    lastSample               = 0.0f;
    samplesSinceLastCrossing = 0.0f;
    accumulatedSamples       = 0.0f;
    crossingsAccum           = 0;
    currentPhase             = 0.0f;
    estimatedPeriod          = (float)(sampleRate / 100.0); // 100 Hz safe default
    lastNegativePeak         = 0.0f;
}

// ── Parameter setters ──────────────────────────────────────────────────────

void WaveletSynth::setHarmonicAmplitudes(const std::array<float, 7>& newAmps) noexcept
{
    for (int i = 0; i < 7; ++i)
        amps[(size_t)i] = juce::jlimit(0.0f, 1.0f, newAmps[(size_t)i]);
}

void WaveletSynth::setStep(float s) noexcept
{
    smoothStep.setTargetValue(juce::jlimit(0.0f, 1.0f, s));
}

void WaveletSynth::setDutyCycle(float d) noexcept
{
    smoothDuty.setTargetValue(juce::jlimit(0.05f, 0.95f, d));
}

void WaveletSynth::setWaveletLength(float len) noexcept
{
    smoothLength.setTargetValue(juce::jlimit(0.05f, 1.0f, len));
}

void WaveletSynth::setGateThreshold(float thr) noexcept
{
    smoothGate.setTargetValue(juce::jlimit(0.0f, 1.0f, thr));
}

void WaveletSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(1, 8, n);
    if (newSkip != skipCount)
    {
        skipCount      = newSkip;
        crossingsAccum = 0;       // restart accumulation on skip change
        accumulatedSamples = 0.0f;
    }
}

float WaveletSynth::getEstimatedHz() const noexcept
{
    return estimatedPeriod > 0.0f ? (float)(sampleRate / (double)estimatedPeriod) : 0.0f;
}

// ── Waveform helpers ───────────────────────────────────────────────────────

float WaveletSynth::warpPhase(float phase, float duty) noexcept
{
    // Standard PWM phase warp.
    //
    // Maps the [0, 2π] cycle so the positive half occupies duty×2π of the input
    // and the negative half occupies the remainder. Both halves still complete
    // a full half-cycle in the warped domain → the waveform is always periodic.
    //
    // At duty=0.5 this is identity (no warp).
    // At duty>0.5 the positive half is wider → even harmonics are emphasised.
    // At duty<0.5 the positive half is narrower → odd harmonics are emphasised.
    const float d = juce::jlimit(0.05f, 0.95f, duty);
    if (phase < kTwoPi * d)
        return phase / (2.0f * d);
    else
        return kPi + (phase - kTwoPi * d) / (2.0f * (1.0f - d));
}

float WaveletSynth::shapedWave(float wp, float step) noexcept
{
    // Pure sine at the warped phase.
    const float s = std::sin(wp);
    if (step <= 0.0f) return s;

    // Push toward square via tanh saturation.
    // drive scales 1→20 as step goes 0→1.
    // Dividing by tanh(drive) normalises peak amplitude to ≈1 at all step values.
    const float drive  = 1.0f + step * 19.0f;
    const float tanhD  = std::tanh(drive);
    return std::tanh(s * drive) / tanhD;
}

// ── Per-sample processing ──────────────────────────────────────────────────

float WaveletSynth::process(float x) noexcept
{
    // ── Period detection with skip accumulation ──────────────────────────
    //
    // samplesSinceLastCrossing: validates each individual crossing interval
    //   is a plausible audio-frequency period (avoids noise crossings).
    // accumulatedSamples: total samples across skipCount crossings — this
    //   becomes the estimated period (= skipCount × actual period when skipCount>1).
    //
    // Example: skip=2, fundamental=100Hz (441 samples per cycle).
    //   After 2 valid crossings, accumulatedSamples ≈ 882.
    //   estimatedPeriod ≈ 882 → phase advances at 2π/882 per sample.
    //   H2 = sin(2 × φ) completes one cycle every 441 samples → lands on 100Hz
    //   (the original fundamental). Sub-harmonic synthesis in action.
    samplesSinceLastCrossing += 1.0f;
    accumulatedSamples       += 1.0f;

    if (lastSample <= 0.0f && x > 0.0f)
    {
        // Only count this crossing if the individual interval is in range.
        // Out-of-range means noise or a frequency outside [16 Hz, 4 kHz].
        if (samplesSinceLastCrossing >= minPeriodSamples &&
            samplesSinceLastCrossing <= maxPeriodSamples)
        {
            crossingsAccum++;

            if (crossingsAccum >= skipCount)
            {
                // Rate-limit period changes before applying the EMA.
                // Without this, noisy residual signal during note release gets
                // normalised to unit amplitude and its (higher-frequency) zero
                // crossings cause the period estimate to snap up an octave.
                // Clamping to ±20% per measurement lets legitimate pitch changes
                // track freely while blocking single-crossing octave jumps.
                const float maxDelta = estimatedPeriod * 0.20f;
                const float delta    = accumulatedSamples - estimatedPeriod;
                estimatedPeriod += trackingAlpha * juce::jlimit(-maxDelta, maxDelta, delta);

                accumulatedSamples = 0.0f;
                crossingsAccum     = 0;
            }
            // KEY DIFFERENCE from ZCS: reset phase only on valid crossings.
            // Each valid interval is a fresh wavelet starting at phase 0.
            currentPhase             = 0.0f;
            samplesSinceLastCrossing = 0.0f;
        }
        else
        {
            // Invalid crossing — reset accumulation, but do NOT reset phase
            accumulatedSamples       = 0.0f;
            crossingsAccum           = 0;
            samplesSinceLastCrossing = 0.0f;
        }
    }
    lastSample = x;

    // ── Phase advance ────────────────────────────────────────────────────
    currentPhase += kTwoPi / estimatedPeriod;
    if (currentPhase >= kTwoPi)
        currentPhase -= kTwoPi;

    // ── Advance shape smoothers (one step per audio sample) ──────────────
    const float step = smoothStep.getNextValue();
    const float duty = smoothDuty.getNextValue();

    // ── Synthesise H1 + H2-H8 ────────────────────────────────────────────
    // H1 at amplitude 1.0 — fundamental carrier, always present in RESYN mode
    float y = shapedWave(warpPhase(currentPhase, duty), step);

    // H2-H8 additive content from recipe wheel
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;
        float hp = std::fmod((float)(i + 2) * currentPhase, kTwoPi);
        y += amps[(size_t)i] * shapedWave(warpPhase(hp, duty), step);
    }

    return y;
}
