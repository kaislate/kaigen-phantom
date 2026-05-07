// Source/UI/widgets/ToggleGroup.cpp
#include "ToggleGroup.h"
#include "../Theme.h"

namespace kaigen::phantom
{

ToggleGroup::ToggleGroup(juce::AudioProcessorValueTreeState& apvts,
                         juce::StringRef paramID,
                         const juce::StringArray& labels)
{
    combo.setVisible(false);
    addChildComponent(combo);
    for (int i = 0; i < labels.size(); ++i)
        combo.addItem(labels[i], i + 1);  // ComboBox item IDs are 1-indexed
    combo.addListener(this);

    if (auto* param = apvts.getParameter(paramID))
        attachment = std::make_unique<juce::ComboBoxParameterAttachment>(*param, combo);
    else
        jassertfalse;  // unknown paramID: typo or stale reference

    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = new juce::TextButton(labels[i]);
        b->setClickingTogglesState(false);
        b->onClick = [this, i] { buttonClicked(i); };
        addAndMakeVisible(*b);
        buttons.add(b);
    }
}

ToggleGroup::~ToggleGroup()
{
    combo.removeListener(this);
}

void ToggleGroup::paint(juce::Graphics& g)
{
    // Highlight the active button. Active = combo.getSelectedItemIndex().
    const int active = combo.getSelectedItemIndex();
    for (int i = 0; i < buttons.size(); ++i)
    {
        auto bounds = buttons[i]->getBounds().toFloat();
        if (i == active)
        {
            g.setColour(Theme::activeGlow);
            g.fillRoundedRectangle(bounds.expanded(2.0f), 4.0f);
        }
    }
}

void ToggleGroup::resized()
{
    auto area = getLocalBounds();
    if (buttons.isEmpty()) return;
    const int btnWidth = area.getWidth() / buttons.size();
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds(area.removeFromLeft(btnWidth));
}

void ToggleGroup::buttonClicked(int index)
{
    combo.setSelectedItemIndex(index, juce::sendNotificationSync);
}

void ToggleGroup::comboBoxChanged(juce::ComboBox*)
{
    repaint();  // active highlight follows the combo
}

} // namespace kaigen::phantom
