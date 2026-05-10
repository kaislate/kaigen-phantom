// Source/UI/widgets/IOMeter.cpp
#include "IOMeter.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    /** Convert linear amplitude to a normalized 0..1 display value over
     *  a -60..0 dB range. Returns 0 for any input <= 0. */
    float toDbNormalized(float lin)
    {
        if (lin <= 0.0f) return 0.0f;
        const float dB = 20.0f * std::log10(lin);
        return juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 60.0f);
    }
}

IOMeter::IOMeter(const std::atomic<float>& src)
    : peakSource(src)
{
    setSize(14, 90);
    startTimerHz(30);
}

IOMeter::~IOMeter()
{
    stopTimer();
}

void IOMeter::timerCallback()
{
    const float raw = peakSource.load(std::memory_order_relaxed);

    // Attack-fast / release-slow on the smoothed level.
    const float coef = (raw > currentLevel) ? kAttackCoef : kReleaseCoef;
    currentLevel += (raw - currentLevel) * coef;

    // Peak-hold: latch when smoothed exceeds it; decay otherwise.
    if (currentLevel > peakHold) peakHold = currentLevel;
    else                          peakHold = juce::jmax(0.0f, peakHold - kPeakHoldDecayPerTick);

    repaint();
}

void IOMeter::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // ── Inset depressed tray exactly the size of the meter ──────────────
    // 2% black wash + clipped edge gradients (top-left dark / bottom-right
    // silver) gives a tiny depressed slot the meter "sits inside".
    {
        constexpr float corner = 3.0f;
        juce::Path clip;
        clip.addRoundedRectangle(bounds, corner);

        g.setColour(juce::Colour(0x14000000));      // 8% black wash — slightly darker than .candy-inner
        g.fillRoundedRectangle(bounds, corner);

        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(clip);

        // Top inset shadow.
        {
            juce::ColourGradient grad(juce::Colour(0x40000000), 0.0f, 0.0f,
                                       juce::Colour(0x00000000), 0.0f, 4.0f, false);
            g.setGradientFill(grad);
            g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, w, 4.0f));
        }
        // Left inset shadow.
        {
            juce::ColourGradient grad(juce::Colour(0x40000000), 0.0f, 0.0f,
                                       juce::Colour(0x00000000), 4.0f, 0.0f, false);
            g.setGradientFill(grad);
            g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, 4.0f, h));
        }
        // Bottom silver highlight.
        {
            const juce::Colour silver = juce::Colour::fromRGB(220, 222, 226);
            juce::ColourGradient grad(silver.withAlpha(0.65f), 0.0f, h,
                                       silver.withAlpha(0.0f),  0.0f, h - 4.0f, false);
            g.setGradientFill(grad);
            g.fillRect(juce::Rectangle<float>(0.0f, h - 4.0f, w, 4.0f));
        }
        // Right silver highlight.
        {
            const juce::Colour silver = juce::Colour::fromRGB(220, 222, 226);
            juce::ColourGradient grad(silver.withAlpha(0.65f), w, 0.0f,
                                       silver.withAlpha(0.0f),  w - 4.0f, 0.0f, false);
            g.setGradientFill(grad);
            g.fillRect(juce::Rectangle<float>(w - 4.0f, 0.0f, 4.0f, h));
        }
    }

    // ── Lit fill — steel-white (matches active button colour `#f5f8fb`) ──
    const float fillNorm = toDbNormalized(currentLevel);
    if (fillNorm > 0.0f)
    {
        const float fillH = fillNorm * h;
        const auto fillBounds = juce::Rectangle<float>(2.0f, h - fillH, w - 4.0f, fillH);

        // Steel-white gradient with a subtle glow.
        auto grad = juce::ColourGradient(juce::Colour(0xfff5f8fb),
                                          0.0f, h - fillH,
                                          juce::Colour(0xffe0e6ed).withAlpha(0.95f),
                                          0.0f, h,
                                          false);
        g.setGradientFill(grad);
        g.fillRect(fillBounds);

        // Inner glow (very subtle accent-blue tint at the active edge).
        g.setColour(juce::Colour(0x664A8DD5));   // 40% accent blue
        g.drawHorizontalLine((int) (h - fillH), 2.0f, w - 2.0f);
    }

    // Peak-hold marker.
    const float holdNorm = toDbNormalized(peakHold);
    if (holdNorm > 0.0f)
    {
        const float py = h - holdNorm * h;
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRect(juce::Rectangle<float>(2.0f, py - 0.5f, w - 4.0f, 1.0f));
    }

    // Clip indicator.
    if (peakSource.load(std::memory_order_relaxed) >= 0.989f)
    {
        g.setColour(Theme::clipRed);
        g.fillRect(juce::Rectangle<float>(1.0f, 1.0f, w - 2.0f, 2.0f));
    }
}

} // namespace kaigen::phantom
