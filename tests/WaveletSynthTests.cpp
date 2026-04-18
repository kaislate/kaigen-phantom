#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/WaveletSynth.h"
#include <cmath>

static constexpr double kSR = 44100.0;

// Feed N samples of a sine at `hz` starting at phase `phaseOffset`.
// Returns final phase so callers can chain without discontinuity.
static float feedSine(WaveletSynth& syn, float hz, int numSamples, float startPhase = 0.0f)
{
    const float w = 2.0f * juce::MathConstants<float>::pi * hz / (float)kSR;
    for (int i = 0; i < numSamples; ++i)
        syn.process(std::sin(w * (float)i + startPhase));
    return std::fmod(w * (float)numSamples + startPhase, 2.0f * juce::MathConstants<float>::pi);
}

// Feed a sine whose frequency slides linearly from startHz to endHz over numSamples.
// Phase is accumulated correctly (integral of instantaneous frequency) so the
// chirp has the correct zero-crossing intervals throughout the glide.
static void feedGlide(WaveletSynth& syn, float startHz, float endHz, int numSamples)
{
    const float kTwoPi = 2.0f * juce::MathConstants<float>::pi;
    float phase = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float t  = (float)i / (float)(numSamples > 1 ? numSamples - 1 : 1);
        const float hz = startHz + (endHz - startHz) * t;
        syn.process(std::sin(phase));
        phase += kTwoPi * hz / (float)kSR;
        if (phase >= kTwoPi) phase -= kTwoPi;
    }
}

// ─── Basic / smoke ───────────────────────────────────────────────────────────

TEST_CASE("WaveletSynth: setters and prepare/reset do not crash")
{
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setWaveletLength(0.5f);
    syn.setGateThreshold(0.3f);
    syn.reset();
    syn.setWaveletLength(1.0f);
    syn.setGateThreshold(0.0f);
    SUCCEED();
}

TEST_CASE("WaveletSynth: default params produce same output as explicit defaults")
{
    WaveletSynth a, b;
    a.prepare(kSR);
    b.prepare(kSR);
    b.setWaveletLength(1.0f);
    b.setGateThreshold(0.0f);

    const float w = 2.0f * juce::MathConstants<float>::pi * 200.0f / (float)kSR;
    for (int i = 0; i < 2048; ++i)
    {
        const float x = std::sin(w * (float)i);
        REQUIRE(a.process(x) == Catch::Approx(b.process(x)).margin(1e-5f));
    }
}

// ─── Pitch convergence ───────────────────────────────────────────────────────

TEST_CASE("WaveletSynth: converges to a steady 100 Hz signal")
{
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setTrackingSpeed(0.25f);
    // Warm-up: 1 second (100 crossings at 100 Hz)
    feedSine(syn, 100.0f, (int)kSR);
    REQUIRE(syn.getEstimatedHz() == Catch::Approx(100.0f).margin(5.0f));
}

TEST_CASE("WaveletSynth: converges to a steady 200 Hz signal")
{
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setTrackingSpeed(0.25f);
    feedSine(syn, 200.0f, (int)kSR);
    REQUIRE(syn.getEstimatedHz() == Catch::Approx(200.0f).margin(8.0f));
}

TEST_CASE("WaveletSynth: converges to a steady 440 Hz signal")
{
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setTrackingSpeed(0.25f);
    feedSine(syn, 440.0f, (int)kSR);
    REQUIRE(syn.getEstimatedHz() == Catch::Approx(440.0f).margin(15.0f));
}

TEST_CASE("WaveletSynth: returns 0 when input is below amplitude floor")
{
    // getEstimatedHz() is gated — should return 0 on silence even after tracking
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setTrackingSpeed(0.25f);
    feedSine(syn, 200.0f, (int)kSR);
    REQUIRE(syn.getEstimatedHz() > 0.0f); // was tracking

    // Now feed silence long enough for inputPeak to decay below floor (~520 ms)
    for (int i = 0; i < (int)(0.6 * kSR); ++i)
        syn.process(0.0f);
    REQUIRE(syn.getEstimatedHz() == Catch::Approx(0.0f).margin(0.1f));
}

// ─── Tracking speed ──────────────────────────────────────────────────────────
// After a pitch jump, a fast tracker should converge much sooner than a slow one.
// We measure convergence by counting crossings until the estimate is within 5%.

