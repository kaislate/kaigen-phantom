// Source/UI/PhantomLookAndFeel.cpp
#include "PhantomLookAndFeel.h"
#include "Theme.h"
#include "PhantomNativeAssets.h"
#include <juce_graphics/juce_graphics.h>

namespace kaigen::phantom
{

PhantomLookAndFeel::PhantomLookAndFeel()
{
    // Load all bundled Space Grotesk weight variants once at startup. They
    // persist for the LookAndFeel's lifetime; getTypefaceForFont picks among
    // them based on the requested style.
    lightTypeface    = juce::Typeface::createSystemTypefaceFor(
        PhantomNativeAssets::SpaceGroteskLight_ttf,
        PhantomNativeAssets::SpaceGroteskLight_ttfSize);
    regularTypeface  = juce::Typeface::createSystemTypefaceFor(
        PhantomNativeAssets::SpaceGroteskRegular_ttf,
        PhantomNativeAssets::SpaceGroteskRegular_ttfSize);
    mediumTypeface   = juce::Typeface::createSystemTypefaceFor(
        PhantomNativeAssets::SpaceGroteskMedium_ttf,
        PhantomNativeAssets::SpaceGroteskMedium_ttfSize);
    semiBoldTypeface = juce::Typeface::createSystemTypefaceFor(
        PhantomNativeAssets::SpaceGroteskSemiBold_ttf,
        PhantomNativeAssets::SpaceGroteskSemiBold_ttfSize);
    boldTypeface     = juce::Typeface::createSystemTypefaceFor(
        PhantomNativeAssets::SpaceGroteskBold_ttf,
        PhantomNativeAssets::SpaceGroteskBold_ttfSize);

    // Make Space Grotesk the project-wide default sans-serif. Any code path
    // that doesn't explicitly name a typeface picks this up.
    setDefaultSansSerifTypeface(regularTypeface);

    // Default ColourId palette — widgets that don't get custom-painted still
    // pick up reasonable colors via these defaults.
    setColour(juce::Slider::rotarySliderFillColourId,    Theme::accentBlue);
    setColour(juce::Slider::rotarySliderOutlineColourId, Theme::ringTrack);

    setColour(juce::Label::textColourId,                 Theme::textOnLightBody);

    setColour(juce::TextButton::buttonColourId,          juce::Colour(0x14000000));   // 8% black inactive bg
    setColour(juce::TextButton::buttonOnColourId,        juce::Colour(0x8cffffff));   // 55% white active bg
    setColour(juce::TextButton::textColourOffId,         Theme::textOnLightInactive); // ~28% black
    setColour(juce::TextButton::textColourOnId,          Theme::textOnLightActive);   // ~62% black

    setColour(juce::TextEditor::backgroundColourId,      Theme::mtxPopoverBg);
    setColour(juce::TextEditor::textColourId,            Theme::macroTeal);
    setColour(juce::TextEditor::outlineColourId,         Theme::macroTeal);
    setColour(juce::TextEditor::focusedOutlineColourId,  Theme::macroTeal);

    setColour(juce::PopupMenu::backgroundColourId,             Theme::mtxPopoverBg);
    setColour(juce::PopupMenu::textColourId,                   Theme::vizText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,  juce::Colour(0x2e5DD3E0));
    setColour(juce::PopupMenu::highlightedTextColourId,        Theme::macroTeal);
}

void PhantomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(backgroundColour);
    const auto bounds = b.getLocalBounds().toFloat().reduced(0.5f);
    const float corner = juce::jmin(bounds.getHeight() * 0.5f, 16.0f);

    const bool isOn = b.getToggleState() || shouldDrawButtonAsDown;
    const auto styleHint = b.getProperties().getWithDefault("phantom-style", "");
    const bool isHeaderRaised = styleHint == juce::var("header-raised");
    const bool isMtSegment    = styleHint == juce::var("mt-segment");
    const bool isHeaderGlyph  = styleHint == juce::var("header-glyph");

    if (isHeaderGlyph)
    {
        // No background — pure etched glyph. Hover painted in drawButtonText
        // via colour shift; nothing to fill here.
        return;
    }

