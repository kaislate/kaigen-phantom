#include <catch2/catch_test_macros.hpp>
#include "DualEngineHost.h"

TEST_CASE("DualEngineHost: compiles and links", "[host]")
{
    // Constructing a DualEngineHost requires a real APVTS (which itself
    // requires a real AudioProcessor). We can't easily build that in a
    // unit-test context, so this test is a build canary only — full
    // integration verification happens via the plugin's processBlock in
    // PR1 Task 9 (manual smoke test in Live).
    SUCCEED("DualEngineHost compiles and links");
}