TEST_CASE("WaveletSynth: faster tracking speed converges sooner after a pitch jump")
{
    const float freqFrom = 100.0f;
    const float freqTo   = 200.0f;
    const float target   = 200.0f;
    const float tolerance = target * 0.05f; // 5% = ±10 Hz

    auto countCrossingsToConverge = [&](float alpha) -> int
    {
        WaveletSynth syn;
        syn.prepare(kSR);
        syn.setTrackingSpeed(alpha);

        // Establish the starting pitch firmly
        feedSine(syn, freqFrom, (int)kSR);

        // Jump to new pitch; count crossings until estimate is within tolerance
        const float w = 2.0f * juce::MathConstants<float>::pi * freqTo / (float)kSR;
        const int maxSamples = (int)(2.0 * kSR); // 2 second budget
        int crossings = 0;
        float prev = 0.0f;
        for (int i = 0; i < maxSamples; ++i)
        {
            const float x = std::sin(w * (float)i);
            syn.process(x);
            if (prev <= 0.0f && x > 0.0f)
            {
                crossings++;
                if (std::abs(syn.getEstimatedHz() - target) <= tolerance)
                    return crossings;
            }
            prev = x;
        }
        return INT_MAX; // never converged
    };

    const int fastCrossings = countCrossingsToConverge(0.5f);
    const int slowCrossings = countCrossingsToConverge(0.05f);

    // Fast tracker must converge; slow tracker must take longer
    REQUIRE(fastCrossings < INT_MAX);
    REQUIRE(fastCrossings < slowCrossings);
    // Fast at alpha=0.5 should settle within ~10 crossings (~50 ms at 200 Hz)
    REQUIRE(fastCrossings <= 15);
}

// ─── Glide / portamento following ────────────────────────────────────────────

TEST_CASE("WaveletSynth: tracks a slow pitch glide within reasonable lag")
{
    // 2-octave glide (100 → 400 Hz) over 2 seconds.
    // After the glide completes, the fast tracker should be within 15% of 400 Hz.
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setTrackingSpeed(0.4f);

    feedGlide(syn, 100.0f, 400.0f, (int)(2.0 * kSR));

    REQUIRE(syn.getEstimatedHz() == Catch::Approx(400.0f).margin(400.0f * 0.15f));
}

TEST_CASE("WaveletSynth: slow tracking speed lags significantly on a pitch jump")
{
    // With alpha=0.01, after only 10 crossings the estimate should still be
    // well below the new target — this confirms the tracking speed knob works.
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setTrackingSpeed(0.01f);

    feedSine(syn, 100.0f, (int)kSR); // establish 100 Hz
    const float before = syn.getEstimatedHz();
    REQUIRE(before == Catch::Approx(100.0f).margin(5.0f));

    // Feed 20 cycles of 400 Hz
    feedSine(syn, 400.0f, (int)(20.0f / 400.0f * (float)kSR));
    const float after = syn.getEstimatedHz();

    // Should have barely moved (slow tracker)
    REQUIRE(after < 200.0f); // still much closer to 100 than 400
}

// ─── Gate threshold ──────────────────────────────────────────────────────────

