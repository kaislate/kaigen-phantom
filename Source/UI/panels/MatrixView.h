// Source/UI/panels/MatrixView.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../widgets/MatrixModRow.h"

class PhantomProcessor;

namespace kaigen::phantom
{

/** Modulation matrix view — overlay component that fills the editor area
 *  when active. Houses ModulatorStrip (left) + DestinationGrid (right).
 *
 *  Toggled visible/hidden by ModulationPanel's MATRIX button. */
class MatrixView : public juce::Component
{
public:
    MatrixView(PhantomProcessor& processor,
               juce::AudioProcessorValueTreeState& apvts);
    ~MatrixView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void visibilityChanged() override;

private:
    juce::Rectangle<int> getCardBounds() const noexcept;

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    static constexpr int kCardMargin     = 30;
    static constexpr int kTitleBarHeight = 40;
    static constexpr int kStripWidth     = 160;

    juce::OwnedArray<MatrixModRow> modRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixView)
};

} // namespace kaigen::phantom
