// Source/UI/PhantomLookAndFeel.cpp
#include "PhantomLookAndFeel.h"
#include "Theme.h"
#include <juce_graphics/juce_graphics.h>

namespace kaigen::phantom
{

PhantomLookAndFeel::PhantomLookAndFeel()
{
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

void PhantomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                            float sliderPos,
                                            float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat();
    if (bounds.isEmpty()) return;

    const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto centre    = bounds.getCentre();
    const float radius   = diameter * 0.5f;

    // ── Outer offset shadow (bottom-right): "4px 4px 14px rgba(0,0,0,0.22)" ──
    {
        const auto shadowBounds = juce::Rectangle<float>(diameter, diameter)
                                       .withCentre({ centre.x + 2.0f, centre.y + 2.0f });
        juce::DropShadow shadow { juce::Colour(0x38000000), 8, juce::Point<int>(2, 2) };
        juce::Path circlePath;
        circlePath.addEllipse(shadowBounds);
        shadow.drawForPath(g, circlePath);
    }
    // ── Outer offset highlight (top-left): "-4px -4px 10px rgba(255,255,255,0.50)" ──
    {
        const auto hlBounds = juce::Rectangle<float>(diameter, diameter)
                                   .withCentre({ centre.x - 2.0f, centre.y - 2.0f });
        juce::DropShadow hl { juce::Colour(0x80FFFFFF), 6, juce::Point<int>(-2, -2) };
        juce::Path circlePath;
        circlePath.addEllipse(hlBounds);
        hl.drawForPath(g, circlePath);
    }

    // ── Convex disc body — radial-gradient(ellipse at 35% 30%, ...) ──
    const juce::Point<float> gradOrigin {
        centre.x - radius * 0.30f,   // 35% from left of bounds == ~-15% from centre
        centre.y - radius * 0.40f    // 30% from top  of bounds == ~-20% from centre
    };
    juce::ColourGradient body(juce::Colour(0x3DFFFFFF), gradOrigin,                   // rgba(255,255,255,0.24)
                                juce::Colour(0x12000000),                              // rgba(0,0,0,0.07)
                                { centre.x + radius, centre.y + radius },
                                true);
    body.addColour(0.20, juce::Colour(0x1EFFFFFF));   // rgba(255,255,255,0.12)
    body.addColour(0.55, juce::Colour(0x05000000));   // rgba(0,0,0,0.02)
    g.setGradientFill(body);
    g.fillEllipse(juce::Rectangle<float>(diameter, diameter).withCentre(centre));

    // ── Inset rim ring (CSS: 0 0 0 4px rgba(0,0,0,0.07)) ──
    g.setColour(juce::Colour(0x12000000));
    g.drawEllipse(juce::Rectangle<float>(diameter, diameter).withCentre(centre).reduced(0.5f), 1.0f);

    // ── Indicator arc (the colored sweep showing current value) ──
    const float arcRadius      = radius - 4.0f;
    const float arcThickness   = juce::jmax(2.0f, diameter * 0.06f);
    const float toAngle        = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Background arc (faint full-range track).
    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0x14000000));
    g.strokePath(bgArc, juce::PathStrokeType(arcThickness * 0.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // Foreground active arc — type-color or accent fallback.
    juce::Colour indicatorColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
    if (indicatorColour == juce::Colour())
        indicatorColour = Theme::accentBlue;

    juce::Path fgArc;
    fgArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, toAngle, true);
    g.setColour(indicatorColour);
    g.strokePath(fgArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // ── White tick mark — short line from indicator radius inward ──
    const float tickLen = radius * 0.40f;
    const float tickX1  = centre.x + std::cos(toAngle - juce::MathConstants<float>::halfPi) * (arcRadius - arcThickness);
    const float tickY1  = centre.y + std::sin(toAngle - juce::MathConstants<float>::halfPi) * (arcRadius - arcThickness);
    const float tickX2  = centre.x + std::cos(toAngle - juce::MathConstants<float>::halfPi) * (arcRadius - arcThickness - tickLen);
    const float tickY2  = centre.y + std::sin(toAngle - juce::MathConstants<float>::halfPi) * (arcRadius - arcThickness - tickLen);
    g.setColour(juce::Colour(0xE6FFFFFF));   // ~90% white
    g.drawLine(tickX1, tickY1, tickX2, tickY2, juce::jmax(1.5f, arcThickness * 0.40f));
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
    const bool isHeaderRaised = b.getProperties().getWithDefault("phantom-style", "")
                                    == juce::var("header-raised");

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
                                          bool /*shouldDrawButtonAsHighlighted*/,
                                          bool shouldDrawButtonAsDown)
{
    const bool isOn = b.getToggleState() || shouldDrawButtonAsDown;
    const bool isHeaderRaised = b.getProperties().getWithDefault("phantom-style", "")
                                    == juce::var("header-raised");

    juce::Colour colour;
    if (isHeaderRaised)
        colour = Theme::textOnLightActive;   // ~62% black, readable on the silver pill
    else
        colour = isOn ? b.findColour(juce::TextButton::textColourOnId)
                      : b.findColour(juce::TextButton::textColourOffId);

    juce::Font font(juce::FontOptions("Space Grotesk", juce::jmin(13.0f, b.getHeight() * 0.50f),
                                        juce::Font::bold));
    font.setExtraKerningFactor(0.10f);   // approximate 1-2 px letter-spacing

    // Etched: shadow below glyph + foreground.
    g.setFont(font);
    g.setColour(juce::Colour(0x80FFFFFF));   // 50% white shadow
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

} // namespace kaigen::phantom
