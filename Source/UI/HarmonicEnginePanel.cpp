#include "HarmonicEnginePanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

HarmonicEnginePanel::HarmonicEnginePanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(strengthKnob);

    saturationAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::HARMONIC_SATURATION, saturationKnob);
    strengthAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::PHANTOM_STRENGTH, strengthKnob);
}

void HarmonicEnginePanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold));
    g.drawText("HARMONIC ENGINE", getLocalBounds().removeFromTop(16), juce::Justification::topLeft, false);
}

void HarmonicEnginePanel::resized()
{
    const int labelH    = 16;
    const int gap       = 10;
    const int largeDiam = saturationKnob.getDiameter();   // 100
    const int medDiam   = strengthKnob.getDiameter();     // 72
    const int totalW    = largeDiam + gap + medDiam;

    const juce::Rectangle<int> contentArea = getLocalBounds().withTrimmedTop(labelH);
    const int cx = contentArea.getCentreX();
    const int cy = contentArea.getCentreY();

    // Large knob left, medium knob right, both vertically centred in the content area
    saturationKnob.setBounds(cx - gap / 2 - largeDiam, cy - largeDiam / 2, largeDiam, largeDiam);
    strengthKnob  .setBounds(cx + gap / 2,             cy - medDiam   / 2, medDiam,   medDiam);

    juce::ignoreUnused(totalW);
}
