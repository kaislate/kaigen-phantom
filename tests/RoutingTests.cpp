#include <catch2/catch_test_macros.hpp>
#include "Modulation/Routing.h"

using namespace kaigen::phantom;

TEST_CASE("Routing default values", "[routing]")
{
    Routing r;
    REQUIRE(r.sourceId.isEmpty());
    REQUIRE(r.paramId.isEmpty());
    REQUIRE(r.depth == 0.0f);
    REQUIRE_FALSE(r.polarityInverted);
}

TEST_CASE("Routing toValueTree round-trip", "[routing]")
{
    Routing src;
    src.sourceId = "macro1";
    src.paramId  = "a_ghost";
    src.depth    = 0.5f;

    auto tree = src.toValueTree();
    auto restored = Routing::fromValueTree(tree);

    REQUIRE(restored.sourceId == "macro1");
    REQUIRE(restored.paramId  == "a_ghost");
    REQUIRE(restored.depth    == 0.5f);
    REQUIRE_FALSE(restored.polarityInverted);
}

TEST_CASE("Routing polarity-inverted round-trip", "[routing]")
{
    Routing src;
    src.sourceId = "macro2";
    src.paramId  = "a_phantom_threshold";
    src.depth    = -0.3f;
    src.polarityInverted = true;

    auto tree = src.toValueTree();
    auto restored = Routing::fromValueTree(tree);

    REQUIRE(restored.depth == -0.3f);
    REQUIRE(restored.polarityInverted);
}

TEST_CASE("Routing equality", "[routing]")
{
    Routing a; a.sourceId = "macro1"; a.paramId = "a_ghost"; a.depth = 0.5f;
    Routing b = a;
    REQUIRE(a == b);
    b.depth = 0.6f;
    REQUIRE_FALSE(a == b);
}
