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

    // Etched dark on silver, slightly brighter than the ~22% standard.
    counterLabel.setColour(juce::Label::textColourId, juce::Colour(0xa0000000));   // ~63% black
    counterLabel.setFont(juce::Font(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold))
                            .withExtraKerningFactor(0.18f));
    counterLabel.setJustificationType(juce::Justification::centredRight);
    counterLabel.setText("0 ROUTINGS", juce::dontSendNotification);
    addAndMakeVisible(counterLabel);

    // Slot row: 11 slots in WebView2 order.
    struct SlotDef { ModSlot::Type type; const char* slotId; const char* paramID; const char* label; const char* placeholder; };
    const SlotDef defs[] = {
        { ModSlot::Type::Lfo,    "lfo1",    "",             "LFO 1", "PR4" },
        { ModSlot::Type::Lfo,    "lfo2",    "",             "LFO 2", "PR4" },
        { ModSlot::Type::Random, "randomA", "",             "RAND",  "PR5" },
        { ModSlot::Type::Macro,  "macro1",  "macro1",       "m1",    "" },
        { ModSlot::Type::Macro,  "macro2",  "macro2",       "m2",    "" },
        { ModSlot::Type::Morph,  "morph",   "morph_amount", "Morph", "" },
        { ModSlot::Type::Macro,  "macro3",  "macro3",       "m3",    "" },
        { ModSlot::Type::Macro,  "macro4",  "macro4",       "m4",    "" },
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

    // ── Top row: mode-bar buttons (left) + always-visible macros + morph (right) ──
    auto topRow = area.removeFromTop(getHeight() < 90 ? getHeight() : 60).reduced(12, 4);

    // Mode bar — left.
    slotsButton.setBounds(topRow.removeFromLeft(96));   // wider to fit "MODULATIONS"
    topRow.removeFromLeft(4);
    matrixButton.setBounds(topRow.removeFromLeft(64));

    // Macros + Morph — right (always visible).
    // Order in the grouped cluster: Mac1 Mac2 Mac3 Mac4 [gap] Morph.
    if (slots.size() >= 8)   // sanity
    {
        constexpr int kMacroW    = 44;
        constexpr int kMorphW    = 92;
        constexpr int kSlotGap   = 10;   // wider spacing between macros
        constexpr int kMorphGap  = 16;

        // Slot indices in `slots`: macro1=3, macro2=4, morph=5, macro3=6, macro4=7.
        constexpr int macroIdxs[4] = { 3, 4, 6, 7 };
        constexpr int morphIdx  = 5;

        // Reserve width on the right side of topRow.
        const int totalW = kMacroW * 4 + kSlotGap * 3 + kMorphGap + kMorphW;
        // Counter sits to the right of macros+morph.
        counterLabel.setBounds(topRow.removeFromRight(110));
        topRow.removeFromRight(8);

        auto slotsArea = topRow.removeFromRight(totalW);
        // Lay out macros 1-4.
        for (int i = 0; i < 4; ++i)
        {
            auto slot = slots[macroIdxs[i]];
            slot->setBounds(slotsArea.removeFromLeft(kMacroW));
            slot->setVisible(true);
            if (i < 3) slotsArea.removeFromLeft(kSlotGap);
        }
        slotsArea.removeFromLeft(kMorphGap);
        slots[morphIdx]->setBounds(slotsArea.removeFromLeft(kMorphW));
        slots[morphIdx]->setVisible(true);
    }

    // ── Expanded row: full slot row (LFO + Random + all 11 slots) ──
    const bool fullRowVisible = slotsExpanded && ! matrixActive;
    for (int i = 0; i < slots.size(); ++i)
    {
        // Macros + Morph stay always visible. LFO/Random visible only in
        // expanded mode.
        const bool isMacroOrMorph = (i == 3 || i == 4 || i == 5 || i == 6 || i == 7);
        if (! isMacroOrMorph)
            slots[i]->setVisible(fullRowVisible);
    }

    if (! fullRowVisible || slots.isEmpty() || area.getHeight() < 30) return;

    // Expanded row positions ONLY the LFO + Random slots (the 6 not in the
    // top row): indices 0, 1, 2, 8, 9, 10. Macros + Morph stay in the top
    // row (they're already visible there).
    constexpr int lfoRandomIdxs[6] = { 0, 1, 2, 8, 9, 10 };
    auto slotRow = area.reduced(12, 4);
    const int slotW = slotRow.getWidth() / 6;
    for (int idx : lfoRandomIdxs)
    {
        slots[idx]->setBounds(slotRow.removeFromLeft(slotW));
    }
}

} // namespace kaigen::phantom
