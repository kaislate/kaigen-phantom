// Source/UI/visualizers/Oscilloscope.cpp
#include "Oscilloscope.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../Engines/PhantomEngine.h"

namespace kaigen::phantom
{

Oscilloscope::Oscilloscope(PhantomProcessor& p)
    : processor(p)
{
    setSize(800, 120);
    startTimerHz(30);
}

Oscilloscope::~Oscilloscope()
{
    stopTimer();
}

void Oscilloscope::timerCallback()
{
    // Snapshot the most recent kDisplaySamples from the engine's synth ring.
    // Mirrors the existing WebView2 getOscilloscopeData binding in
    // PluginEditor.cpp: same processor.getActiveEngine().oscSynthBuf path,
    // same relaxed atomic loads. The audio thread writes continuously;
    // tearing at 30 fps is invisible — visualisation tolerates eventual
    // consistency and no observable state depends on these samples.
    auto& ring = processor.getActiveEngine().oscSynthBuf;
    const int total = (int) ring.size();
    const int start = total - kDisplaySamples;  // last 1024 samples
    for (int i = 0; i < kDisplaySamples; ++i)
        snapshot[(size_t) i] = ring[(size_t) (start + i)].load(std::memory_order_relaxed);
    repaint();
}

void Oscilloscope::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float midY = h * 0.5f;

    // Background.
    g.setColour(Theme::matrixBg);
    g.fillRect(bounds);

    // Center line.
    g.setColour(Theme::panelBorder);
    g.drawHorizontalLine((int) midY, 0.0f, w);

    // Waveform path.
    juce::Path path;
    if constexpr (kDisplaySamples > 0)
    {
        path.startNewSubPath(0.0f, midY - snapshot[0] * (h * 0.45f));
        for (int i = 1; i < kDisplaySamples; ++i)
        {
            const float x = (float) i / (float) (kDisplaySamples - 1) * w;
            const float y = midY - snapshot[(size_t) i] * (h * 0.45f);
            path.lineTo(x, y);
        }
    }
    g.setColour(Theme::steelBlue);
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

} // namespace kaigen::phantom
