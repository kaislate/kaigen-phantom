#include "PhantomLookAndFeel.h"

//==============================================================================
PhantomLookAndFeel::PhantomLookAndFeel()
{
    // Background / window
    setColour (juce::ResizableWindow::backgroundColourId,    PhantomColours::background);
    setColour (juce::DocumentWindow::backgroundColourId,     PhantomColours::background);

    // Sliders
    setColour (juce::Slider::rotarySliderFillColourId,       PhantomColours::phosphorWhite);
    setColour (juce::Slider::rotarySliderOutlineColourId,    PhantomColours::trackDim);
    setColour (juce::Slider::thumbColourId,                  PhantomColours::phosphorWhite);
    setColour (juce::Slider::trackColourId,                  PhantomColours::trackDim);
    setColour (juce::Slider::textBoxTextColourId,            PhantomColours::phosphorWhite);
    setColour (juce::Slider::textBoxOutlineColourId,         juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,      juce::Colours::transparentBlack);

    // Buttons
    setColour (juce::TextButton::buttonColourId,             PhantomColours::panelDark);
    setColour (juce::TextButton::buttonOnColourId,           PhantomColours::panelHighlight);
    setColour (juce::TextButton::textColourOffId,            PhantomColours::textDim);
    setColour (juce::TextButton::textColourOnId,             PhantomColours::phosphorWhite);

    // Combo box
    setColour (juce::ComboBox::backgroundColourId,           PhantomColours::panelDark);
    setColour (juce::ComboBox::outlineColourId,              PhantomColours::ridgeDark);
    setColour (juce::ComboBox::textColourId,                 PhantomColours::phosphorWhite);
    setColour (juce::ComboBox::arrowColourId,                PhantomColours::textDim);
    setColour (juce::PopupMenu::backgroundColourId,          PhantomColours::panelDark);
    setColour (juce::PopupMenu::textColourId,                PhantomColours::phosphorWhite);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, PhantomColours::ridgeDark);

    // Labels
    setColour (juce::Label::textColourId,                    PhantomColours::textDim);
    setColour (juce::Label::backgroundColourId,              juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId,                 juce::Colours::transparentBlack);

    // Toggle buttons
    setColour (juce::ToggleButton::textColourId,             PhantomColours::phosphorWhite);
    setColour (juce::ToggleButton::tickColourId,             PhantomColours::phosphorWhite);
    setColour (juce::ToggleButton::tickDisabledColourId,     PhantomColours::textDim);
}

//==============================================================================
// STATIC UTILITIES
//==============================================================================

void PhantomLookAndFeel::drawGlowText (juce::Graphics& g,
                                        const juce::String& text,
                                        juce::Rectangle<float> area,
                                        const juce::Font& font,
                                        juce::Justification justification)
{
    // Pass 1 — widest glow, 40 % opacity
    g.setColour (PhantomColours::textGlow.withAlpha (0.40f));
    g.setFont (font.withHeight (font.getHeight() * 1.06f));
    g.drawText (text, area.expanded (2.0f, 1.5f), justification, false);

    // Pass 2 — medium glow, 60 % opacity
    g.setColour (PhantomColours::textGlow.withAlpha (0.60f));
    g.setFont (font.withHeight (font.getHeight() * 1.03f));
    g.drawText (text, area.expanded (1.0f, 0.5f), justification, false);

    // Pass 3 — sharp, fully opaque
    g.setColour (PhantomColours::phosphorWhite);
    g.setFont (font);
    g.drawText (text, area, justification, false);
}

//------------------------------------------------------------------------------

void PhantomLookAndFeel::drawOledRidge (juce::Graphics& g,
                                         juce::Rectangle<float> circleBounds)
{
    // Ring 1 — ridgeBright highlight, 1.5 px, innermost
    {
        juce::Path ring;
        ring.addEllipse (circleBounds.reduced (0.75f));
        g.setColour (PhantomColours::ridgeBright);
        g.strokePath (ring, juce::PathStrokeType (1.5f));
    }

    // Ring 2 — ridgeDark shadow, 1.5 px, middle
    {
        juce::Path ring;
        ring.addEllipse (circleBounds.reduced (2.5f));
        g.setColour (PhantomColours::ridgeDark);
        g.strokePath (ring, juce::PathStrokeType (1.5f));
    }

    // Ring 3 — ridgeOuter very-faint, 1 px, outermost
    {
        juce::Path ring;
        ring.addEllipse (circleBounds.expanded (0.5f));
        g.setColour (PhantomColours::ridgeOuter);
        g.strokePath (ring, juce::PathStrokeType (1.0f));
    }
}

//==============================================================================
// PRIVATE UTILITIES
//==============================================================================

