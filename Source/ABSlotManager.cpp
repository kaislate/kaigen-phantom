#include "ABSlotManager.h"
#include "Parameters.h"
#include <map>

namespace
{
    constexpr const char* kABSlotsNodeId    = "ABSlots";
    constexpr const char* kSlotNodeId       = "Slot";
    constexpr const char* kActiveAttr       = "active";
    constexpr const char* kIncludeDiscAttr  = "includeDiscrete";
    constexpr const char* kSlotNameAttr     = "name";
}

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

void ABSlotManager::syncActiveSlotFromLive()
{
    slots[(int) active] = apvts.copyState();
}

juce::ValueTree ABSlotManager::toStateTree() const
{
    juce::ValueTree tree { kABSlotsNodeId };
    tree.setProperty(kActiveAttr, active == Slot::A ? "A" : "B", nullptr);
    tree.setProperty(kIncludeDiscAttr, includeDiscreteInSnap ? 1 : 0, nullptr);

    for (int i = 0; i < 2; ++i)
    {
        juce::ValueTree slotNode { kSlotNodeId };
        slotNode.setProperty(kSlotNameAttr, i == 0 ? "A" : "B", nullptr);
        if (slots[i].isValid())
            slotNode.appendChild(slots[i].createCopy(), nullptr);
        tree.appendChild(slotNode, nullptr);
    }
    return tree;
}

void ABSlotManager::fromStateTree(const juce::ValueTree& abSlotsTree)
{
    if (!abSlotsTree.isValid() || abSlotsTree.getType().toString() != kABSlotsNodeId)
    {
        slots[0] = apvts.copyState();
        slots[1] = apvts.copyState();
        active = Slot::A;
        includeDiscreteInSnap = false;
        designerAuthored = false;
        modified[0] = modified[1] = false;
        presetRef[0] = presetRef[1] = {};
        return;
    }

    const auto activeStr = abSlotsTree.getProperty(kActiveAttr, juce::var("A")).toString();
    active = (activeStr == "B") ? Slot::B : Slot::A;
    includeDiscreteInSnap = ((int) abSlotsTree.getProperty(kIncludeDiscAttr, 0)) != 0;

    for (int i = 0; i < abSlotsTree.getNumChildren(); ++i)
    {
        auto slotNode = abSlotsTree.getChild(i);
        if (slotNode.getType().toString() != kSlotNodeId) continue;
        const auto nameStr = slotNode.getProperty(kSlotNameAttr).toString();
        const int idx = (nameStr == "B") ? 1 : 0;
        if (slotNode.getNumChildren() > 0)
            slots[idx] = slotNode.getChild(0).createCopy();
    }

    // Default/empty-slot fallback: fill with live state so they're always valid.
    if (!slots[0].isValid()) slots[0] = apvts.copyState();
    if (!slots[1].isValid()) slots[1] = apvts.copyState();

    designerAuthored = false;
    modified[0] = modified[1] = false;
    presetRef[0] = presetRef[1] = {};
}

void ABSlotManager::loadSinglePresetIntoActive(const juce::ValueTree& presetState,
                                               const juce::String& ref)
{
    if (!presetState.isValid() || presetState.getType() != apvts.state.getType())
        return;

    slots[(int) active] = presetState.createCopy();

    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        apvts.replaceState(slots[(int) active]);
    }

    modified[(int) active] = false;
    presetRef[(int) active] = ref;
    designerAuthored = false;
}
void ABSlotManager::loadABPreset(const juce::ValueTree& presetRootState,
                                 const juce::String& ref)
{
    if (!presetRootState.isValid() || presetRootState.getType() != apvts.state.getType())
        return;

    // Slot A = root state, WITHOUT <SlotB> or <MorphConfig> children.
    auto slotA = presetRootState.createCopy();
    if (auto existing = slotA.getChildWithName("SlotB"); existing.isValid())
        slotA.removeChild(existing, nullptr);
    if (auto existingMorph = slotA.getChildWithName("MorphConfig"); existingMorph.isValid())
        slotA.removeChild(existingMorph, nullptr);
    slots[0] = slotA;

    // Slot B = contents of <SlotB>'s first child, if present. Otherwise leave empty
    // (treated as "A/B preset with empty B" — unusual but not an error).
    const auto slotBChild = presetRootState.getChildWithName("SlotB");
    if (slotBChild.isValid() && slotBChild.getNumChildren() > 0)
        slots[1] = slotBChild.getChild(0).createCopy();
    else
        slots[1] = slots[0].createCopy();

    {
        const juce::ScopedValueSetter<bool> guard { suppressModifiedUpdates, true };
        apvts.replaceState(slots[(int) active]);
    }

    modified[0] = modified[1] = false;
    presetRef[0] = presetRef[1] = ref;
    designerAuthored = true;
}

juce::ValueTree ABSlotManager::buildPresetSlotBChild() const
{
    juce::ValueTree child { "SlotB" };
    if (slots[1].isValid())
        child.appendChild(slots[1].createCopy(), nullptr);
    return child;
}

const juce::StringArray& ABSlotManager::discreteParamIDs()
{
    static const juce::StringArray ids { ParamID::MODE, ParamID::BYPASS,
                                         ParamID::GHOST_MODE, ParamID::BINAURAL_MODE };
    return ids;
}

juce::ScopedValueSetter<bool> ABSlotManager::scopedSuppressModified()
{
    return juce::ScopedValueSetter<bool> { suppressModifiedUpdates, true };
}

} // namespace kaigen::phantom
