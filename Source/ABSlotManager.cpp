#include "ABSlotManager.h"
#include "Parameters.h"

namespace kaigen::phantom
{

ABSlotManager::ABSlotManager(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    // Registration of APVTS listeners happens in a later task.
}

ABSlotManager::~ABSlotManager() = default;

void ABSlotManager::parameterChanged(const juce::String&, float) {}

void ABSlotManager::snapTo(Slot) {}
void ABSlotManager::copy(Slot, Slot) {}
bool ABSlotManager::isModified(Slot s) const noexcept { return modified[(int) s]; }
juce::String ABSlotManager::getPresetRef(Slot s) const { return presetRef[(int) s]; }

void ABSlotManager::syncActiveSlotFromLive() {}
juce::ValueTree ABSlotManager::toStateTree() const { return {}; }
void ABSlotManager::fromStateTree(const juce::ValueTree&) {}

void ABSlotManager::loadSinglePresetIntoActive(const juce::ValueTree&, const juce::String&) {}
void ABSlotManager::loadABPreset(const juce::ValueTree&, const juce::String&) {}
juce::ValueTree ABSlotManager::buildPresetSlotBChild() const { return {}; }

const juce::StringArray& ABSlotManager::discreteParamIDs()
{
    static const juce::StringArray ids { ParamID::MODE, ParamID::BYPASS,
                                         ParamID::GHOST_MODE, ParamID::BINAURAL_MODE };
    return ids;
}

} // namespace kaigen::phantom
