#include "ZeroCrossingSynth.h"
#include <cmath>

static constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
static constexpr float kPi    = juce::MathConstants<float>::pi;

// ── Setup ──────────────────────────────────────────────────────────────────

void ZeroCrossingSynth::prepare(double sr) noexcept
{
    sampleRate = sr;

    // Valid period range: 16 Hz (sub) to maxTrackHz
    minPeriodSamples = (float)(sr / (double)maxTrackHz);
    maxPeriodSamples = (float)(sr / (double)minFreqHz);

    const double rampSec = 0.010; // 10 ms parameter smoothing
    // Preserve current/target values across repeated prepareToPlay() calls.
    smoothStep.reset(sr, rampSec);
    smoothDuty.reset(sr, rampSec);

    reset();
}

void ZeroCrossingSynth::reset() noexcept
{
    lastSample               = 0.0f;
    samplesSinceLastCrossing = 0.0f;
    accumulatedSamples       = 0.0f;
    crossingsAccum           = 0;
    fundamentalPhase         = 0.0f;
    estimatedPeriod          = (float)(sampleRate / 100.0); // 100 Hz safe default
    inputPeak                = 0.0f;
    currentWaveletPeak       = 0.0f;
    lastWaveletPeak          = 0.0f;
}

// ── Parameter setters ──────────────────────────────────────────────────────

void ZeroCrossingSynth::setHarmonicAmplitudes(const std::array<float, 7>& newAmps) noexcept
{
    for (int i = 0; i < 7; ++i)
        amps[(size_t)i] = juce::jlimit(0.0f, 1.0f, newAmps[(size_t)i]);
}

void ZeroCrossingSynth::setStep(float s) noexcept
{
    smoothStep.setTargetValue(juce::jlimit(0.0f, 1.0f, s));
}

void ZeroCrossingSynth::setDutyCycle(float d) noexcept
{
    smoothDuty.setTargetValue(juce::jlimit(0.05f, 0.95f, d));
}

void ZeroCrossingSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(1, 8, n);
    if (newSkip != skipCount)
    {
        // Scale accumulated samples proportionally so the EMA has a warm start.
        if (accumulatedSamples > 0.0f)
            accumulatedSamples *= (float)newSkip / (float)skipCount;
        skipCount      = newSkip;
        crossingsAccum = 0;
        // estimatedPeriod is intentionally NOT reset
    }
}

void ZeroCrossingSynth::setTrackingSpeed(float speed) noexcept
{
    trackingAlpha = juce::jlimit(0.001f, 0.80f, speed);
}

void ZeroCrossingSynth::setMaxTrackHz(float hz) noexcept
{
    maxTrackHz       = juce::jlimit(200.0f, 20000.0f, hz);
    minPeriodSamples = (float)(sampleRate / (double)maxTrackHz);
}

void ZeroCrossingSynth::setMinFreqHz(float hz) noexcept
{
    minFreqHz        = juce::jlimit(8.0f, 200.0f, hz);
    maxPeriodSamples = (float)(sampleRate / (double)minFreqHz);
}

float ZeroCrossingSynth::getEstimatedHz() const noexcept
{
    // Return 0 when the input is below the noise floor — callers can show "---" in that case.
    if (inputPeak < 0.01f) return 0.0f;
    return estimatedPeriod > 0.0f ? (float)(sampleRate / (double)estimatedPeriod) : 0.0f;
}

// ── Waveform helpers ───────────────────────────────────────────────────────

float ZeroCrossingSynth::warpPhase(float phase, float duty) noexcept
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

float ZeroCrossingSynth::shapedWave(float wp, float step) noexcept
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

