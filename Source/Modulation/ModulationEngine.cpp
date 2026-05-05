// Source/Modulation/ModulationEngine.cpp
#include "ModulationEngine.h"

namespace kaigen::phantom
{

ModulationEngine::ModulationEngine(juce::AudioProcessorValueTreeState& apvtsRef, juce::String prefixStr)
    : apvts(apvtsRef), prefix(std::move(prefixStr))
{
}

void ModulationEngine::addModulator(std::unique_ptr<Modulator> m)
{
    modulators.push_back(std::move(m));
}

Modulator* ModulationEngine::findModulator(const juce::String& sourceId) const
{
    for (auto& m : modulators)
        if (m->getId() == sourceId) return m.get();
    return nullptr;
}

bool ModulationEngine::addRouting(const Routing& r)
{
    if (! r.paramId.startsWith(prefix)) return false;
    if (findModulator(r.sourceId) == nullptr) return false;
    routings.push_back(r);
    return true;
}

void ModulationEngine::removeRouting(const juce::String& sourceId, const juce::String& paramId)
{
    routings.erase(std::remove_if(routings.begin(), routings.end(),
        [&](const Routing& r) { return r.sourceId == sourceId && r.paramId == paramId; }),
        routings.end());
}

void ModulationEngine::clearRoutings() { routings.clear(); }

float ModulationEngine::getModulatedValue(const juce::String& paramId, float base) const
{
    auto* paramPtr = apvts.getParameter(paramId);
    if (paramPtr == nullptr) return base;
    const auto range = paramPtr->getNormalisableRange();
    const float span = range.end - range.start;

    float modulated = base;
    for (const auto& r : routings)
    {
        if (r.paramId != paramId) continue;
        auto* m = findModulator(r.sourceId);
        if (m == nullptr) continue;
        const float modVal = m->getCurrentValue();
        const float effDepth = r.polarityInverted ? -r.depth : r.depth;
        modulated += effDepth * modVal * span;
    }

    return juce::jlimit(range.start, range.end, modulated);
}

juce::ValueTree ModulationEngine::toValueTree() const
{
    juce::ValueTree node("Engine");
    node.setProperty("prefix", prefix, nullptr);
    juce::ValueTree mods("Modulators");
    for (auto& m : modulators) m->writeToTree(mods);
    node.appendChild(mods, nullptr);
    juce::ValueTree routes("Routings");
    for (auto& r : routings) routes.appendChild(r.toValueTree(), nullptr);
    node.appendChild(routes, nullptr);
    return node;
}

void ModulationEngine::fromValueTree(const juce::ValueTree& engineNode)
{
    if (! engineNode.hasType("Engine")) return;
    if (engineNode.getProperty("prefix").toString() != prefix) return;

    auto modsNode = engineNode.getChildWithName("Modulators");
    for (auto& m : modulators) m->readFromTree(modsNode);

    routings.clear();
    auto routesNode = engineNode.getChildWithName("Routings");
    for (int i = 0; i < routesNode.getNumChildren(); ++i)
    {
        auto child = routesNode.getChild(i);
        if (child.hasType("Route"))
            addRouting(Routing::fromValueTree(child));   // may reject if invalid
    }
}

} // namespace kaigen::phantom
