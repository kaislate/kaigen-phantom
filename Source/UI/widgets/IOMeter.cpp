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

    // Background.
    g.setColour(Theme::matrixBg);
    g.fillRect(bounds);

    // Fill -- clipped to dB-normalized current level.
    const float fillNorm = toDbNormalized(currentLevel);
    if (fillNorm > 0.0f)
    {
        const float fillH = fillNorm * h;
        auto grad = juce::ColourGradient(juce::Colour(0xffeaf3ff),
                                          0.0f, h - fillH,
                                          juce::Colour(0xffd4e5f5).withAlpha(0.95f),
                                          0.0f, h,
                                          false);
        g.setGradientFill(grad);
        g.fillRect(juce::Rectangle<float>(1.0f, h - fillH, w - 2.0f, fillH));
    }

    // Peak-hold marker -- single thin line at the held peak's normalized y.
    const float holdNorm = toDbNormalized(peakHold);
    if (holdNorm > 0.0f)
    {
        const float py = h - holdNorm * h;
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRect(juce::Rectangle<float>(1.0f, py - 0.5f, w - 2.0f, 1.0f));
    }

    // Clip indicator -- top 2px red when raw signal at or above -0.1 dBFS.
    if (peakSource.load(std::memory_order_relaxed) >= 0.989f)
    {
        g.setColour(Theme::clipRed);
        g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, w, 2.0f));
    }

    // Subtle border.
    g.setColour(Theme::panelBorder);
    g.drawRect(bounds, 1.0f);
}

} // namespace kaigen::phantom
