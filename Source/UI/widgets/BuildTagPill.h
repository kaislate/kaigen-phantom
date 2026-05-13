// Source/UI/widgets/BuildTagPill.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Tiny blue rounded-rect tag at the right end of the TopBar carrying a
 *  build identifier (e.g., "MORPH-9"). Matches the webview .build-tag CSS:
 *  #4A8DD5 fill, bold 9 px white text, +0.8 letter-spacing, 3 px corner,
 *  subtle inset top highlight + outer drop shadow.
 *
 *  Used by ops/QA to verify the right DLL is loaded. Text is set at
 *  construction. Width auto-fits the text + padding.
 */
class BuildTagPill : public juce::Component
{
public:
    /** Background colour override — the MORPH build tag uses the
     *  webview's blue (#4A8DD5); the DSP status tag uses green (#0a0).
     *  Default is the blue variant. */
    explicit BuildTagPill(juce::String tagText,
                          juce::Colour bg = juce::Colour(0xff4a8dd5));
    ~BuildTagPill() override = default;

    /** Natural size for the configured text, used by the parent's layout
     *  pass. Caller can ignore and assign explicit bounds if preferred. */
    juce::Rectangle<int> getNaturalBounds() const;
    static constexpr int kNaturalHeight = 16;

    void paint(juce::Graphics& g) override;

private:
    juce::String text;
    juce::Colour bgColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BuildTagPill)
};

} // namespace kaigen::phantom
