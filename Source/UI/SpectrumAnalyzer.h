#pragma once
#include <JuceHeader.h>
#include <array>

class PhantomProcessor; // forward declare

class SpectrumAnalyzer : public juce::Component
{
public:
    enum class Mode { Bar, Line };

    SpectrumAnalyzer();

    void paint(juce::Graphics& g) override;
    void pullSpectrum(PhantomProcessor& processor);
    void toggleMode();

private:
    Mode mode = Mode::Bar;
    std::array<float, 80> bins {};
    std::array<float, 80> smoothed {};

    void paintBars(juce::Graphics& g, juce::Rectangle<float> area);
    void paintLine(juce::Graphics& g, juce::Rectangle<float> area);
    void paintGrid(juce::Graphics& g, juce::Rectangle<float> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