TEST_CASE("WaveletSynth: waveset gate — hard gating with RMS and hysteresis")
{
    // Miya-style: hard gating on waveset RMS energy with 3dB hysteresis.
    // Entire wavesets are admitted (OPEN) or rejected (CLOSED).

    const float freq = 200.0f;
    const float w    = 2.0f * juce::MathConstants<float>::pi * freq / (float)kSR;

    // ── Loud signal (amp=0.5, RMS≈0.35) with gate at 0.3 → OPEN (0.35 > 0.3)
    WaveletSynth loud;
    loud.prepare(kSR);
    loud.setGateThreshold(0.3f);
    loud.setTrackingSpeed(0.25f);

    float loudRms = 0.0f;
    const int warmup = (int)(0.5 * kSR);
    feedSine(loud, freq, warmup);  // warm up
    const int N1 = (int)(0.5 * kSR);
    for (int i = 0; i < N1; ++i)
    {
        const float y = loud.process(0.5f * std::sin(w * (float)(i + warmup)));
        loudRms += y * y;
    }
    loudRms = std::sqrt(loudRms / (float)N1);
    // Gate should be open — RMS of 0.5 sine ≈ 0.35 > threshold 0.3
    REQUIRE(loudRms > 0.3f);

    // ── Quiet signal (amp=0.05, RMS≈0.035) with gate at 0.3 → CLOSED
    WaveletSynth quiet;
    quiet.prepare(kSR);
    quiet.setGateThreshold(0.3f);
    quiet.setTrackingSpeed(0.25f);

    // Skip first few wavesets for gate state to settle
    const int skip = (int)(0.05 * kSR);
    for (int i = 0; i < skip; ++i)
        quiet.process(0.05f * std::sin(w * (float)i));

    float quietRms = 0.0f;
    const int N2 = (int)(0.5 * kSR);
    for (int i = 0; i < N2; ++i)
    {
        const float y = quiet.process(0.05f * std::sin(w * (float)(i + skip)));
        quietRms += y * y;
    }
    quietRms = std::sqrt(quietRms / (float)N2);
    // Gate should be CLOSED — RMS 0.035 < closeThr 0.3*0.71 = 0.213
    REQUIRE(quietRms < 0.02f);
    // Pitch tracking still works regardless of gate state
    REQUIRE(quiet.getEstimatedHz() == Catch::Approx(200.0f).margin(15.0f));
}

// ─── Wavelet length ───────────────────────────────────────────────────────────

TEST_CASE("WaveletSynth: reduced length reduces output RMS")
{
    auto measureRMS = [](float length, float freq) -> float {
        WaveletSynth syn;
        syn.prepare(kSR);
        syn.setWaveletLength(length);
        syn.setTrackingSpeed(0.3f);
        feedSine(syn, freq, (int)(0.1 * kSR));  // warm-up
        float sum = 0.0f;
        const int N = (int)(0.2 * kSR);
        const float w = 2.0f * juce::MathConstants<float>::pi * freq / (float)kSR;
        for (int i = 0; i < N; ++i)
        {
            const float y = syn.process(std::sin(w * (float)(i + (int)(0.1 * kSR))));
            sum += y * y;
        }
        return std::sqrt(sum / (float)N);
    };

    const float rms_full = measureRMS(1.0f, 200.0f);
    const float rms_10pct = measureRMS(0.1f, 200.0f);

    REQUIRE(rms_full  > 0.01f);            // sanity: synth produced output
    REQUIRE(rms_10pct < rms_full * 0.5f);  // 10% length → less than half the energy
}

// ─── Skip count ──────────────────────────────────────────────────────────────

TEST_CASE("WaveletSynth: skip=1 estimates period at twice the fundamental period")
{
    // With skip=1, crossingsAccum fires when crossingsAccum > 1 (i.e. after every
    // 2nd crossing).  At 200 Hz (period=220.5 samples), accumulatedSamples spans
    // 2 crossings ≈ 441 samples.  estimatedPeriod converges to ~441, so
    // getEstimatedHz() = sr/estimatedPeriod ≈ 100 Hz (sub-octave of input).
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setSkipCount(1);
    syn.setTrackingSpeed(0.3f);
    feedSine(syn, 200.0f, (int)kSR);

    // getEstimatedHz() with skip=1 should report ~100 Hz (= 200/2)
    REQUIRE(syn.getEstimatedHz() == Catch::Approx(100.0f).margin(8.0f));
}

TEST_CASE("WaveletSynth: setSkipCount resets currentWaveletPeak to avoid Punch spike")
{
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setSkipCount(1);
    feedSine(syn, 200.0f, (int)(0.1 * kSR));

    // peak should be non-zero after processing
    REQUIRE(syn.getWaveletPeak() > 0.0f);

    // Changing skip should zero it — prevents a spike on the first new-interval latch
    syn.setSkipCount(2);
    // getWaveletPeak() returns lastWaveletPeak which was latched before the skip change.
    // What we're really testing is that currentWaveletPeak (internal) doesn't carry
    // a stale inflated value. We verify this indirectly: after 1 full interval at
    // skip=2, the peak should not exceed the expected signal amplitude (~1.0).
    feedSine(syn, 200.0f, (int)(2.0f / 200.0f * (float)kSR) + 10); // ~2 cycles
    REQUIRE(syn.getWaveletPeak() <= 1.05f); // should not exceed signal amplitude
}

