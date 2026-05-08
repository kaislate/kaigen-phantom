// Source/UI/panels/ModulationPanel.cpp
#include "ModulationPanel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

ModulationPanel::ModulationPanel(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    slotsButton .setClickingTogglesState(true);
    slotsButton .setToggleState(true, juce::dontSendNotification);
    matrixButton.setClickingTogglesState(true);

    slotsButton.onClick = [this] {
        slotsButton.setToggleState(true, juce::dontSendNotification);
        // Slot view is the default; SLOTS button only deactivates matrix.
        if (matrixActive) {
            matrixActive = false;
            matrixButton.setToggleState(false, juce::dontSendNotification);
            if (onMatrixToggle) onMatrixToggle(false);
        }
    };
    matrixButton.onClick = [this] {
        matrixActive = ! matrixActive;
        matrixButton.setToggleState(matrixActive, juce::dontSendNotification);
        slotsButton .setToggleState(! matrixActive, juce::dontSendNotification);
        if (onMatrixToggle) onMatrixToggle(matrixActive);
    };

    addAndMakeVisible(slotsButton);
    addAndMakeVisible(matrixButton);

    counterLabel.setColour(juce::Label::textColourId, Theme::textSecondary);
    counterLabel.setFont(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold));
    counterLabel.setJustificationType(juce::Justification::centredRight);
    counterLabel.setText("0 ROUTINGS", juce::dontSendNotification);
    addAndMakeVisible(counterLabel);
}

ModulationPanel::~ModulationPanel() = default;

void ModulationPanel::setRoutingCount(int count)
{
    counterLabel.setText(juce::String(count) + (count == 1 ? " ROUTING" : " ROUTINGS"),
                          juce::dontSendNotification);
}

void ModulationPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);
    g.setColour(Theme::panelBorder);
    g.drawHorizontalLine(0, 0.0f, (float) getWidth());
}

void ModulationPanel::resized()
{
    auto area = getLocalBounds();

    // Mode bar at top of panel.
    auto modeBar = area.removeFromTop(30).reduced(12, 4);
    slotsButton .setBounds(modeBar.removeFromLeft(64));
    modeBar.removeFromLeft(4);
    matrixButton.setBounds(modeBar.removeFromLeft(64));

    counterLabel.setBounds(modeBar.removeFromRight(140));
}

} // namespace kaigen::phantom
