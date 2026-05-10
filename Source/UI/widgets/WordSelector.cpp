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

    juce::Font font(juce::FontOptions("Space Grotesk", kFontSize, juce::Font::bold));
    font.setExtraKerningFactor(kKerning);

    // Measure each word's rendered width, then center the row.
    int total = 0;
    juce::Array<int> widths;
    for (const auto& w : labels)
    {
        const int wpx = juce::GlyphArrangement::getStringWidthInt(font, w.toUpperCase()) + 4;
        widths.add(wpx);
        total += wpx;
    }
    const int gap = juce::jmax(8, (getWidth() - total) / juce::jmax(1, labels.size() + 1));

    int x = juce::jmax(4, (getWidth() - (total + gap * (labels.size() - 1))) / 2);
    const int y = (getHeight() - (int) kFontSize) / 2;
    for (int i = 0; i < labels.size(); ++i)
    {
        wordBounds.add(juce::Rectangle<int>(x, y, widths[i], (int) kFontSize + kUnderlineGap + 2));
        x += widths[i] + gap;
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

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto bounds = wordBounds[i];
        const bool isActive = (i == active);
        const bool isHover  = (i == hoverIndex);

        // .lw text: 22% black inactive / 65% black active. White etch shadow.
        const auto textColour = isActive ? juce::Colour(0xa6000000)         // 65% black
                                         : (isHover ? juce::Colour(0x80000000) // 50% on hover
                                                    : juce::Colour(0x38000000));// 22%

        const auto upper = labels[i].toUpperCase();

        // Etched effect: white shadow 1 px below glyph + foreground.
        g.setColour(juce::Colour(0x80FFFFFF));
        g.drawText(upper, bounds.translated(0, 1), juce::Justification::centred, false);
        g.setColour(textColour);
        g.drawText(upper, bounds, juce::Justification::centred, false);

        // Underline (full-width of the word slot).
        const float underlineY = (float) bounds.getBottom() - 2.0f;
        const auto underlineColour = isActive ? juce::Colour(0x73000000)    // 45% black active
                                              : juce::Colour(0x38000000);   // 22% inactive
        g.setColour(underlineColour);
        g.drawLine((float) bounds.getX() + 1.0f, underlineY,
                    (float) bounds.getRight() - 1.0f, underlineY, 1.0f);
        // White-50% shadow below underline (etched into surface).
        g.setColour(juce::Colour(0x80FFFFFF));
        g.drawLine((float) bounds.getX() + 1.0f, underlineY + 1.0f,
                    (float) bounds.getRight() - 1.0f, underlineY + 1.0f, 1.0f);
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
