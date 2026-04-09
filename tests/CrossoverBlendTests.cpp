#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/CrossoverBlend.h"

static juce::AudioBuffer<float> makeConstantBuffer(float value, int numCh, int numSamples)
{
    juce::AudioBuffer<float> buf(numCh, numSamples);
    for (int ch = 0; ch < numCh; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buf.setSample(ch, i, value);
    return buf;
}

TEST_CASE("CrossoverBlend ghost=100 Replace: output is non-zero and bounded")
{
    CrossoverBlend blend;
    blend.prepare(44100.0, 512);
    blend.setThresholdHz(80.0f);
    blend.setGhost(1.0f);
    blend.setGhostMode(GhostMode::Replace);
    blend.setSidechainDuckAmount(0.0f);

    auto dry     = makeConstantBuffer(0.8f, 2, 512);
    auto phantom = makeConstantBuffer(0.4f, 2, 512);

    blend.process(dry, phantom, nullptr);

    float peak = 0.0f;
    for (int i = 0; i < 512; ++i)
        peak = std::max(peak, std::abs(dry.getSample(0, i)));

    REQUIRE(peak < 1.5f);
    REQUIRE(peak > 0.0f);
}

TEST_CASE("CrossoverBlend ghost=0: output equals dry (no phantom)")
{
    CrossoverBlend blend;
    blend.prepare(44100.0, 512);
    blend.setThresholdHz(80.0f);
    blend.setGhost(0.0f);
    blend.setGhostMode(GhostMode::Replace);

    auto dry     = makeConstantBuffer(0.5f, 2, 512);
    auto phantom = makeConstantBuffer(0.3f, 2, 512);
    float originalDryValue = dry.getSample(0, 0);

    blend.process(dry, phantom, nullptr);

    REQUIRE(dry.getSample(0, 0) == Catch::Approx(originalDryValue).margin(0.01f));
}

TEST_CASE("CrossoverBlend sidechain ducking reduces phantom amplitude")
{
    CrossoverBlend blend;
    blend.prepare(44100.0, 512);
    blend.setThresholdHz(80.0f);
    blend.setGhost(1.0f);
    blend.setGhostMode(GhostMode::Add);
    blend.setSidechainDuckAmount(1.0f);
    blend.setDuckAttackMs(1.0f);
    blend.setDuckReleaseMs(10.0f);

    auto dry      = makeConstantBuffer(0.0f, 2, 512);
    auto phantom  = makeConstantBuffer(0.5f, 2, 512);
    auto sidechain = makeConstantBuffer(1.0f, 2, 512);

    blend.process(dry, phantom, &sidechain);

    float peakWithDuck = 0.0f;
    for (int i = 0; i < 512; ++i)
        peakWithDuck = std::max(peakWithDuck, std::abs(dry.getSample(0, i)));

    CrossoverBlend blendNoDuck;
    blendNoDuck.prepare(44100.0, 512);
    blendNoDuck.setThresholdHz(80.0f);
    blendNoDuck.setGhost(1.0f);
    blendNoDuck.setGhostMode(GhostMode::Add);
    blendNoDuck.setSidechainDuckAmount(0.0f);

    auto dry2    = makeConstantBuffer(0.0f, 2, 512);
    auto phantom2 = makeConstantBuffer(0.5f, 2, 512);
    blendNoDuck.process(dry2, phantom2, nullptr);
    float peakNoDuck = 0.0f;
    for (int i = 0; i < 512; ++i)
        peakNoDuck = std::max(peakNoDuck, std::abs(dry2.getSample(0, i)));

    REQUIRE(peakWithDuck < peakNoDuck);
}
