// Source/UI/widgets/MatrixModRow.cpp
#include "MatrixModRow.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../Modulation/ModulationEngine.h"
#include "../../Modulation/Modulator.h"
#include "../../Modulation/Macro.h"

namespace kaigen::phantom
{

MatrixModRow::MatrixModRow(PhantomProcessor& p, ModSlot::Type t,
                            const juce::String& id, const juce::String& lbl)
    : processor(p), type(t), modId(id), label(lbl)
{
}

MatrixModRow::~MatrixModRow() = default;

void MatrixModRow::setValueText(const juce::String& text)
{
    if (text != valueText)
    {
        valueText = text;
        repaint();
    }
}

void MatrixModRow::paint(juce::Graphics& g)
{
    const auto fullBounds = getLocalBounds().toFloat();
    const auto bounds     = getLocalBounds().reduced(4, 2);

    // Type-color accent.
    juce::Colour accent;
    switch (type)
    {
        case ModSlot::Type::Macro:  accent = Theme::macroTeal;    break;
        case ModSlot::Type::Lfo:    accent = Theme::lfoBlue;      break;
        case ModSlot::Type::Random: accent = Theme::randomPurple; break;
        case ModSlot::Type::Morph:  accent = Theme::morphWhite;   break;
        default:                    accent = Theme::lfoBlue;       break;
    }

    // ── Strip background — semi-transparent dark (rgba(20,24,30,0.5)) ──────
    g.setColour(Theme::mtxModStrip);
    g.fillRoundedRectangle(fullBounds, 3.0f);

    // ── 2 px left accent bar in type color ───────────────────────────────
    g.setColour(accent);
    g.fillRect(fullBounds.getX(), fullBounds.getY(), 2.0f, fullBounds.getHeight());

    const float alpha = (type == ModSlot::Type::Macro) ? 1.0f : 0.45f;

    // Left dot.
    g.setColour(accent.withAlpha(alpha));
    const float dotR = 4.0f;
    const auto centreY = bounds.getCentreY();
    g.fillEllipse(bounds.getX() + 4.0f, (float) centreY - dotR, dotR * 2.0f, dotR * 2.0f);

    // Name — white text on dark strip.
    g.setColour(Theme::vizText.withAlpha(alpha));
    g.setFont(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold));
    g.drawText(label, bounds.withTrimmedLeft(20).withWidth(60).toFloat(),
               juce::Justification::centredLeft, false);

    // Value readout (right-aligned).
    g.setColour(Theme::vizText.withAlpha(0.5f * alpha));
    g.setFont(juce::FontOptions("Space Grotesk", 9.0f, juce::Font::plain));
    g.drawText(valueText, bounds.withTrimmedRight(4).toFloat(),
               juce::Justification::centredRight, false);
}

void MatrixModRow::resized()
{
    // No children for now (nameEditor positioned in mouseDown).
}

void MatrixModRow::mouseDown(const juce::MouseEvent&)
{
    if (type != ModSlot::Type::Macro) return;
    if (nameEditor != nullptr) return;

    nameEditor = std::make_unique<juce::TextEditor>();
    nameEditor->setText(label);
    nameEditor->setBounds(getLocalBounds().withTrimmedLeft(20).withWidth(100));
    nameEditor->onReturnKey = [this] { commitNameEdit(); };
    nameEditor->onEscapeKey = [this] { cancelNameEdit(); };
    nameEditor->onFocusLost = [this] { commitNameEdit(); };
    addAndMakeVisible(*nameEditor);
    nameEditor->grabKeyboardFocus();
    nameEditor->selectAll();
}

void MatrixModRow::commitNameEdit()
{
    if (nameEditor == nullptr) return;
    const auto newName = nameEditor->getText().trim();
    if (newName.isNotEmpty())
    {
        // Macros 1-2 belong to engine A; 3-4 to engine B.
        auto& engine = (modId == "macro1" || modId == "macro2")
            ? processor.getModulationEngineA()
            : processor.getModulationEngineB();
        if (auto* mod = engine.findModulator(modId))
        {
            if (auto* macro = dynamic_cast<Macro*>(mod))
                macro->setName(newName);
        }
        label = newName;
    }
    nameEditor.reset();
    repaint();
}

void MatrixModRow::cancelNameEdit()
{
    nameEditor.reset();
}

} // namespace kaigen::phantom