    if (isMtSegment)
    {
        // .mt mode-toggle segment: inactive = no bg (the container pill shows
        // through); active = white-55% raised pill with subtle shadow.
        if (isOn)
        {
            g.setColour(juce::Colour(0x8cFFFFFF));   // 55% white
            g.fillRoundedRectangle(bounds, corner);

            // Soft drop shadow + top hairline highlight (matches CSS active).
            g.setColour(juce::Colour(0x1F000000));
            g.drawRoundedRectangle(bounds, corner, 0.6f);
            g.setColour(juce::Colour(0xa6FFFFFF));
            g.drawLine(bounds.getX() + corner, bounds.getY() + 0.5f,
                        bounds.getRight() - corner, bounds.getY() + 0.5f, 0.5f);
        }
        // Hover overlay (visible only when inactive).
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(0x14FFFFFF));
            g.fillRoundedRectangle(bounds, corner);
        }
        return;
    }

    if (isHeaderRaised)
    {
        // Raised neumorphic pill — silver-tinted body with offset highlights/shadows.
        // CSS reference: .hdr-btn — inset 1.5px 1.5px 4px rgba(0,0,0,0.14) +
        //                inset -1.5px -1.5px 3px rgba(255,255,255,0.48) + outer 1px drop shadow.
        if (shouldDrawButtonAsDown)
        {
            // Pressed: tinted active.
            g.setColour(juce::Colour(0x33000000));
            g.fillRoundedRectangle(bounds, corner);
        }
        else
        {
            // Body fill — slightly lighter than the silver panel so the pill reads as raised.
            juce::ColourGradient body(juce::Colour(0xffD8DADC), bounds.getX(), bounds.getY(),
                                        juce::Colour(0xffB6B8BA), bounds.getX(), bounds.getBottom(),
                                        false);
            g.setGradientFill(body);
            g.fillRoundedRectangle(bounds, corner);
        }

        // Top-left bright highlight (shows the pill is raised).
        g.setColour(juce::Colour(0xa0FFFFFF));
        g.drawLine(bounds.getX() + corner * 0.5f, bounds.getY() + 0.5f,
                    bounds.getRight() - corner * 0.5f, bounds.getY() + 0.5f, 0.7f);

        // Bottom-right soft shadow.
        g.setColour(juce::Colour(0x33000000));
        g.drawRoundedRectangle(bounds.reduced(0.25f), corner, 0.7f);

        if (shouldDrawButtonAsHighlighted && ! shouldDrawButtonAsDown)
        {
            g.setColour(juce::Colour(0x18FFFFFF));
            g.fillRoundedRectangle(bounds, corner);
        }
        return;
    }

    if (isOn)
    {
        // Active: white-55% raised pill with subtle shadow.
        g.setColour(juce::Colour(0x8cFFFFFF));
        g.fillRoundedRectangle(bounds, corner);

        // Soft drop shadow under active.
        g.setColour(juce::Colour(0x1F000000));
        g.drawRoundedRectangle(bounds, corner, 0.7f);

        // Top hairline highlight.
        g.setColour(juce::Colour(0xa6FFFFFF));   // ~65% white
        g.drawLine(bounds.getX() + corner, bounds.getY() + 0.5f,
                    bounds.getRight() - corner, bounds.getY() + 0.5f, 0.5f);
    }
    else
    {
        // Inactive: 8% black recessed pill with neumorphic inset.
        g.setColour(juce::Colour(0x14000000));
        g.fillRoundedRectangle(bounds, corner);

        // Inset top-left dark line.
        g.setColour(juce::Colour(0x24000000));
        g.drawRoundedRectangle(bounds.reduced(0.5f), corner - 0.5f, 0.7f);

        // Inset bottom-right white highlight.
        g.setColour(juce::Colour(0x80FFFFFF));
        g.drawLine(bounds.getX() + corner, bounds.getBottom() - 0.5f,
                    bounds.getRight() - corner, bounds.getBottom() - 0.5f, 0.5f);
    }

    // Hover overlay.
    if (shouldDrawButtonAsHighlighted && ! shouldDrawButtonAsDown)
    {
        g.setColour(juce::Colour(0x14FFFFFF));
        g.fillRoundedRectangle(bounds, corner);
    }
}

void PhantomLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    const bool isOn = b.getToggleState() || shouldDrawButtonAsDown;
    const auto styleHint = b.getProperties().getWithDefault("phantom-style", "");
    const bool isHeaderRaised = styleHint == juce::var("header-raised");
    const bool isMtSegment    = styleHint == juce::var("mt-segment");
    const bool isHeaderGlyph  = styleHint == juce::var("header-glyph");

    juce::Colour colour;
    if (isHeaderGlyph)
    {
        // CSS .preset-controls buttons: color #656769 (rest), darker on hover.
        // 88% black on press, 70% on hover, ~40% (the spec #656769 = ~60%) at rest.
        colour = shouldDrawButtonAsDown        ? juce::Colour(0xe0000000)
               : shouldDrawButtonAsHighlighted ? juce::Colour(0xb3000000)
                                                : juce::Colour(0xff656769);
    }
    else if (isHeaderRaised)
        colour = Theme::textOnLightActive;   // ~62% black, readable on the silver pill
    else if (isMtSegment)
        colour = isOn ? juce::Colour(0x94000000)    // ~58% black active (CSS spec)
                      : juce::Colour(0x38000000);   // ~22% black inactive
    else
        colour = isOn ? b.findColour(juce::TextButton::textColourOnId)
                      : b.findColour(juce::TextButton::textColourOffId);

    float fontPx        = isMtSegment    ? juce::jmin(10.0f, b.getHeight() * 0.50f)
                        : isHeaderGlyph  ? juce::jmin(13.0f, b.getHeight() * 0.65f)
                                          : juce::jmin(13.0f, b.getHeight() * 0.50f);
    // Allow per-button glyph-size override (e.g., the preset pill's heart
    // wants to render larger than the other |||, ▲, ▼, 💾 glyphs).
    const auto fontPxOverride = b.getProperties().getWithDefault("phantom-glyph-size", juce::var());
    if (! fontPxOverride.isVoid())
        fontPx = (float) fontPxOverride;
    const float kerning  = isMtSegment ? 0.25f : 0.10f;
    juce::Font font(juce::FontOptions(Theme::uiFontFamily(), fontPx,
                                      isHeaderGlyph ? juce::Font::plain : juce::Font::bold));
    font.setExtraKerningFactor(kerning);

    g.setFont(font);
    // White shadow 1px below glyph (etched effect).
    g.setColour(juce::Colour(0x80FFFFFF));
    g.drawText(b.getButtonText(), b.getLocalBounds().translated(0, 1),
                juce::Justification::centred, false);
    g.setColour(colour);
    g.drawText(b.getButtonText(), b.getLocalBounds(),
                juce::Justification::centred, false);
}

void PhantomLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height,
                                                 juce::TextEditor& te)
{
    g.setColour(te.findColour(juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle(juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f),
                            2.0f, 1.0f);
}

void PhantomLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                                    juce::TextEditor& te)
{
    g.setColour(te.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(juce::Rectangle<float>(0, 0, (float) width, (float) height), 2.0f);
}

void PhantomLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    const auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height);
    g.setColour(Theme::mtxPopoverBg);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(Theme::mtxPopoverBorder);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
}

void PhantomLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                             bool isSeparator, bool /*isActive*/, bool isHighlighted,
                                             bool /*isTicked*/, bool /*hasSubMenu*/,
                                             const juce::String& text,
                                             const juce::String& /*shortcutKeyText*/,
                                             const juce::Drawable* /*icon*/,
                                             const juce::Colour* textColourOverride)
{
    if (isSeparator)
    {
        g.setColour(juce::Colour(0x1FFFFFFF));
        const float y = (float) area.getCentreY();
        g.drawHorizontalLine((int) y, (float) area.getX() + 4.0f, (float) area.getRight() - 4.0f);
        return;
    }

    if (isHighlighted)
    {
        g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRect(area.reduced(2, 1));
    }

    juce::Colour textColour = textColourOverride != nullptr
                                 ? *textColourOverride
                                 : (isHighlighted ? findColour(juce::PopupMenu::highlightedTextColourId)
                                                   : findColour(juce::PopupMenu::textColourId));

    g.setColour(textColour);
    g.setFont(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold));
    g.drawText(text, area.reduced(8, 0), juce::Justification::centredLeft, true);
}

juce::Typeface::Ptr PhantomLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    const auto name = font.getTypefaceName();
    // Only intercept requests for Space Grotesk; everything else (Courier
    // New, monospace, system-ui, etc.) falls through to the base.
    if (name == "Space Grotesk" || name == juce::Font::getDefaultSansSerifFontName())
    {
        const auto style = font.getTypefaceStyle();
        if (style == "Light")    return lightTypeface;
        if (style == "Medium")   return mediumTypeface;
        if (style == "SemiBold") return semiBoldTypeface;
        if (style == "Bold")     return boldTypeface;
        if (font.isBold())       return boldTypeface;
        return regularTypeface;
    }
    return juce::LookAndFeel_V4::getTypefaceForFont(font);
}

} // namespace kaigen::phantom
