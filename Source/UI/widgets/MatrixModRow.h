// Source/UI/widgets/MatrixModRow.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ModSlot.h"  // reuses ModSlot::Type enum

class PhantomProcessor;

namespace kaigen::phantom
{

/** Single modulator row in the MatrixView's left strip.
 *  Shows: type-color icon dot + modulator name + value readout.
 *  Macro rows have an inline name editor (Task 7 wires).
 *  LFO/Random rows are greyed placeholders (PR4/PR5). */
class MatrixModRow : public juce::Component
{
public:
    MatrixModRow(PhantomProcessor& processor,
                 ModSlot::Type type,
                 const juce::String& modId,    // e.g., "macro1", "lfo1"
                 const juce::String& label);   // e.g., "MAC 1"
    ~MatrixModRow() override;

    /** Modulator id — used by Task 5 to compute cell positions. */
    const juce::String& getModId() const noexcept { return modId; }

    /** Update the value readout (Task 7 wires this to the live tick). */
    void setValueText(const juce::String& text);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PhantomProcessor& processor;
    ModSlot::Type type;
    juce::String modId;
    juce::String label;
    juce::String valueText { "0.00" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixModRow)
};

} // namespace kaigen::phantom
