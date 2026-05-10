// Source/UI/widgets/WordSelector.cpp
#include "WordSelector.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    constexpr float kFontSize  = 10.0f;
    constexpr float kKerning   = 0.25f;          // 2.5 px letter-spacing
    constexpr int   kUnderlineGap = 1;
}

WordSelector::WordSelector(juce::AudioProcessorValueTreeState& apvts,
                            juce::StringRef paramID,
                            const juce::StringArray& lbls)
    : labels(lbls)
{
    combo.setVisible(false);
    addChildComponent(combo);
    for (int i = 0; i < labels.size(); ++i)
        combo.addItem(labels[i], i + 1);
    combo.addListener(this);

    if (auto* param = apvts.getParameter(paramID))
        attachment = std::make_unique<juce::ComboBoxParameterAttachment>(*param, combo);
    else
        jassertfalse;
}

WordSelector::~WordSelector()
{
    combo.removeListener(this);
}

void WordSelector::resized()
{
    wordBounds.clear();
    if (labels.isEmpty()) return;

    // Equal-width columns spanning the full bounds. Vertical etched dividers
    // sit between adjacent columns at the column boundaries.
    const int n = labels.size();
    const int colW = getWidth() / n;
    for (int i = 0; i < n; ++i)
        wordBounds.add(juce::Rectangle<int>(i * colW, 0, colW, getHeight()));
}

int WordSelector::hitWord(juce::Point<int> p) const
{
    for (int i = 0; i < wordBounds.size(); ++i)
        if (wordBounds[i].contains(p))
            return i;
    return -1;
}

void WordSelector::paint(juce::Graphics& g)
{
    if (labels.isEmpty()) return;

    const int active = combo.getSelectedItemIndex();

    juce::Font font(juce::FontOptions("Space Grotesk", kFontSize, juce::Font::bold));
    font.setExtraKerningFactor(kKerning);
    g.setFont(font);

    // ── Etched vertical dividers between slots ───────────────────────────
    // CSS .preset-sep: 2x14 px, gradient half-dark / half-white (creates an
    // engraved seam). Drawn at column boundaries.
    for (int i = 1; i < labels.size(); ++i)
    {
        const int sepX = wordBounds[i].getX();
        const int yMid = getHeight() / 2;
        const int sepH = juce::jmin(14, getHeight() - 6);
        const int yTop = yMid - sepH / 2;
        // Dark side (left of seam) — engraved-into-surface.
        g.setColour(juce::Colour(0x38000000));   // 22% black
        g.drawLine((float) sepX - 0.5f, (float) yTop,
                    (float) sepX - 0.5f, (float) (yTop + sepH), 1.0f);
        // Bright side (right of seam) — surface highlight.
        g.setColour(juce::Colour(0x80FFFFFF));   // 50% white
        g.drawLine((float) sepX + 0.5f, (float) yTop,
                    (float) sepX + 0.5f, (float) (yTop + sepH), 1.0f);
    }

    // ── Words ────────────────────────────────────────────────────────────
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto bounds = wordBounds[i];
        const bool isActive = (i == active);
        const bool isHover  = (i == hoverIndex);

        const auto upper = labels[i].toUpperCase();

        if (isActive)
        {
            // ── "Lit through soft-touch plastic" effect ─────────────────
            // The active word glows as if a backlit LED were pressing through
            // the silver surface. We layer: a soft warm halo behind, then
            // bright text on top.
            g.setColour(juce::Colour(0x40FFFFFF));   // 25% white halo glow underneath
            for (int r = 4; r >= 1; --r)
            {
                g.setColour(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.06f));
                g.drawText(upper, bounds.translated(-r, 0), juce::Justification::centred, false);
                g.drawText(upper, bounds.translated(r,  0), juce::Justification::centred, false);
                g.drawText(upper, bounds.translated(0, -r), juce::Justification::centred, false);
                g.drawText(upper, bounds.translated(0,  r), juce::Justification::centred, false);
            }
            // Bright foreground text — almost-white with a slight cool tint.
            g.setColour(juce::Colour(0xfff5f8fb));
            g.drawText(upper, bounds, juce::Justification::centred, false);
        }
        else
        {
            // Inactive — etched dark on light (the .lw default).
            const auto textColour = isHover ? juce::Colour(0x80000000)   // 50% on hover
                                              : juce::Colour(0x38000000); // 22%
            // White shadow 1 px below glyph (etched effect).
            g.setColour(juce::Colour(0x80FFFFFF));
            g.drawText(upper, bounds.translated(0, 1), juce::Justification::centred, false);
            g.setColour(textColour);
            g.drawText(upper, bounds, juce::Justification::centred, false);
        }
    }
}

void WordSelector::mouseDown(const juce::MouseEvent& e)
{
    const int idx = hitWord(e.getPosition());
    if (idx < 0) return;
    combo.setSelectedItemIndex(idx, juce::sendNotificationSync);
}

void WordSelector::mouseMove(const juce::MouseEvent& e)
{
    const int idx = hitWord(e.getPosition());
    if (idx != hoverIndex)
    {
        hoverIndex = idx;
        setMouseCursor(idx >= 0 ? juce::MouseCursor::PointingHandCursor
                                  : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void WordSelector::mouseExit(const juce::MouseEvent&)
{
    if (hoverIndex != -1) { hoverIndex = -1; repaint(); }
}

void WordSelector::comboBoxChanged(juce::ComboBox*)
{
    repaint();
}

} // namespace kaigen::phantom
