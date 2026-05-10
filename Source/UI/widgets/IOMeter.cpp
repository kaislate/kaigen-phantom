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
    startTimerHz(20);   // dropped from 30 — meter responsiveness still feels fine
}

IOMeter::~IOMeter()
{
    stopTimer();
}

void IOMeter::timerCallback()
{
    if (! isShowing()) return;

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

    // ── Subtle thin-bezel well ─────────────────────────────────────────
    // No deep depression — just a faint shaded slot that suggests the
    // bezel is thinner here so the LED can shine through from underneath.
    constexpr float corner = 2.0f;
    g.setColour(juce::Colour(0x10000000));      // ~6% black wash — barely darker than panel
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(juce::Colour(0x18000000));      // hairline top edge
    g.drawHorizontalLine(0, 0.0f, w);

    // ── Lit fill — fills the entire well width (no inset gap) ──────────
    const float fillNorm = toDbNormalized(currentLevel);
    if (fillNorm > 0.0f)
    {
        const float fillH = fillNorm * h;
        const auto fillBounds = juce::Rectangle<float>(0.0f, h - fillH, w, fillH);

        // Steel-white gradient — like a backlit LED through translucent plastic.
        auto grad = juce::ColourGradient(juce::Colour(0xfff5f8fb),
                                          0.0f, h - fillH,
                                          juce::Colour(0xffd8e0e8).withAlpha(0.92f),
                                          0.0f, h,
                                          false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fillBounds, corner);
    }

    // Peak-hold marker.
    const float holdNorm = toDbNormalized(peakHold);
    if (holdNorm > 0.0f)
    {
        const float py = h - holdNorm * h;
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRect(juce::Rectangle<float>(0.0f, py - 0.5f, w, 1.0f));
    }

    // Clip indicator.
    if (peakSource.load(std::memory_order_relaxed) >= 0.989f)
    {
        g.setColour(Theme::clipRed);
        g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, w, 2.0f));
    }
}

} // namespace kaigen::phantom
