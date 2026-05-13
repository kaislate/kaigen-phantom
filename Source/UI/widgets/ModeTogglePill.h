// Source/UI/widgets/ModeTogglePill.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace kaigen::phantom
{

/** Two-segment neumorphic pill toggle for the engine mode (EFFECT / RESYN).
 *  Matches the webview .mt / .mb CSS spec — depressed black-tinted track,
 *  active segment is a raised white pill.
 *
 *  Currently bound to a_mode only; task #14 (A/B engine rebinding) will
 *  swap to a_mode / b_mode based on the active engine tab.
 */
class ModeTogglePill : public juce::Component,
                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit ModeTogglePill(juce::AudioProcessorValueTreeState& apvts);
    ~ModeTogglePill() override;

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;

    // Scaled ~1.4× from CSS spec so the pill matches the webview's
    // rendered size at Live's plugin-window scaling.
    static constexpr int kNaturalWidth  = 240;
    static constexpr int kNaturalHeight = 36;

    /** Retarget to <activePrefix>mode. With mirror set (LINK mode), writes
     *  also flow to the other engine's mode. */
    void setEnginePrefix(const juce::String& activePrefix,
                          const juce::String& mirrorPrefix = {});

private:
    void parameterChanged(const juce::String& paramId, float newValue) override;
    juce::Rectangle<int> segmentBounds(int idx) const;
    int hitTest(juce::Point<int> p) const;        // 0 = EFFECT, 1 = RESYN, -1 = none

    juce::AudioProcessorValueTreeState& apvts;
    juce::String activeParamId   { "a_mode" };
    juce::String mirrorParamId;
    int hoverSegment { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModeTogglePill)
};

} // namespace kaigen::phantom
