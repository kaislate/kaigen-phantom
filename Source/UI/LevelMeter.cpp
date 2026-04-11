#include "LevelMeter.h"
#include "PhantomColours.h"

LevelMeter::LevelMeter(const juce::String& labelText)
    : label(labelText)
{
}

void LevelMeter::setLevel(float leftPeak, float rightPeak)
{
    const float newLevel = juce::jmax(leftPeak, rightPeak);

    if (std::abs(newLevel - level) > 0.001f)
    {
        level    = newLevel;
        smoothed = smoothed + 0.3f * (level - smoothed);
        repaint();
    }
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fill entire bounds with oledBlack, 3px rounded corners
    g.setColour(PhantomColours::oledBlack);
    g.fillRoundedRectangle(bounds, 3.0f);

    auto inner = bounds.reduced(1.0f);

    // Fill height = smoothed * inner.height * 0.85
    const float fillHeight = smoothed * inner.getHeight() * 0.85f;
    if (fillHeight > 0.0f)
    {
        const float fillY = inner.getBottom() - fillHeight;
        const juce::Rectangle<float> fillRect(inner.getX(), fillY, inner.getWidth(), fillHeight);

        // Vertical gradient: 35% white at bottom, 5% white at top
        juce::ColourGradient meterGradient(
            juce::Colours::white.withAlpha(0.05f), inner.getX(), fillY,
            juce::Colours::white.withAlpha(0.35f), inner.getX(), inner.getBottom(),
            false);

        g.setGradientFill(meterGradient);
        g.fillRect(fillRect);

        // 1px peak line at top of fill in 50% white
        g.setColour(juce::Colours::white.withAlpha(0.50f));
        g.drawHorizontalLine(static_cast<int>(fillY),
                             inner.getX(),
                             inner.getRight());
    }

    // Label ("IN" or "OUT") at bottom, 5px monospace, textDim at 0.2 alpha
    g.setColour(PhantomColours::textDim.withAlpha(0.2f));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 5.0f, juce::Font::plain));
    g.drawText(label,
               inner.toNearestInt(),
               juce::Justification::centredBottom,
               false);
}
