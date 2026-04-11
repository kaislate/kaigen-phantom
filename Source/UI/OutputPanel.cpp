#include "OutputPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

OutputPanel::OutputPanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(gainKnob);

    gainAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::OUTPUT_GAIN, gainKnob);
}

void OutputPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold));
    g.drawText("OUTPUT", getLocalBounds(), juce::Justification::centredTop, false);
}

void OutputPanel::resized()
{
    const int diam = gainKnob.getDiameter(); // 100
    const int cx   = getWidth()  / 2;
    const int cy   = getHeight() / 2;

    gainKnob.setBounds(cx - diam / 2, cy - diam / 2, diam, diam);
}
