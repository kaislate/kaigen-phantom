// Source/UI/widgets/MatrixCell.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ModSlot.h"  // for Type enum (color coding)

class PhantomProcessor;

namespace kaigen::phantom
{

/** Single cell in the modulation matrix grid.
 *  Reads routing depth from ModulationEngine via paint-time lookups.
 *  Click → AddRouting at +0.5 (Task 6). Drag → SetRoutingDepth (Task 6).
 *  Right-click → quick-set + remove popover (Task 6). */
class MatrixCell : public juce::Component
{
public:
    MatrixCell(PhantomProcessor& processor,
               const juce::String& sourceId,    // e.g., "macro1"
               const juce::String& paramId,     // e.g., "a_ghost"
               ModSlot::Type type);
    ~MatrixCell() override;

    /** Modulator id — used by Task 7 for live cell pulse. */
    const juce::String& getSourceId() const noexcept { return sourceId; }

    /** Param id — used by Task 7 for cell lookup. */
    const juce::String& getParamId() const noexcept { return paramId; }

    /** Live modulation contribution — used by Task 7 to drive cell glow.
     *  Setting this triggers a repaint if the value changed meaningfully. */
    void setLiveContribution(float value);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    /** Read current routing depth from ModulationEngine. Returns 0 if no
     *  routing exists. */
    float currentDepth() const;

    /** Show right-click popover with quick-set + remove options. */
    void showPopover();

    PhantomProcessor& processor;
    juce::String sourceId;
    juce::String paramId;
    ModSlot::Type type;
    float liveContribution { 0.0f };

    float dragStartDepth { 0.0f };
    int   dragStartY     { 0 };
    bool  dragArmed      { false };  // set by left-click on a routed cell

    static constexpr float kRoutedThreshold = 0.001f;  // |depth| below this == effectively unrouted
    static constexpr float kDragPxPerUnit   = 100.0f;  // 100 logical px = full ±1.0 sweep

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixCell)
};

} // namespace kaigen::phantom
