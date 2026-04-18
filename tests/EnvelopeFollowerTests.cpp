#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/EnvelopeFollower.h"

TEST_CASE("EnvelopeFollower starts at zero")
{
    EnvelopeFollower e;
    e.prepare(44100.0);
    REQUIRE(e.getValue() == Catch::Approx(0.0f));
}

TEST_CASE("EnvelopeFollower: step input reaches target")
{
    EnvelopeFollower e;
    e.prepare(44100.0);
    e.setAttackMs(1.0f);
    e.setReleaseMs(50.0f);

    // Feed 1.0 for 100 samples (~2.27 ms at 44.1kHz) — should approach 1.0
    for (int i = 0; i < 500; ++i)
        e.process(1.0f);

    REQUIRE(e.getValue() > 0.9f);
    REQUIRE(e.getValue() <= 1.0f);
}

TEST_CASE("EnvelopeFollower: silent input decays towards zero")
{
    EnvelopeFollower e;
    e.prepare(44100.0);
    e.setAttackMs(1.0f);
    e.setReleaseMs(10.0f);

    // Ramp up first
    for (int i = 0; i < 500; ++i) e.process(1.0f);
    REQUIRE(e.getValue() > 0.9f);

    // Now silence
    for (int i = 0; i < 5000; ++i) e.process(0.0f);
    REQUIRE(e.getValue() < 0.01f);
}

TEST_CASE("EnvelopeFollower: negative input is rectified")
{
    EnvelopeFollower e;
    e.prepare(44100.0);
    for (int i = 0; i < 500; ++i) e.process(-0.7f);
    REQUIRE(e.getValue() > 0.6f);
}

TEST_CASE("EnvelopeFollower: reset clears state")
{
    EnvelopeFollower e;
    e.prepare(44100.0);
    for (int i = 0; i < 500; ++i) e.process(1.0f);
    REQUIRE(e.getValue() > 0.5f);
    e.reset();
    REQUIRE(e.getValue() == Catch::Approx(0.0f));
}
