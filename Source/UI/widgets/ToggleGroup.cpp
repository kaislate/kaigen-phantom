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
        // Tag for the LookAndFeel so it knows to render the .mt mode-toggle
        // segment style (lighter touch than the .hdr-btn raised pill).
        b->getProperties().set("phantom-style", "mt-segment");
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
    // ── .mt container pill ───────────────────────────────────────────────
    // CSS: background rgba(0,0,0,0.08), border-radius 20px, neumorphic inset
    // shadows: inset 1.5px 1.5px 5px rgba(0,0,0,0.14) +
    //          inset -1.5px -1.5px 4px rgba(255,255,255,0.50)
    const auto bounds = getLocalBounds().toFloat();
    const float corner = bounds.getHeight() * 0.5f;

    g.setColour(juce::Colour(0x14000000));    // 8% black bg
    g.fillRoundedRectangle(bounds, corner);

    // Top-left inset dark line.
    g.setColour(juce::Colour(0x24000000));
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner - 0.5f, 0.7f);
    // Bottom-right white hairline highlight.
    g.setColour(juce::Colour(0x80FFFFFF));
    g.drawLine(bounds.getX() + corner, bounds.getBottom() - 0.5f,
                bounds.getRight() - corner, bounds.getBottom() - 0.5f, 0.5f);
}

void ToggleGroup::resized()
{
    auto area = getLocalBounds().reduced(3);   // 3 px inner padding
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
    // Sync each button's toggle state to the active selection so the
    // LookAndFeel can paint the active segment correctly.
    const int active = combo.getSelectedItemIndex();
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState(i == active, juce::dontSendNotification);
    repaint();
}

} // namespace kaigen::phantom
