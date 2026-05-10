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

        // ── Inset shadow / highlight via clipped edge gradients ──
        // CSS reference (.candy-inner):
        //   inset  3px  3px 14px rgba(0,0,0,0.12)        — top-left dark inset
        //   inset -3px -3px 12px rgba(220,222,226,0.72)  — bottom-right silver
        // We clip to the rounded rect, then fill four edge gradients to fake
        // the inset blur. The strong silver bottom-right highlight is the
        // load-bearing trick for the depressed-tray illusion.
        juce::Path clip;
        clip.addRoundedRectangle(fb, cornerRadius);

        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(clip);

        const float depth = 14.0f;
        const juce::Colour silver = juce::Colour::fromRGB(220, 222, 226);

        // Top edge — dark inset shadow (surface curves down into tray).
        {
            juce::ColourGradient grad(juce::Colour(0x33000000), fb.getX(), fb.getY(),
                                       juce::Colour(0x00000000), fb.getX(), fb.getY() + depth,
                                       false);
            g.setGradientFill(grad);
            g.fillRect(fb.withHeight(depth));
        }
        // Left edge — dark inset shadow.
        {
            juce::ColourGradient grad(juce::Colour(0x33000000), fb.getX(), fb.getY(),
                                       juce::Colour(0x00000000), fb.getX() + depth, fb.getY(),
                                       false);
            g.setGradientFill(grad);
            g.fillRect(fb.withWidth(depth));
        }
        // Bottom edge — strong silver highlight (surface curves back out).
        {
            juce::ColourGradient grad(silver.withAlpha(0.72f), fb.getX(), fb.getBottom(),
                                       silver.withAlpha(0.0f),  fb.getX(), fb.getBottom() - depth,
                                       false);
            g.setGradientFill(grad);
            g.fillRect(fb.withTop(fb.getBottom() - depth));
        }
        // Right edge — silver highlight.
        {
            juce::ColourGradient grad(silver.withAlpha(0.72f), fb.getRight(), fb.getY(),
                                       silver.withAlpha(0.0f),  fb.getRight() - depth, fb.getY(),
                                       false);
            g.setGradientFill(grad);
            g.fillRect(fb.withLeft(fb.getRight() - depth));
        }
    }

    namespace
    {
        // Build a rounded-rect path with a FLAT-BOTTOMED notch dipping inward
        // at the top center. The notch has steep sides (small radius corners)
        // so the title sits in a clearly defined recess instead of a smooth U.
        //
        //  ___________________   ___ y                  _________________
        //                    \\_/                      /
        //                     |   <-- dipDepth (~16) --|
        //                     |________________________|
        //                       <-- notchWidth (~140) -->
        //
        juce::Path buildNotchedRoundedRect(juce::Rectangle<float> r, float corner,
                                            float notchW, float dipDepth)
        {
            juce::Path p;
            const float x = r.getX();
            const float y = r.getY();
            const float w = r.getWidth();
            const float h = r.getHeight();
            const float cx = x + w * 0.5f;
            const float maxNotchHalf = juce::jmax(0.0f, w * 0.5f - corner - 4.0f);
            const float notchHalf    = juce::jmin(notchW * 0.5f, maxNotchHalf);
            // Generous taper — slope ~2× the dip depth wide so the silver
            // bezel reads as a long gentle curve into the tray rather than
            // a sharp drop.
            const float taper        = juce::jmin(dipDepth * 2.4f, notchHalf * 0.6f);

            p.startNewSubPath(x + corner, y);
            p.lineTo(cx - notchHalf, y);
            // Gentle curve DOWN into the notch — control point biased toward
            // the cardTop so the slope eases out near the top edge then
            // accelerates downward.
            p.cubicTo(cx - notchHalf + taper * 0.4f, y,
                      cx - notchHalf + taper * 0.6f, y + dipDepth,
                      cx - notchHalf + taper,        y + dipDepth);
            // Flat bottom of the notch.
            p.lineTo(cx + notchHalf - taper, y + dipDepth);
            // Gentle curve UP out of the notch (mirror of the down-curve).
            p.cubicTo(cx + notchHalf - taper * 0.6f, y + dipDepth,
                      cx + notchHalf - taper * 0.4f, y,
                      cx + notchHalf,                y);
            p.lineTo(x + w - corner, y);
            p.quadraticTo(x + w, y, x + w, y + corner);
            p.lineTo(x + w, y + h - corner);
            p.quadraticTo(x + w, y + h, x + w - corner, y + h);
            p.lineTo(x + corner, y + h);
            p.quadraticTo(x, y + h, x, y + h - corner);
            p.lineTo(x, y + corner);
            p.quadraticTo(x, y, x + corner, y);
            p.closeSubPath();
            return p;
        }
    }

    void paintInsetCardWithNotch(juce::Graphics& g, juce::Rectangle<int> bounds,
                                   float cornerRadius, float notchWidth, float notchDepth)
    {
        const auto fb = bounds.toFloat();
        if (fb.isEmpty()) return;

        const auto path = buildNotchedRoundedRect(fb, cornerRadius, notchWidth, notchDepth);

        // Subtle wash background.
        g.setColour(insetSurfaceTint);
        g.fillPath(path);

        // Edge gradients clipped to the path.
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(path);

        const float depth = 14.0f;
        const juce::Colour silver = juce::Colour::fromRGB(220, 222, 226);

        // Top — dark inset shadow. Extended height through the notch depth so
        // the dip floor reads as the deepest part of the depression.
        {
            juce::ColourGradient grad(juce::Colour(0x4D000000), fb.getX(), fb.getY(),    // 30% black
                                       juce::Colour(0x00000000), fb.getX(), fb.getY() + depth + notchDepth + 2.0f,
                                       false);
            g.setGradientFill(grad);
            g.fillRect(fb.withHeight(depth + notchDepth + 4.0f));
        }
        // Left — dark.
        {
            juce::ColourGradient grad(juce::Colour(0x33000000), fb.getX(), fb.getY(),
                                       juce::Colour(0x00000000), fb.getX() + depth, fb.getY(),
                                       false);
            g.setGradientFill(grad);
            g.fillRect(fb.withWidth(depth));
        }
        // Bottom — silver highlight.
        {
            juce::ColourGradient grad(silver.withAlpha(0.72f), fb.getX(), fb.getBottom(),
                                       silver.withAlpha(0.0f),  fb.getX(), fb.getBottom() - depth,
                                       false);
            g.setGradientFill(grad);
            g.fillRect(fb.withTop(fb.getBottom() - depth));
        }
        // Right — silver highlight.
        {
            juce::ColourGradient grad(silver.withAlpha(0.72f), fb.getRight(), fb.getY(),
                                       silver.withAlpha(0.0f),  fb.getRight() - depth, fb.getY(),
                                       false);
            g.setGradientFill(grad);
            g.fillRect(fb.withLeft(fb.getRight() - depth));
        }
    }

    void paintHeaderStrip(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto fb = bounds.toFloat();
        if (fb.isEmpty()) return;

        // Light silver base — #d6d7d9 (the webview's `--bg`). Subtle white
        // top-highlight, no bottom darkening — gives the strip a faint
        // top-lit feel while keeping the surface tone consistent with the
        // rest of the silver chassis.
        juce::ColourGradient grad(juce::Colour(0xffe4e5e7), fb.getX(), fb.getY(),
                                   juce::Colour(0xffd6d7d9), fb.getX(), fb.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRect(fb);

        // Soft fading separator at the bottom (matches CSS .hdr::after):
        // transparent at the edges, ~7% black across the central 50%, fading
        // to 0 at 5% / 95%. Replaces the previous hard hairline.
        const float yLine = fb.getBottom() - 0.5f;
        const float xL    = fb.getX() + fb.getWidth() * 0.05f;
        const float xR    = fb.getRight() - fb.getWidth() * 0.05f;
        const float xMidL = fb.getX() + fb.getWidth() * 0.25f;
        const float xMidR = fb.getRight() - fb.getWidth() * 0.25f;

        g.setColour(headerSeparator);
        g.fillRect(juce::Rectangle<float>(xMidL, yLine - 0.5f, xMidR - xMidL, 1.0f));

        juce::ColourGradient fadeL(juce::Colour(0x00000000), xL, yLine,
                                    headerSeparator,         xMidL, yLine, false);
        g.setGradientFill(fadeL);
        g.fillRect(juce::Rectangle<float>(xL, yLine - 0.5f, xMidL - xL, 1.0f));

        juce::ColourGradient fadeR(headerSeparator,         xMidR, yLine,
                                    juce::Colour(0x00000000), xR, yLine, false);
        g.setGradientFill(fadeR);
        g.fillRect(juce::Rectangle<float>(xMidR, yLine - 0.5f, xR - xMidR, 1.0f));
    }

    void paintGlassPill(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        if (bounds.isEmpty()) return;
        const float corner = bounds.getHeight() * 0.5f;

        // Body: subtle dark vertical gradient — matches CSS
        // linear-gradient(180deg, rgba(0,0,0,0.06) 0%, rgba(0,0,0,0.02) 100%).
        juce::ColourGradient body(juce::Colour(0x14000000), bounds.getX(), bounds.getY(),
                                   juce::Colour(0x08000000), bounds.getX(), bounds.getBottom(),
                                   false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(bounds, corner);

        // Outer "lifted" white highlight just below the pill (CSS 0 1px 2px
        // rgba(255,255,255,0.6)).
        g.setColour(juce::Colour(0x99FFFFFF));
        g.drawLine(bounds.getX() + corner * 0.6f, bounds.getBottom() + 0.5f,
                    bounds.getRight() - corner * 0.6f, bounds.getBottom() + 0.5f, 0.7f);

        // Inset top shadow (CSS inset 0 2px 4px rgba(0,0,0,0.10)).
        g.setColour(juce::Colour(0x33000000));
        g.drawLine(bounds.getX() + corner * 0.6f, bounds.getY() + 0.6f,
                    bounds.getRight() - corner * 0.6f, bounds.getY() + 0.6f, 0.8f);

        // Inset bottom white highlight (CSS inset 0 -1px 2px rgba(255,255,255,0.3)).
        g.setColour(juce::Colour(0x66FFFFFF));
        g.drawLine(bounds.getX() + corner * 0.6f, bounds.getBottom() - 1.0f,
                    bounds.getRight() - corner * 0.6f, bounds.getBottom() - 1.0f, 0.6f);

        // Subtle outer edge.
        g.setColour(juce::Colour(0x1f000000));
        g.drawRoundedRectangle(bounds.reduced(0.25f), corner, 0.5f);
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
