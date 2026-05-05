#include <catch2/catch_test_macros.hpp>
#include "Modulation/Modulator.h"

using namespace kaigen::phantom;

namespace {

class StubModulator : public Modulator
{
public:
    StubModulator() : Modulator("stub1") {}
    float getCurrentValue() const noexcept override { return 0.5f; }
};

} // namespace

TEST_CASE("Modulator id is preserved", "[modulator]")
{
    StubModulator m;
    REQUIRE(m.getId() == "stub1");
}

TEST_CASE("Modulator getCurrentValue returns subclass value", "[modulator]")
{
    StubModulator m;
    REQUIRE(m.getCurrentValue() == 0.5f);
}
