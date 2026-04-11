#include "StereoPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

StereoPanel::StereoPanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(widthKnob);

    widthAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::STEREO_WIDTH, widthKnob);
}

void StereoPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold));
    g.drawText("STEREO", getLocalBounds().removeFromTop(16), juce::Justification::centredTop, false);
}

void StereoPanel::resized()
{
    const int labelH = 16;
    const int diam   = widthKnob.getDiameter(); // 100 (large)

    const juce::Rectangle<int> contentArea = getLocalBounds().withTrimmedTop(labelH);
    const int cx = contentArea.getCentreX();
    const int cy = contentArea.getCentreY();

    widthKnob.setBounds(cx - diam / 2, cy - diam / 2, diam, diam);
}
