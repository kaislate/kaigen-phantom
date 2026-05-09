// Source/UI/Theme.cpp
#include "Theme.h"

namespace kaigen::phantom::Theme
{
    void paintSilverPanel(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto fb = bounds.toFloat();
        if (fb.isEmpty()) return;

        // Radial gradient body — ellipse at 28% horizontal, 18% vertical.
        // CSS: radial-gradient(ellipse at 28% 18%, #CACCCE 0%, #BBBDBF 48%, #AEAFB1 100%)
        const juce::Point<float> centre {
            fb.getX() + fb.getWidth()  * 0.28f,
            fb.getY() + fb.getHeight() * 0.18f
        };
        const float radius = juce::jmax(fb.getWidth(), fb.getHeight()) * 1.1f;

        juce::ColourGradient grad(panelRadialA, centre,
                                   panelRadialC, { centre.x + radius, centre.y + radius },
                                   true);
        grad.addColour(0.48, panelRadialB);
        g.setGradientFill(grad);
        g.fillRect(fb);

        // Top gloss highlight (CSS: inset 0 6px 16px rgba(255,255,255,0.62)).
        // Approximate: 16-pixel-tall gradient from white-62% to transparent at the top.
        juce::ColourGradient topGloss(juce::Colour(0x9eFFFFFF), fb.getX(), fb.getY(),
                                       juce::Colour(0x00FFFFFF), fb.getX(), fb.getY() + 16.0f, false);
        g.setGradientFill(topGloss);
        g.fillRect(fb.withHeight(16.0f));

        // Left gloss highlight (CSS: inset 6px 0 12px rgba(255,255,255,0.32)).
        juce::ColourGradient leftGloss(juce::Colour(0x52FFFFFF), fb.getX(), fb.getY(),
                                        juce::Colour(0x00FFFFFF), fb.getX() + 12.0f, fb.getY(), false);
        g.setGradientFill(leftGloss);
        g.fillRect(fb.withWidth(12.0f));

        // Bottom shade (CSS: inset 0 -5px 14px rgba(0,0,0,0.10)).
        juce::ColourGradient bottomShade(juce::Colour(0x00000000), fb.getX(), fb.getBottom() - 14.0f,
                                          juce::Colour(0x1a000000), fb.getX(), fb.getBottom(), false);
        g.setGradientFill(bottomShade);
        g.fillRect(fb.withTop(fb.getBottom() - 14.0f));

        // Right shade (CSS: inset -5px 0 12px rgba(0,0,0,0.07)).
        juce::ColourGradient rightShade(juce::Colour(0x00000000), fb.getRight() - 12.0f, fb.getY(),
                                         juce::Colour(0x12000000), fb.getRight(), fb.getY(), false);
        g.setGradientFill(rightShade);
        g.fillRect(fb.withLeft(fb.getRight() - 12.0f));
    }

    void paintInsetCard(juce::Graphics& g, juce::Rectangle<int> bounds, float cornerRadius)
    {
        const auto fb = bounds.toFloat();
        if (fb.isEmpty()) return;

        // 2% black wash background.
        g.setColour(insetSurfaceTint);
        g.fillRoundedRectangle(fb, cornerRadius);

        // Top-left inset shadow (CSS: inset 3px 3px 14px rgba(0,0,0,0.12)).
        // Approximate with 3 stacked thin rectangles fading inward.
        for (int i = 0; i < 3; ++i)
        {
            const float a = 0.12f * (1.0f - (float) i / 3.0f);
            g.setColour(juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, a));
            const auto layer = fb.reduced((float) i + 1.0f);
            g.drawRoundedRectangle(layer, cornerRadius - i, 1.0f);
        }

        // Bottom-right inset highlight (CSS: inset -3px -3px 12px rgba(220,222,226,0.72)).
        // Approximate with a soft path on the bottom-right edge.
        for (int i = 0; i < 3; ++i)
        {
            const float a = 0.72f * (1.0f - (float) i / 3.0f);
            g.setColour(juce::Colour::fromFloatRGBA(220.0f / 255.0f, 222.0f / 255.0f, 226.0f / 255.0f, a * 0.4f));
            const auto layer = fb.reduced((float) i + 1.0f);
            // Draw bottom + right edges only — emulate the directional inset highlight.
            g.drawRoundedRectangle(layer, cornerRadius - i, 0.7f);
        }
    }

    void paintHeaderStrip(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto fb = bounds.toFloat();
        if (fb.isEmpty()) return;

        juce::ColourGradient grad(headerHi, fb.getX(), fb.getY(),
                                   headerLo, fb.getX(), fb.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRect(fb);

        // Bottom hairline separator.
        g.setColour(headerSeparator);
        g.drawHorizontalLine(bounds.getBottom() - 1, fb.getX(), fb.getRight());
    }

    void paintVisualizerInset(juce::Graphics& g, juce::Rectangle<int> bounds, float cornerRadius)
    {
        const auto fb = bounds.toFloat();
        if (fb.isEmpty()) return;

        // Pitch-black body.
        g.setColour(vizSurface);
        g.fillRoundedRectangle(fb, cornerRadius);

        // Hard inner shadow at top-left (CSS: inset 3px 3px 12px rgba(0,0,0,1)).
        // Approximate with 4 stacked thin lines fading inward, top + left edges only.
        for (int i = 0; i < 4; ++i)
        {
            const float a = 1.0f - (float) i / 4.0f;
            g.setColour(juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, a * 0.35f));
            const auto inner = fb.reduced((float) i + 0.5f);
            g.drawRoundedRectangle(inner, cornerRadius - i * 0.5f, 0.8f);
        }
    }

    void drawEtchedText(juce::Graphics& g, juce::String text, juce::Rectangle<int> bounds,
                        juce::Justification just, juce::Font font, juce::Colour textColour)
    {
        g.setFont(font);
        // Etched effect: 1px white shadow below glyph.
        g.setColour(textShadowEtch);
        g.drawText(text, bounds.translated(0, 1), just, false);
        // Foreground.
        g.setColour(textColour);
        g.drawText(text, bounds, just, false);
    }
}
