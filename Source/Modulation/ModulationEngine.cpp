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

    auto current = routingsAtomic.load();
    auto next = std::make_shared<RoutingsList>(*current);
    next->push_back(r);
    routingsAtomic.store(std::shared_ptr<const RoutingsList>(next));
    return true;
}

void ModulationEngine::removeRouting(const juce::String& sourceId, const juce::String& paramId)
{
    auto current = routingsAtomic.load();
    auto next = std::make_shared<RoutingsList>(*current);
    next->erase(std::remove_if(next->begin(), next->end(),
        [&](const Routing& r) { return r.sourceId == sourceId && r.paramId == paramId; }),
        next->end());
    routingsAtomic.store(std::shared_ptr<const RoutingsList>(next));
}

void ModulationEngine::clearRoutings()
{
    routingsAtomic.store(std::make_shared<const RoutingsList>());
}

bool ModulationEngine::setRoutingDepth(const juce::String& sourceId, const juce::String& paramId, float newDepth)
{
    auto current = routingsAtomic.load();
    auto next = std::make_shared<RoutingsList>(*current);
    bool found = false;
    for (auto& r : *next)
    {
        if (r.sourceId == sourceId && r.paramId == paramId)
        {
            r.depth = newDepth;
            found = true;
            break;
        }
    }
    if (! found) return false;
    routingsAtomic.store(std::shared_ptr<const RoutingsList>(next));
    return true;
}

ModulationEngine::RoutingsSnapshot ModulationEngine::getRoutingsSnapshot() const noexcept
{
    return routingsAtomic.load();
}

std::vector<Routing> ModulationEngine::getRoutings() const
{
    return *routingsAtomic.load();
}

float ModulationEngine::getModulatedValue(const juce::String& paramId, float base) const
{
    return getModulatedValue(paramId, apvts.getParameter(paramId), base);
}

float ModulationEngine::getModulatedValue(const juce::String& paramId,
                                            juce::RangedAudioParameter* param,
                                            float base) const noexcept
{
    if (param == nullptr) return base;

    // Cheap early-out when no routings exist (the COMMON case). Avoids
    // dereferencing range + iterating an empty list — the snapshot load is
    // a single atomic shared_ptr load, ~10ns.
    auto snapshot = routingsAtomic.load();
    if (! snapshot || snapshot->empty()) return base;

    const auto& range = param->getNormalisableRange();
    const float span  = range.end - range.start;

    float modulated = base;
    for (const auto& r : *snapshot)
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
    auto snapshot = routingsAtomic.load();
    for (const auto& r : *snapshot) routes.appendChild(r.toValueTree(), nullptr);
    node.appendChild(routes, nullptr);
    return node;
}

void ModulationEngine::fromValueTree(const juce::ValueTree& engineNode)
{
    if (! engineNode.hasType("Engine")) return;
    if (engineNode.getProperty("prefix").toString() != prefix) return;

    auto modsNode = engineNode.getChildWithName("Modulators");
    for (auto& m : modulators) m->readFromTree(modsNode);

    auto next = std::make_shared<RoutingsList>();
    auto routesNode = engineNode.getChildWithName("Routings");
    for (int i = 0; i < routesNode.getNumChildren(); ++i)
    {
        auto child = routesNode.getChild(i);
        if (! child.hasType("Route")) continue;
        Routing r = Routing::fromValueTree(child);
        // Validate (same as addRouting): correct prefix + known modulator.
        if (! r.paramId.startsWith(prefix)) continue;
        if (findModulator(r.sourceId) == nullptr) continue;
        next->push_back(r);
    }
    routingsAtomic.store(std::shared_ptr<const RoutingsList>(next));
}

} // namespace kaigen::phantom
