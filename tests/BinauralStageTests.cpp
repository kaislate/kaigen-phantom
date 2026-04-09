#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/BinauralStage.h"

static juce::AudioBuffer<float> makeMonoBuffer(float value, int samples)
{
    juce::AudioBuffer<float> buf(2, samples);
    buf.clear();
    for (int i = 0; i < samples; ++i)
    {
        buf.setSample(0, i, value);
        buf.setSample(1, i, value);
    }
    return buf;
}

TEST_CASE("BinauralStage Off mode: output equals input")
{
    BinauralStage stage;
    stage.prepare(44100.0, 512);
    stage.setMode(BinauralMode::Off);

    auto buf = makeMonoBuffer(0.5f, 512);
    stage.process(buf);

    REQUIRE(buf.getSample(0, 0) == Catch::Approx(0.5f));
    REQUIRE(buf.getSample(1, 0) == Catch::Approx(0.5f));
}

TEST_CASE("BinauralStage Spread mode: channels differ at non-zero width")
{
    BinauralStage stage;
    stage.prepare(44100.0, 512);
    stage.setMode(BinauralMode::Spread);
    stage.setWidth(1.0f);

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    for (int i = 0; i < 512; ++i)
    {
        buf.setSample(0, i, 0.5f);
        buf.setSample(1, i, 0.5f);
    }
    stage.process(buf);

    // After spread processing L and R should differ (high harmonics pushed outward)
    float diffSum = 0.0f;
    for (int i = 0; i < 512; ++i)
        diffSum += std::abs(buf.getSample(0, i) - buf.getSample(1, i));

    REQUIRE(diffSum > 0.0f);
}

TEST_CASE("BinauralStage isUsingBinaural returns true when mode != Off")
{
    BinauralStage stage;
    stage.prepare(44100.0, 512);
    stage.setMode(BinauralMode::Off);
    REQUIRE(stage.isUsingBinaural() == false);
    stage.setMode(BinauralMode::Spread);
    REQUIRE(stage.isUsingBinaural() == true);
}

TEST_CASE("BinauralStage width 0 in Spread mode: channels equal")
{
    BinauralStage stage;
    stage.prepare(44100.0, 512);
    stage.setMode(BinauralMode::Spread);
    stage.setWidth(0.0f);

    auto buf = makeMonoBuffer(0.5f, 512);
    stage.process(buf);

    for (int i = 0; i < 512; ++i)
        REQUIRE(buf.getSample(0, i) == Catch::Approx(buf.getSample(1, i)).margin(1e-5f));
}
