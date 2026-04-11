#pragma once
#include <JuceHeader.h>

class FooterBar : public juce::Component
{
public:
    FooterBar();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void drawStippleTexture(juce::Graphics& g);
    void drawTopGlowSeam(juce::Graphics& g);

    juce::TextButton bypassBtn     { "BYPASS" };
    juce::TextButton recipeBtn     { "RECIPE" };
    juce::TextButton ghostBtn      { "GHOST" };
    juce::TextButton binauralBtn   { "BINAURAL" };
    juce::TextButton deconflictBtn { "DECONFLICT" };
    juce::TextButton settingsBtn   { "SETTINGS" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FooterBar)
};
