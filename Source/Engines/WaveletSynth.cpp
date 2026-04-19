#include "WaveletSynth.h"
#include <cmath>

static constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
static constexpr float kPi    = juce::MathConstants<float>::pi;

// ── Setup ──────────────────────────────────────────────────────────────────

void WaveletSynth::prepare(double sr) noexcept
{
    sampleRate = sr;
    // minPeriodSamples / maxPeriodSamples are configured via setMinPeriodSamples /
    // setMaxPeriodSamples (called from syncParamsToEngine after prepareToPlay).
    // Member defaults handle the brief window before the first sync.

    const double rampSec = 0.010; // 10 ms parameter smoothing
    // reset(sr, rampSec) recalculates ramp duration without touching the
    // current/target value — parameter state survives repeated prepareToPlay()
    // calls (e.g. Ableton transport loop).  setCurrentAndTargetValue is NOT
    // called here so step/duty/length/gate are preserved across DAW restarts.
    smoothStep  .reset(sr, rampSec);
    smoothDuty  .reset(sr, rampSec);
    smoothLength.reset(sr, rampSec);
    smoothGate  .reset(sr, rampSec);
    smoothBoostThr.reset(sr, rampSec);
    smoothBoostAmt.reset(sr, rampSec);
    gateGainSmooth.reset(sr, 0.005); // 5 ms — smooths gate open/close transitions

    reset();
}

void WaveletSynth::reset() noexcept
{
    lastSample               = 0.0f;
    samplesSinceLastCrossing = 0.0f;
    accumulatedSamples       = 0.0f;
    crossingsAccum           = 0;
    currentPhase             = kTwoPi;  // start silent; reset to 0 at first valid crossing
    estimatedPeriod          = (float)(sampleRate / 100.0); // 100 Hz safe default
    inputPeak                = 0.0f;
    currentWaveletPeak       = 0.0f;
    lastWaveletPeak          = 0.0f;
    currentWaveletSumSq      = 0.0f;
    currentWaveletSampleCount = 0;
    lastWaveletRMS           = 0.0f;
    gateOpen                 = true;
    gateGainSmooth.setCurrentAndTargetValue(1.0f);
    targetBoostGain   = 1.0f;
    smoothedBoostGain = 1.0f;
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
    trackingAlpha = juce::jlimit(0.001f, 1.0f, speed);
}

void WaveletSynth::setMinPeriodSamples(float samples) noexcept
{
    minPeriodSamples = juce::jlimit(2.0f, 500.0f, samples);
}

void WaveletSynth::setMaxPeriodSamples(float samples) noexcept
{
    maxPeriodSamples = juce::jlimit(100.0f, 8000.0f, samples);
}

void WaveletSynth::setH1Amplitude(float amp) noexcept
{
    h1Amp = juce::jlimit(0.0f, 2.0f, amp);
}

void WaveletSynth::setSubAmplitude(float amp) noexcept
{
    subAmp = juce::jlimit(0.0f, 2.0f, amp);
}

void WaveletSynth::setBoostThreshold(float thr) noexcept
{
    smoothBoostThr.setTargetValue(juce::jlimit(0.0f, 1.0f, thr));
}

void WaveletSynth::setBoostAmount(float amt) noexcept
{
    smoothBoostAmt.setTargetValue(juce::jlimit(0.0f, 2.0f, amt));
}

void WaveletSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(0, 8, n);  // 0 = muted
    if (newSkip != skipCount)
    {
        // Scale accumulated samples proportionally so the EMA has a warm start.
        // e.g. going from skip=1 to skip=2: double the accumulation so the
        // first new measurement lands near the current estimate.
        // Guard: don't scale when either side is 0 (would div-by-zero or produce nothing).
        if (newSkip > 0 && skipCount > 0 && accumulatedSamples > 0.0f)
            accumulatedSamples *= (float)newSkip / (float)skipCount;
        else
            accumulatedSamples = 0.0f;
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
    // RMS accumulation for gate (energy per waveset interval).
    currentWaveletSumSq += x * x;
    currentWaveletSampleCount++;

    // Advance gate smoother every sample (must not skip for correct ramp behaviour).
    const float rawGateVal = smoothGate.getNextValue();
    const float rawBoostThr = smoothBoostThr.getNextValue();
    const float rawBoostAmt = smoothBoostAmt.getNextValue();

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

            if (crossingsAccum > skipCount)
            {
                // ── Period estimation ────────────────────────────────────────
                static constexpr float kAlphaRef = 0.25f;
                if (inputPeak >= kAmplitudeFloor)
                {
                    const float alphaScale = juce::jlimit(0.0f, 1.0f, inputPeak / kAlphaRef);
                    const float maxDelta   = estimatedPeriod * 0.50f;
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

                // ── Latch waveset RMS energy ─────────────────────────────────
                lastWaveletRMS = (currentWaveletSampleCount > 0)
                    ? std::sqrt(currentWaveletSumSq / (float)currentWaveletSampleCount)
                    : 0.0f;
                currentWaveletSumSq      = 0.0f;
                currentWaveletSampleCount = 0;

                // ── Waveset gate (Miya Architecture A) ──────────────────────
                // Hysteresis (~3 dB ≈ factor 0.71) prevents rapid flapping.
                // Gate state drives gateGainSmooth; actual silence is a 5 ms
                // fade rather than a hard cut to eliminate click artefacts.
                //   0% knob → gate off (always open)
                //   RMS > threshold → OPEN
                //   RMS < threshold * 0.71 → CLOSED
                //   in between → maintain previous state
                if (rawGateVal <= 0.0f)
                {
                    gateOpen = true;
                }
                else
                {
                    static constexpr float kHysteresis = 0.71f;  // ~3 dB below threshold
                    const float openThr  = rawGateVal;
                    const float closeThr = rawGateVal * kHysteresis;
                    if (lastWaveletRMS >= openThr)
                        gateOpen = true;
                    else if (lastWaveletRMS < closeThr)
                        gateOpen = false;
                    // else: maintain previous state (hysteresis band)
                }
                gateGainSmooth.setTargetValue(gateOpen ? 1.0f : 0.0f);

                // ── Upward expansion (Miya-style Threshold & Boost) ─────────
                // Sets targetBoostGain at each wavelet boundary; smoothedBoostGain
                // tracks it per-sample to avoid step discontinuities.
                if (rawBoostThr <= 0.0f || lastWaveletPeak < rawBoostThr * inputPeak)
                    targetBoostGain = 1.0f;
                else
                    targetBoostGain = 1.0f + rawBoostAmt;
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
    // Phase is NOT wrapped. Once it passes kTwoPi the wavelet window below
    // silences output until the next valid crossing resets it to ~0. This
    // makes synthesis naturally burst-like (discrete wavesets), matching
    // Miya Architecture A behaviour.
    const float safePeriod = juce::jlimit(minPeriodSamples, maxPeriodSamples, estimatedPeriod);
    currentPhase += kTwoPi / safePeriod;

    // ── Advance shape smoothers (one step per audio sample) ──────────────
    const float step = smoothStep.getNextValue();
    const float duty = smoothDuty.getNextValue();
    const float len  = smoothLength.getNextValue();

    // Free-run: wrap phase when it exceeds the wavelet window so synthesis
    // continues at the last known period. Normal mode lets phase run past
    // gateEnd so the wavelet silences until a new zero crossing.
    if (freeRun)
    {
        const float gateEnd = len * kTwoPi;
        if (currentPhase >= gateEnd && gateEnd > 0.0f)
            currentPhase = std::fmod(currentPhase, gateEnd);
    }


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
    // H1 is a pure sine at the fundamental — not shaped by Step or Push.
    // This matches Miya's additive model: each partial is sin(n×φ), so the
    // fundamental is always a clean sine regardless of shape settings.
    // It also acts as a "glue" layer under heavy Step values since it stays
    // smooth while H2-H8 are pushed toward square/harsh shapes.
    y += h1Amp * std::sin(currentPhase);

    // ── Sub-harmonic (H-1): one octave below the fundamental ──────────
    // Phase at half the fundamental frequency = currentPhase / 2.
    // Needs to wrap within [0, 2pi] independently.
    if (subAmp > 0.0f)
    {
        const float subPhase = std::fmod(currentPhase * 0.5f, kTwoPi);
        y += subAmp * shapedWave(warpPhase(subPhase, duty), step);
    }

    // ── Smoothed gate gain + boost gain ─────────────────────────────────
    // Gate ramps 0↔1 over 5 ms (set in prepare) — no hard clicks on open/close.
    // Boost gain one-pole smooths between wavesets (~1 ms) instead of stepping.
    const float gateGain = gateGainSmooth.getNextValue();
    smoothedBoostGain += 0.05f * (targetBoostGain - smoothedBoostGain);
    y *= gateGain * smoothedBoostGain;

    // ── Wavelet window: silence once phase exceeds the active zone ───────
    // Phase runs past 2π (no wrap) and stays there until the next valid crossing
    // resets it.  len=1.0 means the full cycle plays (gateEnd = 2π). len<1.0
    // truncates earlier with a cosine fade over the last 20% of the active zone.
    // A matching cosine fade-in over the first 10% eliminates the leading click
    // that occurred when each new wavelet started at full amplitude.
    // In free-run mode, phase has already been wrapped above, so this branch
    // only silences when the wavelet has naturally ended and no crossing
    // has reset phase.
    {
        const float gateEnd   = len * kTwoPi;
        const float fadeStart = gateEnd * 0.80f;
        const float fadeInEnd = gateEnd * 0.10f;
        if (currentPhase >= gateEnd)
            return 0.0f;
        else if (currentPhase >= fadeStart)
        {
            const float t = (currentPhase - fadeStart) / (gateEnd - fadeStart); // 0→1
            y *= 0.5f * (1.0f + std::cos(kPi * t));  // cosine 1→0
        }
        else if (currentPhase < fadeInEnd && fadeInEnd > 0.0f)
        {
            const float t = currentPhase / fadeInEnd;              // 0→1
            y *= 0.5f * (1.0f - std::cos(kPi * t));               // cosine 0→1
        }
    }
    return y;
}
