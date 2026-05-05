// Source/Modulation/Routing.cpp
#include "Routing.h"

namespace kaigen::phantom
{

juce::ValueTree Routing::toValueTree() const
{
    juce::ValueTree t("Route");
    t.setProperty("source", sourceId, nullptr);
    t.setProperty("param",  paramId,  nullptr);
    t.setProperty("depth",  depth,    nullptr);
    if (polarityInverted) t.setProperty("invert", true, nullptr);
    return t;
}

Routing Routing::fromValueTree(const juce::ValueTree& t)
{
    Routing r;
    r.sourceId         = t.getProperty("source").toString();
    r.paramId          = t.getProperty("param").toString();
    r.depth            = (float) t.getProperty("depth", 0.0f);
    r.polarityInverted = (bool) t.getProperty("invert", false);
    return r;
}

bool Routing::operator==(const Routing& other) const noexcept
{
    return sourceId == other.sourceId
        && paramId == other.paramId
        && depth == other.depth
        && polarityInverted == other.polarityInverted;
}

} // namespace kaigen::phantom
