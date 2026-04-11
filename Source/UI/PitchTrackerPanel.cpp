#include "PitchTrackerPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

PitchTrackerPanel::PitchTrackerPanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(sensitivityKnob);
    addAndMakeVisible(glideKnob);

    sensitivityAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::TRACKING_SENSITIVITY, sensitivityKnob);
    glideAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::TRACKING_GLIDE, glideKnob);
}

void PitchTrackerPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold));
    g.drawText("PITCH TRACKER", getLocalBounds().removeFromTop(16), juce::Justification::topLeft, false);
}

void PitchTrackerPanel::resized()
{
    const int labelH = 16;
    const int gap    = 10;
    const int diam   = sensitivityKnob.getDiameter(); // 72 (medium)
    const int totalW = diam + gap + diam;

    const juce::Rectangle<int> contentArea = getLocalBounds().withTrimmedTop(labelH);
    const int cx = contentArea.getCentreX();
    const int cy = contentArea.getCentreY();

    sensitivityKnob.setBounds(cx - totalW / 2,             cy - diam / 2, diam, diam);
    glideKnob      .setBounds(cx - totalW / 2 + diam + gap, cy - diam / 2, diam, diam);
}
