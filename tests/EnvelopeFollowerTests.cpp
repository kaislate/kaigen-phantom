#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/EnvelopeFollower.h"
#include <cmath>

// ─── helpers ────────────────────────────────────────────────────────────────

// Number of samples for the attack (−60 dB charge) or release (−20 dB decay)
// at the given time constant and sample rate.
static int samplesForTime(float ms, double sr)
{
    return static_cast<int>(std::round(ms * 0.001 * sr));
}

// Feed `n` samples of `val` through the follower, return final env value.
static float feedN(EnvelopeFollower& e, float val, int n)
{
    float out = 0.0f;
    for (int i = 0; i < n; ++i)
        out = e.process(val);
    return out;
}

// ─── Basic sanity ────────────────────────────────────────────────────────────

TEST_CASE("EnvelopeFollower: starts at zero after prepare")
{
    EnvelopeFollower e;
    e.prepare(44100.0);
    REQUIRE(e.getValue() == Catch::Approx(0.0f));
}

TEST_CASE("EnvelopeFollower: reset clears state")
{
    EnvelopeFollower e;
    e.prepare(44100.0);
    feedN(e, 1.0f, 500);
    REQUIRE(e.getValue() > 0.5f);
    e.reset();
    REQUIRE(e.getValue() == Catch::Approx(0.0f));
}

TEST_CASE("EnvelopeFollower: negative input is full-wave rectified")
{
    EnvelopeFollower e;
    e.prepare(44100.0);
    feedN(e, -1.0f, 2000);
    // Should track magnitude of -1, not 0
    REQUIRE(e.getValue() > 0.9f);
}

// ─── Attack timing ───────────────────────────────────────────────────────────
// The coef definition guarantees: starting from 0, a step input of 1.0 reaches
// 0.999 (−60 dBFS below target gap) in exactly attackMs × sampleRate / 1000 samples.

TEST_CASE("EnvelopeFollower: attack reaches 0.999 at exactly the labelled attack time")
{
    const double sr = 44100.0;
    EnvelopeFollower e;
    e.prepare(sr);

    SECTION("1 ms attack")
    {
        e.setAttackMs(1.0f);
        const int N = samplesForTime(1.0f, sr);   // 44 samples
        feedN(e, 1.0f, N);
        REQUIRE(e.getValue() == Catch::Approx(0.999f).margin(0.005f));
    }

    SECTION("10 ms attack")
    {
        e.setAttackMs(10.0f);
        const int N = samplesForTime(10.0f, sr);  // 441 samples
        feedN(e, 1.0f, N);
        REQUIRE(e.getValue() == Catch::Approx(0.999f).margin(0.005f));
    }

    SECTION("100 ms attack")
    {
        e.setAttackMs(100.0f);
        const int N = samplesForTime(100.0f, sr); // 4410 samples
        feedN(e, 1.0f, N);
        REQUIRE(e.getValue() == Catch::Approx(0.999f).margin(0.005f));
    }

    SECTION("500 ms attack")
    {
        e.setAttackMs(500.0f);
        const int N = samplesForTime(500.0f, sr); // 22050 samples
        feedN(e, 1.0f, N);
        REQUIRE(e.getValue() == Catch::Approx(0.999f).margin(0.005f));
    }
}

TEST_CASE("EnvelopeFollower: at half the labelled attack time, envelope is substantially below target")
{
    // At t/2, the one-pole filter should be at ~1 - sqrt(0.001) ≈ 0.968.
    // Verifying it is NOT yet near 1.0 confirms the time scale is correct.
    const double sr = 44100.0;
    EnvelopeFollower e;
    e.prepare(sr);
    e.setAttackMs(100.0f);
    feedN(e, 1.0f, samplesForTime(100.0f, sr) / 2);
    REQUIRE(e.getValue() < 0.98f);
    REQUIRE(e.getValue() > 0.90f);
}

// ─── Release timing ──────────────────────────────────────────────────────────
// Release uses ln(10) → time to decay to 10% of peak (−20 dB).
// This matches perceived release: the sound is still faintly audible at the
// labelled time, then tails off naturally below that.

