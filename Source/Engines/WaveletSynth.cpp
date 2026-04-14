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
    maxPeriodSamples = (float)(sr / (double)minFreqHz);

    const double rampSec = 0.010; // 10 ms parameter smoothing
    // reset(sr, rampSec) recalculates ramp duration without touching the
    // current/target value — parameter state survives repeated prepareToPlay()
    // calls (e.g. Ableton transport loop).  setCurrentAndTargetValue is NOT
    // called here so step/duty/length/gate are preserved across DAW restarts.
    smoothStep  .reset(sr, rampSec);
    smoothDuty  .reset(sr, rampSec);
    smoothLength.reset(sr, rampSec);
    smoothGate  .reset(sr, rampSec);

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
    inputPeak                = 0.0f;
    currentWaveletPeak       = 0.0f;
    lastWaveletPeak          = 0.0f;
    lastGateGain             = 1.0f;
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
    trackingAlpha = juce::jlimit(0.001f, 0.8f, speed);
}

void WaveletSynth::setMaxTrackHz(float hz) noexcept
{
    maxTrackHz       = juce::jlimit(200.0f, 20000.0f, hz);
    minPeriodSamples = (float)(sampleRate / (double)maxTrackHz);
}

void WaveletSynth::setMinFreqHz(float hz) noexcept
{
    minFreqHz        = juce::jlimit(8.0f, 200.0f, hz);
    maxPeriodSamples = (float)(sampleRate / (double)minFreqHz);
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
        skipCount            = newSkip;
        crossingsAccum       = 0;
        currentWaveletPeak   = 0.0f;   // discard partial-interval peak so Punch doesn't spike
        // estimatedPeriod is intentionally NOT reset
    }
}

