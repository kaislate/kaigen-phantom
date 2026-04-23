#include "ABSlotManager.h"
#include "Parameters.h"
#include <map>

namespace kaigen::phantom
{

ABSlotManager::ABSlotManager(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    slots[0] = apvts.copyState();
    slots[1] = apvts.copyState();

    // Subscribe to every parameter so we can flip the active slot's
    // modified flag on any user-initiated change.
    for (const auto& id : getAllParameterIDs())
        apvts.addParameterListener(id, this);
}

ABSlotManager::~ABSlotManager()
{
    for (const auto& id : getAllParameterIDs())
        apvts.removeParameterListener(id, this);
}

void ABSlotManager::parameterChanged(const juce::String& /*parameterID*/, float /*newValue*/)
{
    if (suppressModifiedUpdates) return;
    modified[(int) active] = true;
    // Any user edit leaves designer-authored territory.
    designerAuthored = false;
}

void ABSlotManager::snapTo(Slot target)
{
    if (target == active) return;

    // Capture discrete-param values from the active slot's STORED tree (before
    // the commit overwrites it) so we can restore them after loading the target.
    // This preserves the currently-active slot's discrete settings in the live
    // state, preventing discrete params from flipping on A/B snap.
    //
    // NOTE: The plan specified reading from p->getValue() (the live atomic) here.
    // However, that approach is broken for the case where the user has already
    // set a live discrete value to configure the TARGET slot before snapping back.
    // In that scenario, the live atomic reflects the target slot's desired value,
    // not the active slot's discrete setting — so "preserving" it defeats the
    // entire purpose. Reading from slots[(int) active] (the last-committed tree for
    // the active slot) correctly captures what the active slot's discrete param was.
    const bool preserveDiscrete = !designerAuthored && !includeDiscreteInSnap;
    std::map<juce::String, float> savedDiscreteNorm;
    if (preserveDiscrete)
    {
        const auto& activeTree = slots[(int) active];
        for (const auto& id : discreteParamIDs())
        {
            auto child = activeTree.getChildWithProperty("id", juce::var(id));
            if (child.isValid())
            {
                if (auto* p = apvts.getParameter(id))
                {
                    const float denorm = (float) child.getProperty("value", 0.0f);
                    savedDiscreteNorm[id] = p->convertTo0to1(denorm);
                }
            }
        }
    }

    slots[(int) active] = apvts.copyState();

    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        apvts.replaceState(slots[(int) target]);
    }

    if (preserveDiscrete)
    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        for (const auto& [id, normValue] : savedDiscreteNorm)
        {
            if (auto* p = apvts.getParameter(id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(normValue);
                p->endChangeGesture();
            }
        }
    }

    active = target;
}
void ABSlotManager::copy(Slot from, Slot to)
{
    if (from == to) return;

    // If source is active, commit in-flight edits first so `from` is current.
    if (from == active)
        slots[(int) active] = apvts.copyState();

    slots[(int) to] = slots[(int) from].createCopy();
    modified[(int) to] = false;
    presetRef[(int) to] = presetRef[(int) from];
}
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
