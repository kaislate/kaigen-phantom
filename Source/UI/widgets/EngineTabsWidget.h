// Source/UI/widgets/EngineTabsWidget.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../EngineFocus.h"

class PhantomProcessor;

namespace kaigen::phantom
{

/** Three-tab pill control: A | B | LINK. Lives in the TopBar; matches the
 *  webview's .engine-tabs CSS (dark blue-tinted container, accent-blue
 *  active/hover states). LINK is a separate toggle on the right.
 *
 *  Reads / writes EngineFocus on the processor. Knob attachments don't
 *  rebind yet — that's a follow-up refactor. For now switching A↔B updates
 *  the persisted focus state + this widget's appearance only.
 */
class EngineTabsWidget : public juce::Component
{
public:
    explicit EngineTabsWidget(PhantomProcessor& processor);
    ~EngineTabsWidget() override;

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;

    /** Caller-defined natural width for layout. Container is sized
     *  generously so labels (A / B / LINK) breathe at the bumped font. */
    // Scaled ~1.4× from CSS spec so it matches the webview's rendered size.
    static constexpr int kNaturalWidth  = 188;
    static constexpr int kNaturalHeight = 36;

private:
    enum class Hit { None, TabA, TabB, Link };
    Hit hitTest(juce::Point<int> p) const;

    juce::Rectangle<int> tabRect(Hit which) const;

    PhantomProcessor& processor;
    Hit               hoverHit { Hit::None };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EngineTabsWidget)
};

} // namespace kaigen::phantom