juce::String PhantomLookAndFeel::formatSliderValue (const juce::Slider& slider)
{
    const double value  = slider.getValue();
    const juce::String suffix = slider.getTextValueSuffix().trim();

    // If the slider already carries a non-empty suffix, use it directly.
    if (suffix.isNotEmpty())
    {
        // Round to a clean integer where possible
        const int rounded = juce::roundToInt (value);
        if (juce::approximatelyEqual ((double) rounded, value))
            return juce::String (rounded) + suffix;

        return juce::String (value, 1) + suffix;
    }

    // Heuristic: normalised 0-1 range → percentage
    const double rangeMin = slider.getMinimum();
    const double rangeMax = slider.getMaximum();

    if (juce::approximatelyEqual (rangeMin, 0.0) && juce::approximatelyEqual (rangeMax, 1.0))
    {
        const int pct = juce::roundToInt (value * 100.0);
        return juce::String (pct) + "%";
    }

    // Round integers
    const int rounded = juce::roundToInt (value);
    if (juce::approximatelyEqual ((double) rounded, value))
        return juce::String (rounded);

    return juce::String (value, 1);
}

//==============================================================================
// ROTARY SLIDER
//==============================================================================

void PhantomLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPosProportional,
                                            float rotaryStartAngle,
                                            float rotaryEndAngle,
                                            juce::Slider& slider)
{
    using namespace juce;

    // ---- sizing -----------------------------------------------------------
    const auto* phantomKnob   = dynamic_cast<const PhantomKnob*> (&slider);
    const bool  isLarge       = (phantomKnob != nullptr &&
                                 phantomKnob->getKnobSize() == PhantomKnob::Size::Large);

    // Reserve vertical space for the label below the circle
    constexpr float kLabelHeight  = 14.0f;
    constexpr float kLabelPad     =  3.0f;

    const float availableH = (float) height - kLabelHeight - kLabelPad;
    const float diameter   = jmin ((float) width, availableH);
    const float radius     = diameter * 0.5f;
    const float cx         = (float) x + (float) width  * 0.5f;
    const float cy         = (float) y + availableH     * 0.5f;

    const auto knobBounds  = Rectangle<float> (cx - radius, cy - radius, diameter, diameter);

    // ---- arc angles -------------------------------------------------------
    const float angle      = rotaryStartAngle +
                             sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // ---- arc track radius (sits just outside the OLED well) ---------------
    const float trackR     = radius - 5.0f;

    // ================================================================
    // LAYER 1 — Neumorphic outer shadows
    // ================================================================
    {
        // Highlight — top-left offset
        DropShadow hlShadow (PhantomColours::panelShadowLight.withAlpha (0.55f), 12, { -5, -5 });
        {
            juce::Path ellipsePath;
            ellipsePath.addEllipse (knobBounds);
            hlShadow.drawForPath (g, ellipsePath);
        }

        // Dark drop shadow — bottom-right offset
        DropShadow dkShadow (PhantomColours::panelShadowDark.withAlpha (0.85f), 16, { 6, 6 });
        {
            juce::Path ellipsePath;
            ellipsePath.addEllipse (knobBounds);
            dkShadow.drawForPath (g, ellipsePath);
        }
    }

    // ================================================================
    // LAYER 2 — Volcano surface (radial gradient)
    //           Bright at ~32 deg from top-left, dark at bottom-right
    // ================================================================
    {
        // Light source: upper-left quadrant
        const float lightOffsetX = -radius * 0.55f;
        const float lightOffsetY = -radius * 0.55f;
        const float lightX       = cx + lightOffsetX;
        const float lightY       = cy + lightOffsetY;

        // Base surface tones  — very dark with a faint warm tint
        const Colour surfaceBright { 0xff1e1c22 };
        const Colour surfaceDark   { 0xff080609 };

        ColourGradient grad (surfaceBright, lightX, lightY,
                             surfaceDark,   cx + radius * 0.5f, cy + radius * 0.5f,
                             true);  // radial
        g.setGradientFill (grad);
        g.fillEllipse (knobBounds);
    }

    // ================================================================
    // LAYER 3 — OLED well (solid black inset circle)
    // ================================================================
    const float oledInset = isLarge ? 13.0f : 9.0f;
    const auto  oledBounds = knobBounds.reduced (oledInset);

    g.setColour (PhantomColours::oledBlack);
    g.fillEllipse (oledBounds);

    // ================================================================
    // LAYER 4 — Lip / ridge  (three concentric stroked circles)
    // ================================================================
    drawOledRidge (g, knobBounds);

    // ================================================================
    // LAYER 5 — Arc track  (full sweep, dim)
    // ================================================================
    {
        Path trackPath;
        trackPath.addCentredArc (cx, cy, trackR, trackR,
                                  0.0f,
                                  rotaryStartAngle, rotaryEndAngle,
                                  true);
        g.setColour (PhantomColours::trackDim);
        g.strokePath (trackPath, PathStrokeType (3.5f,
                                                  PathStrokeType::curved,
                                                  PathStrokeType::rounded));
    }

    // ================================================================
    // LAYER 6 — Arc glow  (value arc, 30 % opacity, 6 px halo)
    // ================================================================
    if (sliderPosProportional > 0.0f)
    {
        Path glowPath;
        glowPath.addCentredArc (cx, cy, trackR, trackR,
                                 0.0f,
                                 rotaryStartAngle, angle,
                                 true);
        g.setColour (PhantomColours::phosphorWhite.withAlpha (0.30f));
        g.strokePath (glowPath, PathStrokeType (6.0f,
                                                 PathStrokeType::curved,
                                                 PathStrokeType::rounded));
    }

    // ================================================================
    // LAYER 7 — Arc value  (value arc, phosphor white, 2.8 px sharp)
    // ================================================================
    if (sliderPosProportional > 0.0f)
    {
        Path valuePath;
        valuePath.addCentredArc (cx, cy, trackR, trackR,
                                  0.0f,
                                  rotaryStartAngle, angle,
                                  true);
        g.setColour (PhantomColours::phosphorWhite);
        g.strokePath (valuePath, PathStrokeType (2.8f,
                                                  PathStrokeType::curved,
                                                  PathStrokeType::rounded));
    }

    // ================================================================
    // LAYER 8 — Value text  (3-pass glow, Courier New monospace bold)
    // ================================================================
    {
        const juce::String valueText = formatSliderValue (slider);

        const float valueFontSize = isLarge ? 11.5f : 9.5f;
        const Font  valueFont ("Courier New", valueFontSize, Font::bold);

        // Centre the text in the OLED well
        const auto textArea = oledBounds.reduced (2.0f);
        drawGlowText (g, valueText, textArea, valueFont, Justification::centred);
    }

    // ================================================================
    // LAYER 9 — Label text  (slider name, small-caps style, textDim)
    // ================================================================
    {
        const juce::String labelText = slider.getName().toUpperCase();
        const float labelFontSize    = 5.0f;
        const Font  labelFont (labelFontSize);

        const float labelTop = (float) y + availableH + kLabelPad;
        const auto  labelArea = Rectangle<float> ((float) x, labelTop,
                                                   (float) width, kLabelHeight);

        g.setColour (PhantomColours::textDim);
        g.setFont (labelFont);
        g.drawText (labelText, labelArea, Justification::centredTop, false);
    }
}