TEST_CASE("WaveletSynth: minFreq rejects crossings below the floor")
{
    WaveletSynth syn;
    syn.prepare(kSR);
    // 100 Hz at 44100 = 441 samples/period.  Set max period to 441 so crossings
    // slower than 100 Hz (longer period) are rejected.
    syn.setMaxPeriodSamples(441.0f);
    syn.setTrackingSpeed(0.25f);

    // Feed 50 Hz sine (below the 100 Hz floor) for 1 second
    feedSine(syn, 50.0f, (int)kSR);

    // Crossings at 50 Hz have intervals of ~882 samples, exceeding maxPeriodSamples
    // (= 44100/100 = 441). So all crossings are rejected and the estimate stays at the
    // default 100 Hz.
    REQUIRE(syn.getEstimatedHz() == Catch::Approx(100.0f).margin(5.0f));
}

TEST_CASE("WaveletSynth: sub-sample interpolation improves pitch accuracy")
{
    // At 44100 Hz, a 100 Hz signal has period = 441.0 samples exactly.
    // Without interpolation, snapping to integer boundaries introduces
    // up to 1 sample of error per crossing. With interpolation, the
    // fractional offset is accounted for.
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setTrackingSpeed(0.25f);

    feedSine(syn, 100.0f, (int)(2.0 * kSR));  // 2 seconds for stable estimate

    // With interpolation, the estimate should be within ~1 Hz of 100 Hz.
    // (Without interpolation the margin needed to be 5 Hz.)
    REQUIRE(syn.getEstimatedHz() == Catch::Approx(100.0f).margin(1.0f));
}

TEST_CASE("WaveletSynth: boost increases output RMS when wavelet peak exceeds threshold")
{
    auto measureRMS = [](float boostThr, float boostAmt) -> float {
        WaveletSynth syn;
        syn.prepare(kSR);
        syn.setTrackingSpeed(0.25f);
        syn.setBoostThreshold(boostThr);
        syn.setBoostAmount(boostAmt);

        // Warm up
        feedSine(syn, 200.0f, (int)(0.5 * kSR));

        // Measure
        float sum = 0.0f;
        const int N = (int)(0.5 * kSR);
        const float w = 2.0f * juce::MathConstants<float>::pi * 200.0f / (float)kSR;
        for (int i = 0; i < N; ++i)
        {
            const float y = syn.process(std::sin(w * (float)(i + (int)(0.5 * kSR))));
            sum += y * y;
        }
        return std::sqrt(sum / (float)N);
    };

    const float rmsNoBoost = measureRMS(0.0f, 0.0f);   // boost off
    const float rmsBoosted = measureRMS(0.3f, 1.0f);    // threshold 30%, boost 100% (+1x)

    // Wavelet peak is ~1.0 (full sine), well above 30% threshold.
    // Boost gain = 1.0 + 1.0 = 2.0 -> output should be ~2x louder.
    REQUIRE(rmsBoosted > rmsNoBoost * 1.5f);
}

TEST_CASE("WaveletSynth: boost with threshold=0 (off) produces no extra gain")
{
    // When boost threshold is 0 (default), boost should never fire regardless
    // of input level — lastBoostGain stays at 1.0.
    auto measureRMS = [](float boostThr, float boostAmt) -> float {
        WaveletSynth syn;
        syn.prepare(kSR);
        syn.setTrackingSpeed(0.25f);
        syn.setBoostThreshold(boostThr);
        syn.setBoostAmount(boostAmt);

        feedSine(syn, 200.0f, (int)(0.5 * kSR));

        float sum = 0.0f;
        const int N = (int)(0.5 * kSR);
        const float w = 2.0f * juce::MathConstants<float>::pi * 200.0f / (float)kSR;
        for (int i = 0; i < N; ++i)
        {
            const float y = syn.process(std::sin(w * (float)(i + (int)(0.5 * kSR))));
            sum += y * y;
        }
        return std::sqrt(sum / (float)N);
    };

    const float rmsOff  = measureRMS(0.0f, 1.0f);   // threshold=0 → boost disabled
    const float rmsNone = measureRMS(0.0f, 0.0f);   // both off

    // With threshold=0 the boost should never fire, so output is identical
    REQUIRE(rmsOff == Catch::Approx(rmsNone).margin(0.01f));
}
