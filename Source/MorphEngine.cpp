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

void MorphEngine::setArcDepth(const juce::String&, float) {}
float MorphEngine::getArcDepth(const juce::String&) const { return 0.0f; }
bool  MorphEngine::hasNonZeroArc(const juce::String&) const { return false; }
int   MorphEngine::armedKnobCount() const { return 0; }
std::vector<juce::String> MorphEngine::getArmedParamIDs() const { return {}; }

void MorphEngine::setEnabled(bool) {}
void MorphEngine::beginCapture() {}
std::vector<juce::String> MorphEngine::endCapture(bool) { return {}; }

void MorphEngine::setSceneCrossfadeEnabled(bool) {}

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
