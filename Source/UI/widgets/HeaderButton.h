// Source/UI/widgets/HeaderButton.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace kaigen::phantom
{

/** Circular neumorphic header button matching the webview .hdr-btn CSS.
 *  Used in the TopBar for bypass / settings / advanced. Each instance
 *  paints a specific icon (set at construction) and either binds to an
 *  APVTS bool param (for toggleable buttons) or fires onClick (for the
 *  inert ones until their panels land).
 */
class HeaderButton : public juce::Component,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class Icon { Bypass, Settings, AdvancedChevron };

    /** Bind to a bool param via the APVTS, e.g. "bypass" or "advanced_open". */
    HeaderButton(juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& boolParamId,
                 Icon icon);

    /** Paint-only ctor — click fires onClick instead of toggling an APVTS
     *  param. */
    HeaderButton(Icon icon);

    ~HeaderButton() override;

    std::function<void()> onClick;

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override;

    static constexpr int kNaturalSize = 42;   // CSS 30 × 1.4 for webview-scale parity

private:
    void parameterChanged(const juce::String& paramId, float newValue) override;
    bool isActive() const;

    juce::AudioProcessorValueTreeState* apvts { nullptr };
    juce::String boundParam;
    Icon icon;
    bool hover { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderButton)
};

} // namespace kaigen::phantom
