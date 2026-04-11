#include "GhostPanel.h"
#include "PhantomColours.h"
#include "../Parameters.h"

GhostPanel::GhostPanel(juce::AudioProcessorValueTreeState& apvts)
    : apvtsPtr(&apvts)
{
    addAndMakeVisible(amountKnob);
    addAndMakeVisible(thresholdKnob);
    addAndMakeVisible(replaceBtn);
    addAndMakeVisible(addBtn);

    amountAttachment    = std::make_unique<SliderAttachment>(apvts, ParamID::GHOST,             amountKnob);
    thresholdAttachment = std::make_unique<SliderAttachment>(apvts, ParamID::PHANTOM_THRESHOLD, thresholdKnob);

    // Radio group — only one can be on at a time
    replaceBtn.setRadioGroupId(2);
    addBtn    .setRadioGroupId(2);
    replaceBtn.setClickingTogglesState(true);
    addBtn    .setClickingTogglesState(true);

    // Replace starts toggled on
    replaceBtn.setToggleState(true, juce::dontSendNotification);

    replaceBtn.onClick = [this]
    {
        if (auto* param = apvtsPtr->getParameter(ParamID::GHOST_MODE))
            param->setValueNotifyingHost(param->convertTo0to1(0.0f));
    };

    addBtn.onClick = [this]
    {
        if (auto* param = apvtsPtr->getParameter(ParamID::GHOST_MODE))
            param->setValueNotifyingHost(param->convertTo0to1(1.0f));
    };
}

void GhostPanel::paintContent(juce::Graphics& g)
{
    g.setColour(PhantomColours::textDim);
    g.setFont(juce::Font(7.0f, juce::Font::bold));
    g.drawText("GHOST", getLocalBounds().removeFromTop(16), juce::Justification::topLeft, false);
}

void GhostPanel::resized()
{
    const int labelH    = 16;
    const int btnW      = 28;
    const int btnH      = 14;
    const int btnGap    = 4;
    const int gap       = 10;
    const int largeDiam = amountKnob.getDiameter();    // 100
    const int medDiam   = thresholdKnob.getDiameter(); // 72

    const auto bounds = getLocalBounds();

    // Toggle buttons — top-right of the label row
    const int btnAreaRight = bounds.getRight() - 4;
    const int btnY         = (labelH - btnH) / 2;
    addBtn    .setBounds(btnAreaRight - btnW,           btnY, btnW, btnH);
    replaceBtn.setBounds(btnAreaRight - btnW * 2 - btnGap, btnY, btnW, btnH);

    // Knobs — centred below the label row
    const juce::Rectangle<int> contentArea = bounds.withTrimmedTop(labelH);
    const int cx = contentArea.getCentreX();
    const int cy = contentArea.getCentreY();

    amountKnob   .setBounds(cx - gap / 2 - largeDiam, cy - largeDiam / 2, largeDiam, largeDiam);
    thresholdKnob.setBounds(cx + gap / 2,             cy - medDiam   / 2, medDiam,   medDiam);
}