float ZeroCrossingSynth::process(float x) noexcept
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
    // Fast-attack / slow-release amplitude tracker (~78 ms half-life at 44.1 kHz).
    const float absX = std::abs(x);
    inputPeak = (absX > inputPeak) ? absX : inputPeak * 0.9998f;
    // Per-wavelet peak: track max |x| within current crossing interval.
    if (absX > currentWaveletPeak) currentWaveletPeak = absX;
    // Decay lastWaveletPeak alongside inputPeak so Punch doesn't hold a stale
    // amplitude during silence and cause self-oscillation.
    lastWaveletPeak *= 0.9998f;

    samplesSinceLastCrossing += 1.0f;
    accumulatedSamples       += 1.0f;

    if (lastSample <= 0.0f && x > 0.0f)
    {
        // ── Sub-sample interpolation ────────────────────────────────────
        const float denom    = x - lastSample;
        const float fraction = (std::abs(denom) > 1e-12f) ? (-lastSample / denom) : 0.5f;
        const float correction = 1.0f - fraction;
        samplesSinceLastCrossing -= correction;
        accumulatedSamples       -= correction;

        // Only count this crossing if the individual interval is in range.
        // Out-of-range means noise or a frequency outside [16 Hz, 4 kHz].
        if (samplesSinceLastCrossing >= minPeriodSamples &&
            samplesSinceLastCrossing <= maxPeriodSamples)
        {
            crossingsAccum++;

            if (crossingsAccum >= skipCount)
            {
                // Scale tracking speed by signal amplitude.
                // Full alpha at ≥ −12 dBFS (0.25); proportionally reduced below that.
                // Prevents harmonic-content changes during note decay from walking
                // the period estimate to a wrong frequency.
                // Hard floor at −40 dBFS: stop updating entirely once signal is noise.
                static constexpr float kAmplitudeFloor = 0.01f;  // ≈ −40 dBFS
                static constexpr float kAlphaRef       = 0.25f;  // ≈ −12 dBFS full-speed threshold
                if (inputPeak >= kAmplitudeFloor)
                {
                    const float alphaScale = juce::jlimit(0.0f, 1.0f, inputPeak / kAlphaRef);
                    const float maxDelta   = estimatedPeriod * 0.20f;
                    const float delta      = accumulatedSamples - estimatedPeriod;
                    estimatedPeriod += trackingAlpha * alphaScale * juce::jlimit(-maxDelta, maxDelta, delta);
                }

                accumulatedSamples = 0.0f;
                crossingsAccum     = 0;
            }
            // Latch wavelet peak for Punch feature: capture peak amplitude of completed interval.
            lastWaveletPeak    = currentWaveletPeak;
            currentWaveletPeak = 0.0f;
        }
        else
        {
            // Invalid crossing — reset accumulation to avoid poisoned measurements.
            accumulatedSamples = 0.0f;
            crossingsAccum     = 0;
        }

        samplesSinceLastCrossing = 0.0f;
    }
    lastSample = x;

    // ── Phase advance ────────────────────────────────────────────────────
    // Clamp period to valid range before dividing — prevents NaN/inf if the
    // EMA somehow drifts out of bounds (e.g., first block before any crossings).
    const float safePeriod = juce::jlimit(minPeriodSamples, maxPeriodSamples, estimatedPeriod);
    fundamentalPhase += kTwoPi / safePeriod;
    if (fundamentalPhase >= kTwoPi)
        fundamentalPhase -= kTwoPi;

    // ── Advance shape smoothers (one step per audio sample) ──────────────
    const float step = smoothStep.getNextValue();
    const float duty = smoothDuty.getNextValue();

    // ── Synthesise H2-H8 ─────────────────────────────────────────────────
    // Each harmonic n advances at n × the fundamental phase.
    // warpPhase applies PWM distortion; shapedWave applies sine→square morph.
    // The fundamental (n=1) is never synthesised — only integer multiples ≥ 2.
    float y = 0.0f;
    const float nyquist     = (float)(sampleRate * 0.5);
    const float aaFadeStart = nyquist * 0.5f;  // fade starts 1 octave below Nyquist
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;
        const float harmonicHz = (float)(i + 2) / estimatedPeriod * (float)sampleRate;
        const float aaMul      = juce::jlimit(0.0f, 1.0f,
                                   (nyquist - harmonicHz) / (nyquist - aaFadeStart));
        if (aaMul <= 0.0f) continue;

        float hp = std::fmod((float)(i + 2) * fundamentalPhase, kTwoPi);
        y += amps[(size_t)i] * aaMul * shapedWave(warpPhase(hp, duty), step);
    }

    // Soft-clip harmonic sum — prevents hard clipping on dense recipes.
    // Amplitude 1.0 passes through unchanged; larger sums are gently compressed.
    {
        static const float kClipRef  = 1.5f;
        static const float kClipNorm = std::tanh(1.0f / kClipRef);
        y = std::tanh(y / kClipRef) / kClipNorm;
    }

    return y;
}
