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