//==============================================================================
// BUTTON BACKGROUND
//==============================================================================

void PhantomLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour& /*backgroundColour*/,
                                                bool /*shouldDrawButtonAsHighlighted*/,
                                                bool shouldDrawButtonAsDown)
{
    using namespace juce;

    const auto  bounds  = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner  = 5.0f;
    const bool  toggled = button.getToggleState();
    const bool  active  = toggled || shouldDrawButtonAsDown;

    if (active)
    {
        // Raised neumorphic look
        {
            DropShadow hlShadow (PhantomColours::panelShadowLight.withAlpha (0.45f), 10, { -3, -3 });
            hlShadow.drawForRectangle (g, button.getLocalBounds().reduced (2));
        }
        {
            DropShadow dkShadow (PhantomColours::panelShadowDark.withAlpha (0.75f), 14, { 4, 4 });
            dkShadow.drawForRectangle (g, button.getLocalBounds().reduced (2));
        }

        // Slight bright fill for the toggled state
        g.setColour (PhantomColours::panelHighlight.withAlpha (0.22f));
        g.fillRoundedRectangle (bounds, corner);

        // Base fill
        g.setColour (PhantomColours::panelDark.brighter (0.06f));
        g.fillRoundedRectangle (bounds, corner);

        // Bright top-left edge
        ColourGradient edgeGrad (PhantomColours::ridgeBright, bounds.getX(), bounds.getY(),
                                  Colours::transparentBlack,
                                  bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (edgeGrad);
        g.fillRoundedRectangle (bounds, corner);
    }
    else
    {
        // Inset / sunken look
        // Base fill
        g.setColour (PhantomColours::panelDark);
        g.fillRoundedRectangle (bounds, corner);

        // Dark gradient overlay (sunken effect — dark at top-left)
        ColourGradient sunkenGrad (Colour (0x40000000), bounds.getX(), bounds.getY(),
                                    Colours::transparentBlack,
                                    bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (sunkenGrad);
        g.fillRoundedRectangle (bounds, corner);
    }

    // Outline ridge
    g.setColour (active ? PhantomColours::ridgeBright : PhantomColours::ridgeDark);
    g.drawRoundedRectangle (bounds, corner, 0.8f);
}

//==============================================================================
// BUTTON TEXT
//==============================================================================

void PhantomLookAndFeel::drawButtonText (juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool /*shouldDrawButtonAsHighlighted*/,
                                          bool /*shouldDrawButtonAsDown*/)
{
    using namespace juce;

    const bool   toggled   = button.getToggleState();
    const float  fontSize  = jmin (15.0f, (float) button.getHeight() * 0.55f);
    const Font   font (fontSize);
    const String label     = button.getButtonText();

    const auto textArea = button.getLocalBounds().toFloat().reduced (4.0f, 2.0f);

    if (toggled)
    {
        // Bright phosphor glow when active
        drawGlowText (g, label, textArea, font, Justification::centred);
    }
    else
    {
        // Dim when off
        g.setColour (PhantomColours::textDim);
        g.setFont (font);
        g.drawText (label, textArea, Justification::centred, false);
    }
}
