#if KAIGEN_PRO_BUILD

#include "MorphEngine.h"
#include "Parameters.h"
#include "ABSlotManager.h"
#include "Engines/PhantomEngine.h"

namespace kaigen::phantom
{

MorphEngine::MorphEngine(juce::AudioProcessorValueTreeState& apvtsRef,
                         ABSlotManager& abSlotsRef,
                         PhantomEngine& primaryEngineRef)
    : apvts(apvtsRef), abSlots(abSlotsRef), primaryEngine(primaryEngineRef)
{
    // Listener registration happens in Task 8.
}

MorphEngine::~MorphEngine() = default;

void MorphEngine::prepareToPlay(double sr, int spb) { sampleRate = sr; samplesPerBlock = spb; }
void MorphEngine::preProcessBlock() {}
void MorphEngine::postProcessBlock(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) {}
void MorphEngine::parameterChanged(const juce::String&, float) {}

void MorphEngine::setArcDepth(const juce::String& paramID, float normalizedDepth)
{
    // Clamp to bipolar range.
    normalizedDepth = juce::jlimit(-1.0f, 1.0f, normalizedDepth);

    if (std::abs(normalizedDepth) < 1e-6f)
    {
        lane1Arcs.erase(paramID);
        return;
    }

    // Capture current base value for this parameter.
    float base = 0.0f;
    if (auto* p = apvts.getParameter(paramID))
        base = p->convertFrom0to1(p->getValue());

    lane1Arcs[paramID] = { normalizedDepth, base };
}

float MorphEngine::getArcDepth(const juce::String& paramID) const
{
    auto it = lane1Arcs.find(paramID);
    return (it != lane1Arcs.end()) ? it->second.depth : 0.0f;
}

bool MorphEngine::hasNonZeroArc(const juce::String& paramID) const
{
    return lane1Arcs.find(paramID) != lane1Arcs.end();
}

int MorphEngine::armedKnobCount() const
{
    return (int) lane1Arcs.size();
}

std::vector<juce::String> MorphEngine::getArmedParamIDs() const
{
    std::vector<juce::String> result;
    result.reserve(lane1Arcs.size());
    for (const auto& [id, entry] : lane1Arcs)
        result.push_back(id);
    return result;
}

void MorphEngine::setEnabled(bool on)
{
    enabled = on;
    // Also sync the APVTS parameter so the UI slider and DAW reflect state.
    if (auto* p = apvts.getParameter(ParamID::MORPH_ENABLED))
    {
        const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
        p->beginChangeGesture();
        p->setValueNotifyingHost(on ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

void MorphEngine::beginCapture() {}
std::vector<juce::String> MorphEngine::endCapture(bool) { return {}; }

void MorphEngine::setSceneCrossfadeEnabled(bool on)
{
    sceneEnabled = on;
    if (auto* p = apvts.getParameter(ParamID::SCENE_ENABLED))
    {
        const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
        p->beginChangeGesture();
        p->setValueNotifyingHost(on ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
    // Note: Task 16 handles lazy secondaryEngine construction on first enable.
}

juce::ValueTree MorphEngine::toMorphConfigTree() const { return juce::ValueTree("MorphConfig"); }
void MorphEngine::fromMorphConfigTree(const juce::ValueTree&) {}

juce::ValueTree MorphEngine::toStateTree() const { return juce::ValueTree("MorphState"); }
void MorphEngine::fromStateTree(const juce::ValueTree&) {}

std::vector<juce::String> MorphEngine::getContinuousParamIDs(juce::AudioProcessorValueTreeState& apvts)
{
    std::vector<juce::String> result;
    for (const auto& id : getAllParameterIDs())
    {
        if (auto* p = apvts.getParameter(id))
        {
            // Continuous = range with non-integer interval, or a non-bool/non-choice type.
            if (dynamic_cast<juce::AudioParameterBool*>(p)   != nullptr) continue;
            if (dynamic_cast<juce::AudioParameterChoice*>(p) != nullptr) continue;
            // Skip Pro morph params themselves.
            if (id == ParamID::MORPH_ENABLED || id == ParamID::MORPH_AMOUNT
             || id == ParamID::SCENE_ENABLED || id == ParamID::SCENE_POSITION)
                continue;
            result.push_back(id);
        }
    }
    return result;
}

void MorphEngine::updateSmoothing() {}
float MorphEngine::smoothOne(float&, float) const { return 0.0f; }
void MorphEngine::writeParamClamped(const juce::String&, float) {}

} // namespace kaigen::phantom

#endif // KAIGEN_PRO_BUILD
