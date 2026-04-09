#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/HarmonicGenerator.h"
#include "Engines/Deconfliction/PartitionStrategy.h"
#include "Engines/Deconfliction/OddEvenStrategy.h"
#include "Engines/Deconfliction/ResidueStrategy.h"
#include "Engines/Deconfliction/SpectralLaneStrategy.h"
#include "Engines/Deconfliction/StaggerStrategy.h"
#include "Engines/Deconfliction/BinauralStrategy.h"

static std::vector<Voice> makeTwoVoices()
{
    Voice a, b;
    a.active = true; a.fundamentalHz = 40.0f; a.voiceIndex = 0;
    b.active = true; b.fundamentalHz = 60.0f; b.voiceIndex = 1;
    for (int i = 0; i < 7; ++i) { a.amps[i] = 1.0f; b.amps[i] = 1.0f; }
    return { a, b };
}

TEST_CASE("PartitionStrategy: shared harmonics owned by one voice only")
{
    PartitionStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    bool foundDivided = false;
    for (int i = 0; i < 7; ++i)
    {
        if ((voices[0].amps[i] > 0.0f) != (voices[1].amps[i] > 0.0f))
            foundDivided = true;
    }
    REQUIRE(foundDivided);
}

TEST_CASE("OddEvenStrategy: voice 0 has only odd harmonics, voice 1 has only even harmonics")
{
    OddEvenStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    REQUIRE(voices[0].amps[0] == Catch::Approx(0.0f));  // H2 zero for voice 0
    REQUIRE(voices[0].amps[1] > 0.0f);                  // H3 present for voice 0
    REQUIRE(voices[1].amps[0] > 0.0f);                  // H2 present for voice 1
    REQUIRE(voices[1].amps[1] == Catch::Approx(0.0f));  // H3 zero for voice 1
}

TEST_CASE("ResidueStrategy: does not zero all harmonics for any voice")
{
    ResidueStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    float sumA = 0.0f, sumB = 0.0f;
    for (int i = 0; i < 7; ++i) { sumA += voices[0].amps[i]; sumB += voices[1].amps[i]; }
    REQUIRE(sumA > 0.0f);
    REQUIRE(sumB > 0.0f);
}

TEST_CASE("SpectralLaneStrategy: voice 0 has higher low harmonics than voice 1")
{
    SpectralLaneStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    REQUIRE(voices[0].amps[0] >= voices[1].amps[0]);  // voice 0 owns H2 more
    REQUIRE(voices[1].amps[6] >= voices[0].amps[6]);  // voice 1 owns H8 more
}

TEST_CASE("StaggerStrategy: stores delay per voice index")
{
    StaggerStrategy strat;
    strat.setDelayMs(8.0f, 44100.0);

    auto voices = makeTwoVoices();
    strat.resolve(voices);

    REQUIRE(strat.getDelaySamplesForVoice(0) == 0);
    REQUIRE(strat.getDelaySamplesForVoice(1) > 0);
}

TEST_CASE("BinauralStrategy: voices assigned to different pan positions")
{
    BinauralStrategy strat;
    auto voices = makeTwoVoices();
    strat.resolve(voices);

    REQUIRE(strat.getPanForVoice(0) != strat.getPanForVoice(1));
}
