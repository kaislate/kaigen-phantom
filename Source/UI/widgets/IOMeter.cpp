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

    constexpr float kCorner    = 2.0f;
    constexpr int   kBarGap    = 2;     // gap between L and R bars
}

IOMeter::IOMeter(const std::atomic<float>& peakL,
                  const std::atomic<float>& peakR)
    : chanL{ peakL, 0.0f, 0.0f },
      chanR{ peakR, 0.0f, 0.0f }
{
    setSize(20, 90);   // wide enough for 2 bars side-by-side (~8 each + gap)
    startTimerHz(20);
}

IOMeter::~IOMeter()
{
    stopTimer();
}

void IOMeter::timerCallback()
{
    if (! isShowing()) return;

    auto update = [](ChannelState& c) {
        const float raw  = c.peakSource.load(std::memory_order_relaxed);
        const float coef = (raw > c.currentLevel) ? kAttackCoef : kReleaseCoef;
        c.currentLevel  += (raw - c.currentLevel) * coef;
        if (c.currentLevel > c.peakHold) c.peakHold = c.currentLevel;
        else                              c.peakHold = juce::jmax(0.0f, c.peakHold - kPeakHoldDecayPerTick);
    };
    update(chanL);
    update(chanR);
    repaint();
}

void IOMeter::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float barW = (w - (float) kBarGap) * 0.5f;

    auto drawBar = [&](float x, const ChannelState& c) {
        const juce::Rectangle<float> barBounds(x, 0.0f, barW, h);

        // Thin-bezel well.
        g.setColour(juce::Colour(0x10000000));
        g.fillRoundedRectangle(barBounds, kCorner);
        g.setColour(juce::Colour(0x18000000));
        g.drawLine(x, 0.0f, x + barW, 0.0f, 1.0f);

        // Lit fill.
        const float fillNorm = toDbNormalized(c.currentLevel);
        if (fillNorm > 0.0f)
        {
            const float fillH = fillNorm * h;
            const juce::Rectangle<float> fillBounds(x, h - fillH, barW, fillH);
            juce::ColourGradient grad(juce::Colour(0xfff5f8fb),
                                       0.0f, h - fillH,
                                       juce::Colour(0xffd8e0e8).withAlpha(0.92f),
                                       0.0f, h,
                                       false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillBounds, kCorner);
        }

        // Peak-hold marker.
        const float holdNorm = toDbNormalized(c.peakHold);
        if (holdNorm > 0.0f)
        {
            const float py = h - holdNorm * h;
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            g.fillRect(juce::Rectangle<float>(x, py - 0.5f, barW, 1.0f));
        }

        // Clip indicator.
        if (c.peakSource.load(std::memory_order_relaxed) >= 0.989f)
        {
            g.setColour(Theme::clipRed);
            g.fillRect(juce::Rectangle<float>(x, 0.0f, barW, 2.0f));
        }
    };

    drawBar(0.0f,                       chanL);
    drawBar(barW + (float) kBarGap,     chanR);
}

} // namespace kaigen::phantom