float WaveletSynth::getEstimatedHz() const noexcept
{
    // Return 0 when the input is below the noise floor — callers can show "---" in that case.
    if (inputPeak < 0.01f) return 0.0f;
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
    // Per-wavelet peak: track max |x| within current cycle; latch on valid crossing.
    if (absX > currentWaveletPeak) currentWaveletPeak = absX;
    // Decay lastWaveletPeak alongside inputPeak so Punch doesn't hold a stale
    // amplitude during silence and cause self-oscillation.
    lastWaveletPeak *= 0.9998f;

    // Advance gate smoother every sample (must not skip for correct ramp behaviour).
    const float rawGateVal = smoothGate.getNextValue();

    samplesSinceLastCrossing += 1.0f;
    accumulatedSamples       += 1.0f;

    if (lastSample <= 0.0f && x > 0.0f)
    {
        // ── Sub-sample interpolation ────────────────────────────────────
        // Linear interpolation gives the fractional offset (0..1) from the
        // previous sample to the true zero crossing. Correcting the timing
        // accumulators eliminates up to 1 sample of jitter per crossing.
        const float denom    = x - lastSample;
        const float fraction = (std::abs(denom) > 1e-12f) ? (-lastSample / denom) : 0.5f;
        const float correction = 1.0f - fraction;
        samplesSinceLastCrossing -= correction;
        accumulatedSamples       -= correction;

        // inputPeak decays at ~0.9998/sample, so it crosses kAmplitudeFloor
        // roughly 520 ms after the last loud sample (at 44.1 kHz).
        static constexpr float kAmplitudeFloor = 0.01f;  // ≈ −40 dBFS

        // Only count this crossing if the individual interval is in range.
        // Out-of-range means noise or a frequency outside [16 Hz, 4 kHz].
        if (samplesSinceLastCrossing >= minPeriodSamples &&
            samplesSinceLastCrossing <= maxPeriodSamples)
        {
            crossingsAccum++;

            if (crossingsAccum >= skipCount)
            {
                // ── Period estimation ────────────────────────────────────────
                static constexpr float kAlphaRef = 0.25f;
                if (inputPeak >= kAmplitudeFloor)
                {
                    const float alphaScale = juce::jlimit(0.0f, 1.0f, inputPeak / kAlphaRef);
                    const float maxDelta   = estimatedPeriod * 0.20f;
                    const float delta      = accumulatedSamples - estimatedPeriod;
                    estimatedPeriod += trackingAlpha * alphaScale * juce::jlimit(-maxDelta, maxDelta, delta);
                }

                accumulatedSamples = 0.0f;
                crossingsAccum     = 0;

                // ── Phase reset (always — gate controls gain, not firing) ────
                if (inputPeak >= kAmplitudeFloor)
                {
                    const float resetPeriod = juce::jlimit(minPeriodSamples, maxPeriodSamples, estimatedPeriod);
                    currentPhase = correction * (kTwoPi / resetPeriod);
                }

                // ── Latch wavelet peak (always) ──────────────────────────────
                lastWaveletPeak    = currentWaveletPeak;
                currentWaveletPeak = 0.0f;

                // ── Miya-style amplitude gate ────────────────────────────────
                // Gate operates on the completed wavelet's peak amplitude, not on
                // the raw signal's negative excursion.  Soft knee (k=0.75) gives
                // smooth transitions instead of binary pass/fail.
                //   0% knob → gate off (gain=1)
                //   at threshold: wavelets whose peak < threshold×inputPeak are attenuated
                //   below threshold×0.75: fully silenced
                if (rawGateVal <= 0.0f)
                {
                    lastGateGain = 1.0f;
                }
                else
                {
                    const float threshold  = rawGateVal * inputPeak;
                    static constexpr float kKnee = 0.75f;
                    const float lowerBound = threshold * kKnee;
                    if (lastWaveletPeak >= threshold)
                        lastGateGain = 1.0f;
                    else if (lastWaveletPeak < lowerBound)
                        lastGateGain = 0.0f;
                    else
                        lastGateGain = (lastWaveletPeak - lowerBound)
                                     / (threshold - lowerBound);
                }
            }
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
    // Clamp period to valid range before dividing — prevents NaN/inf if the
    // EMA somehow drifts out of bounds (e.g., first block before any crossings).
    const float safePeriod = juce::jlimit(minPeriodSamples, maxPeriodSamples, estimatedPeriod);
    currentPhase += kTwoPi / safePeriod;
    if (currentPhase >= kTwoPi)
        currentPhase -= kTwoPi;

    // ── Advance shape smoothers (one step per audio sample) ──────────────
    const float step = smoothStep.getNextValue();
    const float duty = smoothDuty.getNextValue();
    const float len  = smoothLength.getNextValue();

    // ── Synthesise H2-H8 ─────────────────────────────────────────────────
    // H2-H8 additive content from recipe wheel, soft-clipped as a group.
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
        float hp = std::fmod((float)(i + 2) * currentPhase, kTwoPi);
        y += amps[(size_t)i] * aaMul * shapedWave(warpPhase(hp, duty), step);
    }

    // Soft-clip only the H2-H8 harmonic sum to prevent hard clipping on dense recipes.
    // tanh(y / kClipRef) / tanh(1/kClipRef) normalises so amplitude 1.0 passes
    // through unchanged while gently compressing larger sums.
    {
        static const float kClipRef  = 1.5f;
        static const float kClipNorm = std::tanh(1.0f / kClipRef);
        y = std::tanh(y / kClipRef) / kClipNorm;
    }

    // ── Add H1 (fundamental) after harmonic soft-clip ─────────────────
    // H1 is kept outside the harmonic soft-clip so its effect on the output
    // is independent of how many harmonics are active. At h1Amp=0 you get
    // pure harmonic content; at h1Amp=1 the fundamental always contributes
    // a full unit-amplitude cycle regardless of recipe density.
    y += h1Amp * shapedWave(warpPhase(currentPhase, duty), step);

    // ── Amplitude gate: scale wavelet by gate gain (Miya-style) ────────
    y *= lastGateGain;

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
