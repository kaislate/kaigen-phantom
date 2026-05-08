// Source/UI/widgets/MatrixModRow.cpp
#include "MatrixModRow.h"
#include "../Theme.h"

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
    const auto bounds = getLocalBounds().reduced(4, 2);

    // Type-color border.
    juce::Colour accent;
    switch (type)
    {
        case ModSlot::Type::Macro:  accent = Theme::macroTeal;    break;
        case ModSlot::Type::Lfo:    accent = Theme::lfoBlue;      break;
        case ModSlot::Type::Random: accent = Theme::randomPurple; break;
        case ModSlot::Type::Morph:  accent = Theme::morphWhite;   break;
    }

    const float alpha = (type == ModSlot::Type::Macro) ? 1.0f : 0.45f;

    // Left dot.
    g.setColour(accent.withAlpha(alpha));
    const float dotR = 4.0f;
    const auto centreY = bounds.getCentreY();
    g.fillEllipse(bounds.getX() + 4.0f, (float) centreY - dotR, dotR * 2.0f, dotR * 2.0f);

    // Name.
    g.setFont(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold));
    g.drawText(label, bounds.withTrimmedLeft(20).withWidth(60).toFloat(),
               juce::Justification::centredLeft, false);

    // Value readout (right-aligned).
    g.setColour(Theme::textSecondary);
    g.setFont(juce::FontOptions("Space Grotesk", 9.0f, juce::Font::plain));
    g.drawText(valueText, bounds.withTrimmedRight(4).toFloat(),
               juce::Justification::centredRight, false);
}

void MatrixModRow::resized()
{
    // No children for now.
}

} // namespace kaigen::phantom
