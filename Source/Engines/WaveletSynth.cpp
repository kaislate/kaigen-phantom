#include "WaveletSynth.h"
#include <cmath>

static constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
static constexpr float kPi    = juce::MathConstants<float>::pi;

// ── Setup ──────────────────────────────────────────────────────────────────

void WaveletSynth::prepare(double sr) noexcept
{
    sampleRate = sr;

    // Valid period range: 16 Hz (sub) to maxTrackHz
    minPeriodSamples = (float)(sr / (double)maxTrackHz);
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
    inputPeak                = 0.0f;
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

void WaveletSynth::setTrackingSpeed(float speed) noexcept
{
    trackingAlpha = juce::jlimit(0.01f, 0.8f, speed);
}

void WaveletSynth::setMaxTrackHz(float hz) noexcept
{
    maxTrackHz       = juce::jlimit(200.0f, 20000.0f, hz);
    minPeriodSamples = (float)(sampleRate / (double)maxTrackHz);
}

void WaveletSynth::setH1Amplitude(float amp) noexcept
{
    h1Amp = juce::jlimit(0.0f, 1.0f, amp);
}

void WaveletSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(1, 8, n);
    if (newSkip != skipCount)
    {
        // Scale accumulated samples proportionally so the EMA has a warm start.
        // e.g. going from skip=1 to skip=2: double the accumulation so the
        // first new measurement lands near the current estimate.
        if (accumulatedSamples > 0.0f)
            accumulatedSamples *= (float)newSkip / (float)skipCount;
        skipCount      = newSkip;
        crossingsAccum = 0;
        // estimatedPeriod is intentionally NOT reset
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
    // Fast-attack / slow-release amplitude tracker (~520 ms from 0 dBFS to −40 dBFS floor).
    // Period updates are frozen once inputPeak drops below kAmplitudeFloor so that
    // noise-level zero crossings during a long envelope release cannot walk the
    // estimate to a higher frequency.
    const float absX = std::abs(x);
    inputPeak = (absX > inputPeak) ? absX : inputPeak * 0.9998f;

    // Track most negative excursion since last valid crossing (gate hysteresis)
    if (x < lastNegativePeak)
        lastNegativePeak = x;
    const float gateThr = smoothGate.getNextValue();

    samplesSinceLastCrossing += 1.0f;
    accumulatedSamples       += 1.0f;

    if (lastSample <= 0.0f && x > 0.0f)
    {
        // inputPeak decays at ~0.9998/sample, so it crosses kAmplitudeFloor
        // roughly 520 ms after the last loud sample (at 44.1 kHz).
        static constexpr float kAmplitudeFloor = 0.01f;  // ≈ −40 dBFS

        // Only count this crossing if the individual interval is in range.
        // Out-of-range means noise or a frequency outside [16 Hz, 4 kHz].
        if (samplesSinceLastCrossing >= minPeriodSamples &&
            samplesSinceLastCrossing <= maxPeriodSamples &&
            lastNegativePeak <= -gateThr)
        {
            crossingsAccum++;

            if (crossingsAccum >= skipCount)
            {
                // Only update the period estimate when input amplitude is above the
                // noise floor.  During a long envelope release the signal decays toward
                // zero — any remaining zero crossings are noise-dominated and would
                // otherwise walk the estimate to a higher frequency.
                static constexpr float kAlphaRef = 0.25f;  // ≈ −12 dBFS full-speed threshold
                if (inputPeak >= kAmplitudeFloor)
                {
                    // Scale tracking speed by signal amplitude.
                    // Full alpha at ≥ −12 dBFS; proportionally reduced below that.
                    // Prevents harmonic-drift artifacts during note decay.
                    const float alphaScale = juce::jlimit(0.0f, 1.0f, inputPeak / kAlphaRef);
                    const float maxDelta   = estimatedPeriod * 0.20f;
                    const float delta      = accumulatedSamples - estimatedPeriod;
                    estimatedPeriod += trackingAlpha * alphaScale * juce::jlimit(-maxDelta, maxDelta, delta);
                }

                accumulatedSamples = 0.0f;
                crossingsAccum     = 0;
            }
            // KEY DIFFERENCE from ZCS: reset phase only on valid crossings.
            // Phase reset is also gated by amplitude — when the signal decays below
            // the noise floor, let the phase free-run rather than resetting on each
            // crossing, which would otherwise cause pitch artifacts during note decay.
            if (inputPeak >= kAmplitudeFloor)
                currentPhase = 0.0f;
            samplesSinceLastCrossing = 0.0f;
            lastNegativePeak         = 0.0f;
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
    const float len  = smoothLength.getNextValue();

    // ── Synthesise H1 + H2-H8 ────────────────────────────────────────────
    // H1 amplitude is user-controllable — default 1.0 (full reconstruction).
    // Setting to 0 produces harmonic-only output, matching Effect mode character.
    float y = h1Amp * shapedWave(warpPhase(currentPhase, duty), step);

    // H2-H8 additive content from recipe wheel
    const float nyquist     = (float)(sampleRate * 0.5);
    const float aaFadeStart = nyquist * 0.5f;  // fade starts 1 octave below Nyquist
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;
        const float harmonicHz = (float)(i + 2) / estimatedPeriod * (float)sampleRate;
        const float aaMul      = juce::jlimit(0.0f, 1.0f,
                                   (nyquist - harmonicHz) / (nyquist - aaFadeStart));
        if (aaMul <= 0.0f) continue;
        float hp = std::fmod((float)(i + 2) * currentPhase, kTwoPi);
        y += amps[(size_t)i] * aaMul * shapedWave(warpPhase(hp, duty), step);
    }

    // Soft-clip the harmonic sum to prevent hard clipping on dense recipes.
    // tanh(y / kClipRef) / tanh(1/kClipRef) normalises so amplitude 1.0 passes
    // through unchanged while gently compressing larger sums.
    {
        static const float kClipRef  = 1.5f;
        static const float kClipNorm = std::tanh(1.0f / kClipRef);
        y = std::tanh(y / kClipRef) / kClipNorm;
    }

    // ── Length gate: silence output after len×2π of each wavelet ────────
    if (len < 1.0f)
    {
        const float gateEnd   = len * kTwoPi;
        const float fadeStart = gateEnd * 0.8f;   // cosine fade = last 20% of active zone
        if (currentPhase >= gateEnd)
            y = 0.0f;
        else if (currentPhase >= fadeStart)
        {
            const float t = (currentPhase - fadeStart) / (gateEnd - fadeStart); // 0→1
            y *= 0.5f * (1.0f + std::cos(kPi * t));  // cosine window 1→0
        }
    }
    return y;
}
