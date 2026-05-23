// Source/UI/widgets/RecipeSlotPills.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Save + delete pills engraved beneath the WordSelector's Cust 1/2/3
 *  entries. Renders a single row of 6 etched-word "pills":
 *
 *      [ SAVE  DEL ]   [ SAVE  DEL ]   [ SAVE  DEL ]
 *           Cust 1          Cust 2          Cust 3
 *
 *  Each pill is an etched-text word matching the WordSelector aesthetic
 *  (white shadow below + dark body). State is conveyed by the body
 *  colour:
 *
 *    SAVE pill (per Cust slot):
 *      - dim grey   when this slot is not the active preset
 *      - bright green when active + filled + clean
 *      - blinking red (~2 Hz) when active + (DIRTY or empty-with-pending-edits)
 *
 *    DEL pill (per Cust slot):
 *      - very dim grey when slot is empty (no-op click)
 *      - red-tinted etched when slot is filled (clickable)
 *
 *  Click handling delegates to PhantomProcessor::saveRecipeSlot /
 *  clearRecipeSlot. State is polled on a 4 Hz timer (drives blink). */
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

    /** Per-slot pill bounds, computed in resized(). Each Custom slot has
     *  a SAVE pill (left half of its column) and a DEL pill (right half). */
    std::array<juce::Rectangle<int>, 3> saveBounds;
    std::array<juce::Rectangle<int>, 3> deleteBounds;

    PhantomProcessor& processor;
    bool blinkPhase { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeSlotPills)
};

} // namespace kaigen::phantom
