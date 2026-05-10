// Source/UI/panels/ModulationPanel.cpp
#include "ModulationPanel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

ModulationPanel::ModulationPanel(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    slotsButton .setClickingTogglesState(true);
    slotsButton .setToggleState(false, juce::dontSendNotification);   // hidden by default
    matrixButton.setClickingTogglesState(true);

    // MODULATIONS toggles slot-row visibility. If MATRIX is active, also
    // close it so the slot row can show.
    slotsButton.onClick = [this] {
        slotsExpanded = ! slotsExpanded;
        slotsButton.setToggleState(slotsExpanded, juce::dontSendNotification);
        if (slotsExpanded && matrixActive)
        {
            matrixActive = false;
            matrixButton.setToggleState(false, juce::dontSendNotification);
            if (onMatrixToggle) onMatrixToggle(false);
        }
        for (auto* slot : slots)
            slot->setVisible(slotsExpanded);
        if (onSlotsExpandedChanged) onSlotsExpandedChanged(slotsExpanded);
        resized();
    };

    // MATRIX toggles the matrix overlay. If slot row is open, hide it.
    matrixButton.onClick = [this] {
        matrixActive = ! matrixActive;
        matrixButton.setToggleState(matrixActive, juce::dontSendNotification);
        if (matrixActive && slotsExpanded)
        {
            slotsExpanded = false;
            slotsButton.setToggleState(false, juce::dontSendNotification);
            for (auto* slot : slots)
                slot->setVisible(false);
            if (onSlotsExpandedChanged) onSlotsExpandedChanged(false);
        }
        if (onMatrixToggle) onMatrixToggle(matrixActive);
    };

    addAndMakeVisible(slotsButton);
    addAndMakeVisible(matrixButton);

    counterLabel.setColour(juce::Label::textColourId, Theme::textSecondary);
    counterLabel.setFont(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold));
    counterLabel.setJustificationType(juce::Justification::centredRight);
    counterLabel.setText("0 ROUTINGS", juce::dontSendNotification);
    addAndMakeVisible(counterLabel);

    // Slot row: 11 slots in WebView2 order.
    struct SlotDef { ModSlot::Type type; const char* slotId; const char* paramID; const char* label; const char* placeholder; };
    const SlotDef defs[] = {
        { ModSlot::Type::Lfo,    "lfo1",    "",             "LFO 1", "PR4" },
        { ModSlot::Type::Lfo,    "lfo2",    "",             "LFO 2", "PR4" },
        { ModSlot::Type::Random, "randomA", "",             "RAND",  "PR5" },
        { ModSlot::Type::Macro,  "macro1",  "macro1",       "MAC 1", "" },
        { ModSlot::Type::Macro,  "macro2",  "macro2",       "MAC 2", "" },
        { ModSlot::Type::Morph,  "morph",   "morph_amount", "MORPH", "" },
        { ModSlot::Type::Macro,  "macro3",  "macro3",       "MAC 3", "" },
        { ModSlot::Type::Macro,  "macro4",  "macro4",       "MAC 4", "" },
        { ModSlot::Type::Random, "randomB", "",             "RAND",  "PR5" },
        { ModSlot::Type::Lfo,    "lfo3",    "",             "LFO 3", "PR4" },
        { ModSlot::Type::Lfo,    "lfo4",    "",             "LFO 4", "PR4" },
    };
    for (const auto& def : defs)
    {
        auto* slot = new ModSlot(apvts, def.type, def.slotId, def.paramID, def.label, def.placeholder);
        addChildComponent(*slot);   // hidden by default; slotsExpanded toggles visibility
        slots.add(slot);
    }
}

ModulationPanel::~ModulationPanel() = default;

void ModulationPanel::setRoutingCount(int count)
{
    counterLabel.setText(juce::String(count) + (count == 1 ? " ROUTING" : " ROUTINGS"),
                          juce::dontSendNotification);
}

void ModulationPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::modPanelBg);

    // Top border hairline.
    g.setColour(Theme::modPanelBorder);
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

    // Slot row visible only when MODULATIONS toggle is expanded (and matrix
    // mode is off).
    const bool slotRowVisible = slotsExpanded && ! matrixActive;
    for (auto* slot : slots)
        slot->setVisible(slotRowVisible);

    if (! slotRowVisible || slots.isEmpty()) return;

    auto slotRow = area.reduced(12, 4);
    const int slotW = slotRow.getWidth() / slots.size();
    for (auto* slot : slots)
        slot->setBounds(slotRow.removeFromLeft(slotW));
}

} // namespace kaigen::phantom
