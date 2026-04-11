#pragma once
#include <JuceHeader.h>
#include "PhantomColours.h"
#include "PhantomKnob.h"

//==============================================================================
/**  PhantomLookAndFeel
 *
 *   Central custom rendering class for the Kaigen Phantom plugin UI.
 *   Provides neumorphic / OLED-inspired visuals for all controls.
 */
class PhantomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PhantomLookAndFeel();
    ~PhantomLookAndFeel() override = default;

    //==========================================================================
    // Rotary slider (PhantomKnob)
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    //==========================================================================
    // Buttons
    void drawButtonBackground (juce::Graphics& g,
                                juce::Button& button,
                                const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted,
                                bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    //==========================================================================
    // Static utilities (also used by RecipeWheel and other components)

    /** 3-pass white glow text — pass 40 % expanded, 60 % slightly expanded, 100 % sharp. */
    static void drawGlowText (juce::Graphics& g,
                               const juce::String& text,
                               juce::Rectangle<float> area,
                               const juce::Font& font,
                               juce::Justification justification = juce::Justification::centred);

    /** Triple-ring OLED lip/ridge — ridgeBright 1.5 px, ridgeDark 1.5 px, ridgeOuter 1 px. */
    static void drawOledRidge (juce::Graphics& g,
                                juce::Rectangle<float> circleBounds);

private:
    //==========================================================================
    /** Format a slider value for display (e.g. "100%", "0dB", "20ms"). */
    static juce::String formatSliderValue (const juce::Slider& slider);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhantomLookAndFeel)
};
