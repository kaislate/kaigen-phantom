// Source/UI/widgets/RecipeSlotPills.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Save + delete pills sitting beneath the WordSelector's Cust 1/2/3
 *  entries. Renders three rows (one per Custom slot); each row has two
 *  small circular dots:
 *
 *      [ save-dot ]  [ delete-dot ]
 *
 *  Save dot:
 *    - grey   when this slot is not the active preset
 *    - green  solid when active preset + filled + clean
 *    - red    blinking (~2 Hz) when active preset + (DIRTY or empty-with-pending-edits)
 *
 *  Delete dot:
 *    - grey   when slot is empty
 *    - red    when slot is filled (clickable)
 *
 *  Click handling delegates to PhantomProcessor::saveRecipeSlot /
 *  clearRecipeSlot. State is polled on a 4 Hz timer (blink). */
class RecipeSlotPills : public juce::Component, private juce::Timer
{
public:
    explicit RecipeSlotPills(PhantomProcessor& processor);
    ~RecipeSlotPills() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    /** Returns 0..2 for which row contains the point, or -1. */
    int rowAtY(int y) const noexcept;

    PhantomProcessor& processor;

    // Per-row geometry, computed in resized().
    std::array<juce::Rectangle<int>, 3> saveDots;
    std::array<juce::Rectangle<int>, 3> deleteDots;

    bool blinkPhase { false };   // toggled at 2 Hz by the 4 Hz timer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeSlotPills)
};

} // namespace kaigen::phantom
