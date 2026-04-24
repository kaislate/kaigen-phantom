#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

class ABSlotManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class Slot { A = 0, B = 1 };

    explicit ABSlotManager(juce::AudioProcessorValueTreeState& apvts);
    ~ABSlotManager() override;

    // Snap/copy operations
    void snapTo(Slot target);
    void copy(Slot from, Slot to);
    Slot getActive() const noexcept { return active; }

    // Modified-indicator state
    bool isModified(Slot s) const noexcept;
    juce::String getPresetRef(Slot s) const;

    // Plugin state persistence (called from getStateInformation/setStateInformation)
    void syncActiveSlotFromLive();
    juce::ValueTree toStateTree() const;
    void fromStateTree(const juce::ValueTree& abSlotsTree);

    // Designer-authored preset application
    void loadSinglePresetIntoActive(const juce::ValueTree& presetState,
                                    const juce::String& presetRef);
    void loadABPreset(const juce::ValueTree& presetRootState,
                      const juce::String& presetRef);

    // Called by PresetManager::savePreset when building the tree to write
    juce::ValueTree buildPresetSlotBChild() const;

    // Setting accessors
    bool getIncludeDiscreteInSnap() const noexcept { return includeDiscreteInSnap; }
    void setIncludeDiscreteInSnap(bool on) noexcept { includeDiscreteInSnap = on; }

    // Read-only slot access (Pro MorphEngine seam)
    const juce::ValueTree& getSlot(Slot s) const noexcept { return slots[(int) s]; }

    // Returns the four discrete param IDs that are excluded from ad-hoc snaps
    // when includeDiscreteInSnap == false. Public for tests.
    static const juce::StringArray& discreteParamIDs();

    // Returns a RAII guard that sets suppressModifiedUpdates=true for its lifetime.
    // Intended for external collaborators (like MorphEngine) that need to drive
    // APVTS without the listener marking the active slot modified.
    [[nodiscard]] juce::ScopedValueSetter<bool> scopedSuppressModified();

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState& apvts;
    juce::ValueTree slots[2];
    Slot active = Slot::A;
    bool designerAuthored = false;
    bool includeDiscreteInSnap = false;

    bool modified[2] { false, false };
    juce::String presetRef[2];

    // Guards the APVTS listener from firing during internal snap/copy/load.
    bool suppressModifiedUpdates = false;
};

} // namespace kaigen::phantom
