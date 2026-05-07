// Source/UI/visualizers/Spectrum.cpp
#include "Spectrum.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"

namespace kaigen::phantom
{

Spectrum::Spectrum(PhantomProcessor& p)
    : processor(p)
{
    setSize(800, 280);
    startTimerHz(30);
}

Spectrum::~Spectrum()
{
    stopTimer();
}

void Spectrum::timerCallback()
{
    // Read input + output spectrum sources directly: these std::array<float,80>
    // members are public on PhantomProcessor and live alongside the rest of the
    // UI-facing real-time data. The audio thread fills them; tearing during
    // single-float reads is invisible at 30fps visualization rate (same pattern
    // as the existing WebView2 spectrum binding).
    const auto& inSrc  = processor.spectrumData;
    const auto& outSrc = processor.spectrumOutputData;

    // Asymmetric smoothing: fast attack, slow release.
    for (int i = 0; i < kBins; ++i)
    {
        const float coefIn  = (inSrc [(size_t) i] > smoothedInput [(size_t) i]) ? kSmoothUp : kSmoothDown;
        smoothedInput [(size_t) i] += (inSrc [(size_t) i] - smoothedInput [(size_t) i]) * coefIn;
        const float coefOut = (outSrc[(size_t) i] > smoothedOutput[(size_t) i]) ? kSmoothUp : kSmoothDown;
        smoothedOutput[(size_t) i] += (outSrc[(size_t) i] - smoothedOutput[(size_t) i]) * coefOut;
    }
    repaint();
}

void Spectrum::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background.
    g.setColour(Theme::matrixBg);
    g.fillRect(bounds);

    // Bars: input as a filled silhouette in dim white, output overlaid in steel blue.
    const float binWidth = w / (float) kBins;

    auto drawBars = [&](const std::array<float, kBins>& values, juce::Colour fillColour)
    {
        g.setColour(fillColour);
        for (int i = 0; i < kBins; ++i)
        {
            const float v = juce::jlimit(0.0f, 1.0f, values[(size_t) i]);
            if (v <= 0.0f) continue;
            const float barH = v * h;
            const float x = (float) i * binWidth;
            g.fillRect(juce::Rectangle<float>(x + 0.5f, h - barH, binWidth - 1.0f, barH));
        }
    };

    drawBars(smoothedInput,  Theme::textDim);
    drawBars(smoothedOutput, Theme::steelBlue.withAlpha(0.85f));

    // Border.
    g.setColour(Theme::panelBorder);
    g.drawRect(bounds, 1.0f);
}

} // namespace kaigen::phantom
