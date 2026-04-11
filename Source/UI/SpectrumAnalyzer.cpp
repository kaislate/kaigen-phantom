#include "SpectrumAnalyzer.h"
#include "PhantomColours.h"
#include "../PluginProcessor.h"

SpectrumAnalyzer::SpectrumAnalyzer()
{
    bins.fill(0.0f);
    smoothed.fill(0.0f);
}

void SpectrumAnalyzer::pullSpectrum(PhantomProcessor& processor)
{
    if (processor.spectrumReady.exchange(false))
    {
        for (int i = 0; i < 80; ++i)
            bins[i] = processor.spectrumData[i];

        for (int i = 0; i < 80; ++i)
            smoothed[i] = smoothed[i] + 0.3f * (bins[i] - smoothed[i]);

        repaint();
    }
}

void SpectrumAnalyzer::toggleMode()
{
    mode = (mode == Mode::Bar) ? Mode::Line : Mode::Bar;
    repaint();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fill background with oledBlack, 5px rounded corners
    g.setColour(PhantomColours::oledBlack);
    g.fillRoundedRectangle(bounds, 5.0f);

    auto area = bounds.reduced(5.0f, 3.0f);

    paintGrid(g, area);

    if (mode == Mode::Bar)
        paintBars(g, area);
    else
        paintLine(g, area);
}

void SpectrumAnalyzer::paintGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    // 5 horizontal lines at equal spacing, 2% white opacity
    g.setColour(juce::Colour(0x05ffffff));
    const float lineSpacing = area.getHeight() / 5.0f;

    for (int i = 1; i <= 5; ++i)
    {
        const float y = area.getY() + lineSpacing * static_cast<float>(i);
        g.drawHorizontalLine(static_cast<int>(y),
                             area.getX(),
                             area.getRight());
    }

    // Frequency labels at bottom: 8% white, 5px monospace
    g.setColour(juce::Colour(0x14ffffff));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 5.0f, juce::Font::plain));

    static const char* labels[] = { "30", "60", "125", "250", "500",
                                    "1k", "2k", "4k",  "8k",  "16k" };
    constexpr int numLabels = 10;
    const float labelSpacing = area.getWidth() / static_cast<float>(numLabels);

    for (int i = 0; i < numLabels; ++i)
    {
        const float x = area.getX() + labelSpacing * static_cast<float>(i);
        const float labelW = labelSpacing;
        const float labelY = area.getBottom() - 7.0f;

        g.drawText(labels[i],
                   static_cast<int>(x),
                   static_cast<int>(labelY),
                   static_cast<int>(labelW),
                   7,
                   juce::Justification::centred,
                   false);
    }
}

void SpectrumAnalyzer::paintBars(juce::Graphics& g, juce::Rectangle<float> area)
{
    constexpr int numBins = 80;
    const float totalWidth = area.getWidth();
    const float barWidth   = (totalWidth / static_cast<float>(numBins)) - 1.0f;

    for (int i = 0; i < numBins; ++i)
    {
        const float barHeight = smoothed[i] * area.getHeight() * 0.88f;
        if (barHeight <= 0.0f)
            continue;

        const float x = area.getX() + static_cast<float>(i) * (barWidth + 1.0f);
        const float y = area.getBottom() - barHeight;

        const juce::Rectangle<float> barRect(x, y, barWidth, barHeight);

        // Vertical gradient: 50% white at bottom, 4% white at top
        juce::ColourGradient barGradient(
            juce::Colours::white.withAlpha(0.04f), x, y,
            juce::Colours::white.withAlpha(0.50f), x, area.getBottom(),
            false);

        g.setGradientFill(barGradient);
        g.fillRect(barRect);

        // 1px peak cap at the top of the bar in 65% white
        g.setColour(juce::Colours::white.withAlpha(0.65f));
        g.drawHorizontalLine(static_cast<int>(y), x, x + barWidth);
    }
}

void SpectrumAnalyzer::paintLine(juce::Graphics& g, juce::Rectangle<float> area)
{
    constexpr int numBins = 80;
    const float totalWidth = area.getWidth();
    const float binWidth   = totalWidth / static_cast<float>(numBins);

    juce::Path linePath;
    juce::Path fillPath;

    for (int i = 0; i < numBins; ++i)
    {
        const float x = area.getX() + (static_cast<float>(i) + 0.5f) * binWidth;
        const float y = area.getBottom() - smoothed[i] * area.getHeight() * 0.88f;

        if (i == 0)
            linePath.startNewSubPath(x, y);
        else
            linePath.lineTo(x, y);
    }

    // Build fill path: same line + close to bottom corners
    fillPath = linePath;
    const float lastX = area.getX() + (static_cast<float>(numBins) - 0.5f) * binWidth;
    fillPath.lineTo(lastX, area.getBottom());
    fillPath.lineTo(area.getX() + 0.5f * binWidth, area.getBottom());
    fillPath.closeSubPath();

    // Fill underneath: gradient from 20% white at bottom to transparent at top
    juce::ColourGradient fillGradient(
        juce::Colours::transparentBlack,   area.getX(), area.getY(),
        juce::Colours::white.withAlpha(0.20f), area.getX(), area.getBottom(),
        false);

    g.setGradientFill(fillGradient);
    g.fillPath(fillPath);

    // Stroke the line: 1.5px, 60% white
    g.setColour(juce::Colours::white.withAlpha(0.60f));
    g.strokePath(linePath, juce::PathStrokeType(1.5f));
}
