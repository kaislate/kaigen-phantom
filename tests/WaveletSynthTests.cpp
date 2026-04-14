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

TEST_CASE("WaveletSynth: Miya-style amplitude gate with soft knee")
{
    // Gate operates on wavelet peak amplitude (not raw signal excursion).
    // Threshold is normalised to inputPeak; soft knee (k=0.75) provides smooth transitions.
    // Phase resets always fire — gate only attenuates the synthesised output.

    // ── Full-amplitude sine: wavelet peak ≈ inputPeak → gate gain = 1 at any threshold <1
    WaveletSynth fullSig;
    fullSig.prepare(kSR);
    fullSig.setGateThreshold(0.9f);
    fullSig.setTrackingSpeed(0.25f);

    const float freq = 200.0f;
    const float amp  = 0.5f;
    const float w    = 2.0f * juce::MathConstants<float>::pi * freq / (float)kSR;
    float lastOut = 0.0f;
    for (int i = 0; i < (int)(kSR); ++i)
    {
        lastOut = fullSig.process(amp * std::sin(w * (float)i));
    }
    // Pitch always tracked regardless of gate.
    REQUIRE(fullSig.getEstimatedHz() == Catch::Approx(200.0f).margin(15.0f));
    // Wavelet peak ≈ amp, threshold = 0.9 * amp → peak > threshold → gain = 1
    REQUIRE(fullSig.getWaveletPeak() > 0.0f);

    // ── Quiet signal (10% of previous amplitude): should be gated at threshold 0.9
    WaveletSynth quietSig;
    quietSig.prepare(kSR);
    quietSig.setGateThreshold(0.9f);
    quietSig.setTrackingSpeed(0.25f);

    // First establish a loud signal to set inputPeak high
    for (int i = 0; i < (int)(0.5 * kSR); ++i)
        quietSig.process(amp * std::sin(w * (float)i));

    // Immediately switch to 10% amplitude. inputPeak is still ~0.5 (decays slowly).
    // threshold = 0.9 * ~0.5 = ~0.45, lowerBound = ~0.34
    // wavelet peak ~0.05 < lowerBound → gate gain = 0
    // Skip the first 20ms (one wavelet period) where lastGateGain is still 1.0
    // from the loud section — the gate only updates at crossing points.
    const int skip = (int)(0.02 * kSR);
    for (int i = 0; i < skip; ++i)
        quietSig.process(0.05f * std::sin(w * (float)i));

    float rms = 0.0f;
    const int N = (int)(0.08 * kSR);
    for (int i = 0; i < N; ++i)
    {
        const float y = quietSig.process(0.05f * std::sin(w * (float)(i + skip)));
        rms += y * y;
    }
    rms = std::sqrt(rms / (float)N);
    // Gate should have heavily attenuated the output after the first wavelet.
    REQUIRE(rms < 0.02f);
    // But pitch tracking still works — gate doesn't block phase resets or period estimation.
    REQUIRE(quietSig.getEstimatedHz() == Catch::Approx(200.0f).margin(15.0f));
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

TEST_CASE("WaveletSynth: skip=2 estimates period at twice the fundamental period")
{
    // With skip=2 at 200 Hz (period=220.5 samples), accumulatedSamples spans
    // 2 crossings ≈ 441 samples.  estimatedPeriod converges to ~441, so
    // getEstimatedHz() = sr/estimatedPeriod ≈ 100 Hz (sub-octave of input).
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setSkipCount(2);
    syn.setTrackingSpeed(0.3f);
    feedSine(syn, 200.0f, (int)kSR);

    // getEstimatedHz() with skip=2 should report ~100 Hz (= 200/2)
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
    syn.setMinFreqHz(100.0f);  // reject anything below 100 Hz
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
