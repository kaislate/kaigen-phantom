// Source/UI/widgets/ModSlot.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PhantomKnob.h"

namespace kaigen::phantom
{

/** Single slot in the modulation panel's slot row.
 *  Variants:
 *    - macro: contains a PhantomKnob bound to macroN APVTS param
 *    - morph: PhantomKnob bound to morph_amount
 *    - lfo / random: static placeholder (greyed text label)
 *
 *  Click anywhere on the slot fires onSlotClicked(slotId) — Task 7 wires
 *  this to the slot→matrix handoff. */
class ModSlot : public juce::Component
{
public:
    enum class Type { Macro, Morph, Lfo, Random };

    /** Constructor.
     *  @param apvts        APVTS for parameter binding (macro/morph types)
     *  @param type         Slot type
     *  @param slotId       e.g., "macro1", "morph", "lfo1", "randomA"
     *  @param paramID      APVTS param to bind (macro/morph types only; "" for placeholders)
     *  @param label        Display label, e.g., "MAC 1", "MORPH", "LFO 1"
     *  @param placeholder  Optional placeholder text shown for non-functional slots */
    ModSlot(juce::AudioProcessorValueTreeState& apvts,
            Type type,
            const juce::String& slotId,
            const juce::String& paramID,
            const juce::String& label,
            const juce::String& placeholder = {});

    ~ModSlot() override;

    /** Fires when the slot is clicked. Argument: slotId. */
    std::function<void(juce::String)> onSlotClicked;

    /** Slot id (e.g., "macro1") — used for matrix-row highlight in Task 7. */
    const juce::String& getSlotId() const noexcept { return slotId; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    Type type;
    juce::String slotId;
    juce::String label;
    juce::String placeholder;

    std::unique_ptr<PhantomKnob> knob;  // only for macro/morph

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModSlot)
};

} // namespace kaigen::phantom
