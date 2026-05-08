// Source/UI/panels/ModulationPanel.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../widgets/ModSlot.h"

class PhantomProcessor;

namespace kaigen::phantom
{

/** Bottom panel housing the modulation system UI:
 *  - Mode bar: SLOTS / MATRIX toggle buttons + counter
 *  - Slot row: 11 modulator slots (Task 2 wires)
 *  - Matrix view: overlay component (Task 3 wires)
 *
 *  Replaces the WebView2 modulation-panel.js + matrix.js. */
class ModulationPanel : public juce::Component
{
public:
    ModulationPanel(PhantomProcessor& processor,
                    juce::AudioProcessorValueTreeState& apvts);
    ~ModulationPanel() override;

    /** Fires when the user clicks MATRIX. Boolean argument: new active state. */
    std::function<void(bool)> onMatrixToggle;

    /** True if MATRIX mode is active (slot row collapsed, mode bar only). */
    bool isMatrixActive() const noexcept { return matrixActive; }

    /** Mode-bar-only height when slot row is collapsed (matrix mode). */
    static constexpr int kCollapsedHeight = 38;

    /** Update the routing counter (called by NativePluginEditor on state change). */
    void setRoutingCount(int count);

    /** Allows external code (NativePluginEditor) to install onSlotClicked
     *  handlers on each slot. */
    juce::OwnedArray<ModSlot>& getSlots() noexcept { return slots; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton slotsButton  { "SLOTS"  };
    juce::TextButton matrixButton { "MATRIX" };
    juce::Label      counterLabel;
    bool matrixActive { false };

    juce::OwnedArray<ModSlot> slots;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationPanel)
};

} // namespace kaigen::phantom
