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
                            const juce::StringArray& lbls,
                            int rows)
    : apvtsRef(&apvts), labels(lbls), numRows(juce::jmax(1, rows))
{
    const auto idStr = juce::String(paramID);
    if (idStr.startsWith("a_") || idStr.startsWith("b_"))
    {
        enginePrefix = idStr.substring(0, 2);
        leafName     = idStr.substring(2);
    }
    else
    {
        leafName = idStr;
    }

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

void WordSelector::setEnginePrefix(const juce::String& activePrefix,
                                     const juce::String& newMirrorPrefix)
{
    if (! isPerEngine() || apvtsRef == nullptr) return;
    if (activePrefix == enginePrefix && newMirrorPrefix == mirrorPrefix) return;

    enginePrefix = activePrefix;
    mirrorPrefix = newMirrorPrefix;

    attachment.reset();
    const auto fullId = enginePrefix + leafName;
    if (auto* p = apvtsRef->getParameter(fullId))
        attachment = std::make_unique<juce::ComboBoxParameterAttachment>(*p, combo);
    else
        jassertfalse;
    repaint();
}

WordSelector::~WordSelector()
{
    combo.removeListener(this);
}

void WordSelector::resized()
{
    wordBounds.clear();
    if (labels.isEmpty()) return;

    // Grid layout: ceil(n/rows) columns × `numRows` rows. Cells span
    // the full bounds; last row may have fewer items than columns.
    const int n = labels.size();
    const int cols = (n + numRows - 1) / numRows;
    const int colW = getWidth() / cols;
    const int rowH = getHeight() / numRows;
    for (int i = 0; i < n; ++i)
    {
        const int r = i / cols;
        const int c = i % cols;
        wordBounds.add(juce::Rectangle<int>(c * colW, r * rowH, colW, rowH));
    }
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

    // ── Etched vertical dividers between columns (per row) ───────────────
    const int n = labels.size();
    const int cols = (n + numRows - 1) / numRows;
    const int rowH = getHeight() / juce::jmax(1, numRows);
    for (int r = 0; r < numRows; ++r)
    {
        const int rowStart = r * cols;
        const int rowEnd   = juce::jmin(n, rowStart + cols);
        for (int i = rowStart + 1; i < rowEnd; ++i)
        {
            const int sepX = wordBounds[i].getX();
            const int yMid = r * rowH + rowH / 2;
            const int sepH = juce::jmin(rowH - 4, 14);
            const int yTop = yMid - sepH / 2;
            g.setColour(juce::Colour(0x38000000));
            g.drawLine((float) sepX - 0.5f, (float) yTop,
                        (float) sepX - 0.5f, (float) (yTop + sepH), 1.0f);
            g.setColour(juce::Colour(0x80FFFFFF));
            g.drawLine((float) sepX + 0.5f, (float) yTop,
                        (float) sepX + 0.5f, (float) (yTop + sepH), 1.0f);
        }
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

    // LINK mirror: write the same choice index to the other engine's param.
    if (mirrorPrefix.isNotEmpty() && ! isMirroring && apvtsRef != nullptr)
    {
        if (auto* other = apvtsRef->getParameter(mirrorPrefix + leafName))
        {
            juce::ScopedValueSetter<bool> guard(isMirroring, true);
            const int idx = combo.getSelectedItemIndex();
            const float norm = (float) idx / (float) juce::jmax(1, other->getNumSteps() - 1);
            other->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
        }
    }
}

} // namespace kaigen::phantom
