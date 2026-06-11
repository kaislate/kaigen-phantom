#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "MorphCrossfader.h"

using namespace kaigen::phantom;
using Catch::Approx;

namespace {
    juce::AudioBuffer<float> makeConst(int ch, int n, float value) {
        juce::AudioBuffer<float> b(ch, n);
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < n; ++i)
                b.setSample(c, i, value);
        return b;
    }
}

TEST_CASE("MorphCrossfader: linear curve at canonical positions", "[crossfader]")
{
    MorphCrossfader xf;
    xf.prepare(44100.0, 4);
    xf.setCurve(MorphCrossfader::Curve::Linear);
    xf.setLevels(0.0f, 0.0f);

    auto a = makeConst(2, 4, 1.0f);
    auto b = makeConst(2, 4, 2.0f);
    juce::AudioBuffer<float> out(2, 4);

    xf.setMorph(0.0f);
    xf.mix(a, false, b, false, out);
    REQUIRE(out.getSample(0, 0) == Approx(1.0f));

    xf.setMorph(1.0f);
    xf.mix(a, false, b, false, out);
    REQUIRE(out.getSample(0, 0) == Approx(2.0f));

    xf.setMorph(0.5f);
    xf.mix(a, false, b, false, out);
    REQUIRE(out.getSample(0, 0) == Approx(1.5f));   // 0.5*1 + 0.5*2
}

TEST_CASE("MorphCrossfader: equal-power at midpoint sums to ~1.414", "[crossfader]")
{
    MorphCrossfader xf;
    xf.prepare(44100.0, 4);
    xf.setCurve(MorphCrossfader::Curve::EqualPower);
    xf.setLevels(0.0f, 0.0f);
    xf.setMorph(0.5f);

    auto a = makeConst(1, 1, 1.0f);
    auto b = makeConst(1, 1, 1.0f);
    juce::AudioBuffer<float> out(1, 1);
    xf.mix(a, false, b, false, out);

    REQUIRE(out.getSample(0, 0) == Approx(std::sqrt(2.0f)).margin(1.0e-5f));
}

TEST_CASE("MorphCrossfader: s-curve at midpoint = linear midpoint", "[crossfader]")
{
    MorphCrossfader xf;
    xf.prepare(44100.0, 4);
    xf.setCurve(MorphCrossfader::Curve::SCurve);
    xf.setLevels(0.0f, 0.0f);
    xf.setMorph(0.5f);

    auto a = makeConst(1, 1, 1.0f);
    auto b = makeConst(1, 1, 2.0f);
    juce::AudioBuffer<float> out(1, 1);
    xf.mix(a, false, b, false, out);

    REQUIRE(out.getSample(0, 0) == Approx(1.5f));
}

TEST_CASE("MorphCrossfader: bypassA treats A as silent", "[crossfader]")
{
    MorphCrossfader xf;
    xf.prepare(44100.0, 4);
    xf.setCurve(MorphCrossfader::Curve::Linear);
    xf.setLevels(0.0f, 0.0f);
    xf.setMorph(0.5f);

    auto a = makeConst(1, 1, 1.0f);
    auto b = makeConst(1, 1, 2.0f);
    juce::AudioBuffer<float> out(1, 1);
    xf.mix(a, true /*bypassA*/, b, false, out);
    REQUIRE(out.getSample(0, 0) == Approx(1.0f));
}

TEST_CASE("MorphCrossfader: morph jump ramps across the block instead of stepping", "[crossfader]")
{
    MorphCrossfader xf;
    xf.prepare(44100.0, 64);
    xf.setCurve(MorphCrossfader::Curve::Linear);
    xf.setLevels(0.0f, 0.0f);

    auto a = makeConst(1, 64, 1.0f);
    auto b = makeConst(1, 64, 0.0f);
    juce::AudioBuffer<float> out(1, 64);

    // Settle at morph = 0 (full A).
    xf.setMorph(0.0f);
    xf.startBlock();
    xf.mix(a, false, b, false, out);
    REQUIRE(out.getSample(0, 63) == Approx(1.0f));

    // Jump to morph = 1. The next block must ramp A's gain 1 → 0 across
    // the block rather than stepping to 0 at sample 0.
    xf.setMorph(1.0f);
    xf.startBlock();
    xf.mix(a, false, b, false, out);

    CHECK(out.getSample(0, 0) > 0.9f);                            // starts near previous gain
    CHECK(out.getSample(0, 63) == Approx(0.0f).margin(1.0e-4f));  // lands on target
    CHECK(out.getSample(0, 31) == Approx(0.5f).margin(0.05f));    // ~linear midpoint

    // A second mix within the same block (the phantom-only side buffer in
    // DualEngineHost) must apply the identical ramp.
    juce::AudioBuffer<float> out2(1, 64);
    xf.mix(a, false, b, false, out2);
    for (int i = 0; i < 64; ++i)
        REQUIRE(out2.getSample(0, i) == Approx(out.getSample(0, i)));
}

TEST_CASE("MorphCrossfader: first block after prepare snaps to target (no fade-in)", "[crossfader]")
{
    MorphCrossfader xf;
    xf.prepare(44100.0, 16);
    xf.setCurve(MorphCrossfader::Curve::Linear);
    xf.setLevels(0.0f, 0.0f);
    xf.setMorph(1.0f);

    auto a = makeConst(1, 16, 1.0f);
    auto b = makeConst(1, 16, 2.0f);
    juce::AudioBuffer<float> out(1, 16);
    xf.startBlock();
    xf.mix(a, false, b, false, out);

    // No stale-state ramp on the very first block: full B immediately.
    REQUIRE(out.getSample(0, 0) == Approx(2.0f));
}

TEST_CASE("MorphCrossfader: side reports silent only after its ramp settles", "[crossfader]")
{
    MorphCrossfader xf;
    xf.prepare(44100.0, 64);
    xf.setCurve(MorphCrossfader::Curve::Linear);
    xf.setLevels(0.0f, 0.0f);

    // First block at morph = 0: B is silent immediately (snap), A is live.
    xf.setMorph(0.0f);
    xf.startBlock();
    CHECK(xf.bIsSilentThisBlock());
    CHECK_FALSE(xf.aIsSilentThisBlock());

    // Jump to morph = 1: A is ramping out during this block — NOT silent
    // yet. Bypassing it here would hard-cut the ramp tail.
    xf.setMorph(1.0f);
    xf.startBlock();
    CHECK_FALSE(xf.aIsSilentThisBlock());
    CHECK_FALSE(xf.bIsSilentThisBlock());

    // Next block: ramp has settled — A silent, B live.
    xf.startBlock();
    CHECK(xf.aIsSilentThisBlock());
    CHECK_FALSE(xf.bIsSilentThisBlock());
}

TEST_CASE("MorphCrossfader: per-side level trim applies", "[crossfader]")
{
    MorphCrossfader xf;
    xf.prepare(44100.0, 4);
    xf.setCurve(MorphCrossfader::Curve::Linear);
    xf.setLevels(-6.0f /*A*/, 0.0f /*B*/);
    xf.setMorph(0.0f);

    auto a = makeConst(1, 1, 1.0f);
    auto b = makeConst(1, 1, 0.0f);
    juce::AudioBuffer<float> out(1, 1);
    xf.mix(a, false, b, false, out);

    REQUIRE(out.getSample(0, 0) == Approx(juce::Decibels::decibelsToGain(-6.0f)).margin(1.0e-5f));
}