TEST_CASE("EnvelopeFollower: release decays to 0.1 at exactly the labelled release time")
{
    const double sr = 44100.0;
    EnvelopeFollower e;
    e.prepare(sr);

    SECTION("10 ms release")
    {
        e.setAttackMs(1.0f);
        feedN(e, 1.0f, samplesForTime(1.0f, sr) * 4);
        REQUIRE(e.getValue() > 0.999f);

        e.setReleaseMs(10.0f);
        const int N = samplesForTime(10.0f, sr);
        feedN(e, 0.0f, N);
        REQUIRE(e.getValue() == Catch::Approx(0.1f).margin(0.015f));
    }

    SECTION("50 ms release")
    {
        e.setAttackMs(1.0f);
        feedN(e, 1.0f, samplesForTime(1.0f, sr) * 4);

        e.setReleaseMs(50.0f);
        const int N = samplesForTime(50.0f, sr);
        feedN(e, 0.0f, N);
        REQUIRE(e.getValue() == Catch::Approx(0.1f).margin(0.015f));
    }

    SECTION("200 ms release")
    {
        e.setAttackMs(1.0f);
        feedN(e, 1.0f, samplesForTime(1.0f, sr) * 4);

        e.setReleaseMs(200.0f);
        const int N = samplesForTime(200.0f, sr);
        feedN(e, 0.0f, N);
        REQUIRE(e.getValue() == Catch::Approx(0.1f).margin(0.015f));
    }
}

// ─── Attack vs release asymmetry ─────────────────────────────────────────────

TEST_CASE("EnvelopeFollower: slow attack with fast release — shape is asymmetric")
{
    const double sr = 44100.0;
    EnvelopeFollower e;
    e.prepare(sr);
    e.setAttackMs(200.0f);
    e.setReleaseMs(5.0f);

    // After 50ms (25% of attack time): envelope should be well below 0.9
    feedN(e, 1.0f, (int)(0.050 * sr));
    const float levelAfterAttack = e.getValue();
    REQUIRE(levelAfterAttack < 0.9f);

    // After 5ms of silence (= labelled release time): should be near 0.1 (−20 dB)
    feedN(e, 0.0f, samplesForTime(5.0f, sr));
    REQUIRE(e.getValue() < 0.15f);
}

TEST_CASE("EnvelopeFollower: slow release does not cut off prematurely")
{
    const double sr = 44100.0;
    EnvelopeFollower e;
    e.prepare(sr);
    e.setAttackMs(1.0f);
    e.setReleaseMs(2000.0f);

    // Fully charged
    feedN(e, 1.0f, samplesForTime(1.0f, sr) * 4);
    REQUIRE(e.getValue() > 0.999f);

    // After 100ms of silence — should still be well above 0.1 (release is 2000ms)
    // With ln(10) release, 100ms/2000ms = 5% of release time → very little decay yet.
    feedN(e, 0.0f, (int)(0.100 * sr));
    REQUIRE(e.getValue() > 0.88f);
}

// ─── Parameter updates mid-stream ────────────────────────────────────────────

TEST_CASE("EnvelopeFollower: changing release time mid-decay takes effect immediately")
{
    const double sr = 44100.0;
    EnvelopeFollower e;
    e.prepare(sr);

    // Charge up
    e.setAttackMs(1.0f);
    e.setReleaseMs(1000.0f);
    feedN(e, 1.0f, samplesForTime(1.0f, sr) * 4);
    REQUIRE(e.getValue() > 0.999f);

    // Start decaying with 1000ms release — after 10ms should still be very high
    feedN(e, 0.0f, (int)(0.010 * sr));
    REQUIRE(e.getValue() > 0.93f);

    // Switch to 5ms release — should reach ~0.1 (−20 dB) in 5ms
    e.setReleaseMs(5.0f);
    feedN(e, 0.0f, samplesForTime(5.0f, sr));
    REQUIRE(e.getValue() < 0.15f);
}
