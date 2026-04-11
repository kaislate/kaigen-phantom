#include "DeconflictionPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

DeconflictionPanel::DeconflictionPanel(juce::AudioProcessorValueTreeState& apvts)
{
    modeSelector.addItemList(
        { "Partition", "Lane", "Stagger", "Odd-Even", "Residue", "Binaural" }, 1);
    addAndMakeVisible(modeSelector);

    addAndMakeVisible(voicesKnob);
    addAndMakeVisible(staggerKnob);

    modeAttachment = std::make_unique<ComboBoxAttachment>(
        apvts, ParamID::DECONFLICTION_MODE, modeSelector);
    voicesAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::MAX_VOICES, voicesKnob);
    staggerAttachment = std::make_unique<SliderAttachment>(
        apvts, ParamID::STAGGER_DELAY, staggerKnob);
}

void DeconflictionPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold));
    g.drawText("DECONFLICTION", getLocalBounds().removeFromTop(16), juce::Justification::topLeft, false);
}

void DeconflictionPanel::resized()
{
    const int labelH   = 16;
    const int comboH   = 22;
    const int comboGap = 6;
    const int gap      = 10;
    const int diam     = voicesKnob.getDiameter(); // 72 (medium)
    const int totalW   = diam + gap + diam;

    auto bounds = getLocalBounds().withTrimmedTop(labelH);

    // ComboBox at top, full width minus some horizontal padding
    const int comboPadH = (bounds.getWidth() - totalW) / 2;
    modeSelector.setBounds(bounds.getX() + comboPadH, bounds.getY(),
                           totalW, comboH);

    // Two knobs side-by-side below the combo box
    const juce::Rectangle<int> knobArea = bounds.withTrimmedTop(comboH + comboGap);
    const int cx = knobArea.getCentreX();
    const int cy = knobArea.getCentreY();

    voicesKnob .setBounds(cx - totalW / 2,             cy - diam / 2, diam, diam);
    staggerKnob.setBounds(cx - totalW / 2 + diam + gap, cy - diam / 2, diam, diam);
}
