#include <catch2/catch_test_macros.hpp>
#include "../Source/MatrixViewState.h"

using kaigen::phantom::MatrixViewState;
using kaigen::phantom::MatrixMode;
using kaigen::phantom::writeMatrixViewToTree;
using kaigen::phantom::readMatrixViewFromTree;

TEST_CASE("MatrixViewState round-trips through ValueTree", "[matrix-view][persistence]")
{
    MatrixViewState in;
    in.mode = MatrixMode::Matrix;
    in.expandedA = "GHOST,RECIPE,FILTER";
    in.expandedB = "RESYN";

    juce::ValueTree parent("PluginState");
    writeMatrixViewToTree(parent, in);

    auto out = readMatrixViewFromTree(parent);
    REQUIRE(out.mode == MatrixMode::Matrix);
    REQUIRE(out.expandedA == "GHOST,RECIPE,FILTER");
    REQUIRE(out.expandedB == "RESYN");
}

TEST_CASE("MatrixViewState defaults to Slots mode + GHOST,RECIPE expanded when missing", "[matrix-view][persistence]")
{
    juce::ValueTree parent("PluginState");
    auto out = readMatrixViewFromTree(parent);
    REQUIRE(out.mode == MatrixMode::Slots);
    REQUIRE(out.expandedA == "GHOST,RECIPE");
    REQUIRE(out.expandedB == "GHOST,RECIPE");
}
