#include "ABSlotManager.h"
#include "Parameters.h"

namespace kaigen::phantom
{

ABSlotManager::ABSlotManager(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    slots[0] = apvts.copyState();
    slots[1] = apvts.copyState();
    // Registration of APVTS listeners happens in a later task.
}

ABSlotManager::~ABSlotManager() = default;

void ABSlotManager::parameterChanged(const juce::String&, float) {}

void ABSlotManager::snapTo(Slot target)
{
    if (target == active) return;

    // Commit live → currently-active slot.
    slots[(int) active] = apvts.copyState();

    // Replace live from target.
    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        apvts.replaceState(slots[(int) target]);
    }

    active = target;
}
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
