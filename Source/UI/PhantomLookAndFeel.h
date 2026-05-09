// Source/UI/PhantomLookAndFeel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Custom LookAndFeel matching the WebView2 CSS aesthetic.
 *
 *  Knobs: silver convex disc (radial gradient at 35%/30%) + offset shadows
 *         + inset rim + colored indicator arc + white tick mark.
 *  Buttons: silver neumorphic raised pill — TextButton variants.
 *  Text editors: silver edge + dark text (used by the macro name editor).
 *  Popup menus: dark `#15181d` body + teal accent border + hover rows.
 *
 *  Set on the editor at construction; child widgets inherit it via JUCE's
 *  parent-chain LookAndFeel resolution.  */
class PhantomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PhantomLookAndFeel();
    ~PhantomLookAndFeel() override = default;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomLookAndFeel)
};

} // namespace kaigen::phantom
