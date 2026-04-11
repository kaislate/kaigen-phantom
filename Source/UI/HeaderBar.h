#pragma once
#include <JuceHeader.h>

class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar(juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void drawStippleTexture(juce::Graphics& g);
    void drawBottomGlowSeam(juce::Graphics& g);
    void drawEtchedText(juce::Graphics& g,
                        const juce::String& text,
                        juce::Rectangle<float> area,
                        const juce::Font& font,
                        juce::Justification justification);

    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton effectBtn  { "EFFECT" };
    juce::TextButton instrumentBtn { "INSTRUMENT" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderBar)
};
