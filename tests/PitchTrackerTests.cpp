#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/PitchTracker.h"
#include <cmath>
#include <vector>

static std::vector<float> makeSine(float freq, float sampleRate, int numSamples)
{
    std::vector<float> buf(numSamples);
    for (int i = 0; i < numSamples; ++i)
        buf[i] = std::sin(2.0f * 3.14159265f * freq * i / sampleRate);
    return buf;
}

TEST_CASE("PitchTracker detects 80 Hz sine within 2 Hz")
{
    PitchTracker tracker;
    tracker.prepare(44100.0, 2048);
    auto sine = makeSine(80.0f, 44100.0f, 2048);
    float detected = tracker.detectPitch(sine.data(), (int)sine.size());
    REQUIRE(detected == Catch::Approx(80.0f).margin(2.0f));
}

TEST_CASE("PitchTracker detects 40 Hz sine within 2 Hz")
{
    // 40 Hz period = ~1103 samples at 44100 Hz; need tauMax > 1103, so numSamples >= 4096
    PitchTracker tracker;
    tracker.prepare(44100.0, 4096);
    auto sine = makeSine(40.0f, 44100.0f, 4096);
    float detected = tracker.detectPitch(sine.data(), (int)sine.size());
    REQUIRE(detected == Catch::Approx(40.0f).margin(2.0f));
}

TEST_CASE("PitchTracker returns -1 for silence")
{
    PitchTracker tracker;
    tracker.prepare(44100.0, 2048);
    std::vector<float> silence(2048, 0.0f);
    float detected = tracker.detectPitch(silence.data(), (int)silence.size());
    REQUIRE(detected < 0.0f);
}

TEST_CASE("PitchTracker glide smooths pitch changes")
{
    PitchTracker tracker;
    tracker.prepare(44100.0, 2048);
    tracker.setGlideMs(50.0f);

    // Step 1: establish baseline at 80 Hz
    auto sine80 = makeSine(80.0f, 44100.0f, 2048);
    tracker.detectPitch(sine80.data(), 2048);

    // Step 2: feed a 320 Hz buffer — a large jump
    std::vector<float> buf320(2048);
    for (int i = 0; i < 2048; ++i)
        buf320[i] = std::sin(2.0f * juce::MathConstants<float>::pi * 320.0f * i / 44100.0f);
    tracker.detectPitch(buf320.data(), 2048);

    // After ONE call at 320 Hz, smoothed pitch must not have jumped instantly —
    // it should still be somewhere between 80 and 320 (exclusive).
    float smoothed = tracker.getSmoothedPitch();
    REQUIRE(smoothed > 80.0f);
    REQUIRE(smoothed < 320.0f);
}
