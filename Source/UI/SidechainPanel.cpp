#include "SidechainPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

SidechainPanel::SidechainPanel(juce::AudioProcessorValueTreeState& apvts)
{
    addAndMakeVisible(amountKnob);
    addAndMakeVisible(attackKnob);
    addAndMakeVisible(releaseKnob);

    amountAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::SIDECHAIN_DUCK_AMOUNT, amountKnob);
    attackAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::SIDECHAIN_DUCK_ATTACK, attackKnob);
    releaseAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::SIDECHAIN_DUCK_RELEASE, releaseKnob);
}

void SidechainPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold));
    g.drawText("SIDECHAIN DUCK", getLocalBounds().removeFromTop(16), juce::Justification::topLeft, false);
}

void SidechainPanel::resized()
{
    const int labelH = 16;
    const int gap    = 8;
    const int diam   = amountKnob.getDiameter(); // 72 (medium)
    const int totalW = diam * 3 + gap * 2;

    const juce::Rectangle<int> contentArea = getLocalBounds().withTrimmedTop(labelH);
    const int cx = contentArea.getCentreX();
    const int cy = contentArea.getCentreY();

    const int startX = cx - totalW / 2;
    amountKnob .setBounds(startX,                    cy - diam / 2, diam, diam);
    attackKnob .setBounds(startX + diam + gap,        cy - diam / 2, diam, diam);
    releaseKnob.setBounds(startX + (diam + gap) * 2, cy - diam / 2, diam, diam);
}
