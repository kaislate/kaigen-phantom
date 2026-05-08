// Source/UI/panels/PresetBrowser.cpp
#include "PresetBrowser.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../PresetManager.h"

namespace kaigen::phantom
{

PresetBrowser::PresetBrowser(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    closeButton.onClick = [this] { setVisible(false); };
    addAndMakeVisible(closeButton);
}

PresetBrowser::~PresetBrowser() = default;

void PresetBrowser::visibilityChanged()
{
    if (! isVisible())
    {
        rows.clear();
        return;
    }

    // Rebuild row list each time the browser opens.
    rows.clear();
    const auto all = processor.getPresetManager().getAllPresets();
    for (const auto& [packName, presets] : all)
    {
        Row header;
        header.pack = packName;
        header.isHeader = true;
        rows.push_back(header);
        for (const auto& p : presets)
            rows.push_back({ p.metadata.name, packName, false });
    }
    repaint();
}

void PresetBrowser::loadPresetAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= (int) rows.size()) return;
    const auto& r = rows[(size_t) rowIndex];
    if (r.isHeader) return;
    if (processor.getPresetManager().loadPreset(apvts, r.name, r.pack))
    {
        if (onPresetSelected) onPresetSelected(r.name, r.pack);
        setVisible(false);
    }
}

void PresetBrowser::paint(juce::Graphics& g)
{
    // Backdrop scrim.
    g.fillAll(juce::Colour(0xc0000000));

    // Centered card.
    const auto bounds = getLocalBounds();
    const int cardW = juce::jmin(kCardWidthPx, bounds.getWidth() - 2 * kCardMargin);
    const int cardH = bounds.getHeight() - 2 * kCardMargin;
    const int cardX = (bounds.getWidth() - cardW) / 2;
    const int cardY = kCardMargin;
    auto cardBounds = juce::Rectangle<int>(cardX, cardY, cardW, cardH);

    g.setColour(Theme::matrixBg);
    g.fillRoundedRectangle(cardBounds.toFloat(), 6.0f);
    g.setColour(Theme::panelBorder);
    g.drawRoundedRectangle(cardBounds.toFloat(), 6.0f, 1.0f);

    // Title bar inside card.
    auto titleBar = cardBounds.removeFromTop(36).reduced(12, 8);
    g.setColour(Theme::textPrimary);
    g.setFont(juce::FontOptions("Space Grotesk", 14.0f, juce::Font::bold));
    g.drawText("Presets", titleBar.toFloat(), juce::Justification::centredLeft, false);

    // Row list inside card.
    auto rowArea = cardBounds.reduced(0, 8);
    int y = rowArea.getY();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (y + h > rowArea.getBottom()) break;  // simple clip; no scroll for v1

        auto rowBounds = juce::Rectangle<int>(rowArea.getX() + 8, y, rowArea.getWidth() - 16, h);

        if (r.isHeader)
        {
            g.setColour(Theme::textSecondary);
            g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::bold));
            g.drawText(r.pack.toUpperCase(), rowBounds.toFloat(), juce::Justification::centredLeft, false);
        }
        else
        {
            const bool hovered = ((int) i == hoverRow);
            if (hovered)
            {
                g.setColour(Theme::activeGlow);
                g.fillRoundedRectangle(rowBounds.toFloat(), 3.0f);
            }
            g.setColour(Theme::textPrimary);
            g.setFont(juce::FontOptions("Space Grotesk", 12.0f, juce::Font::plain));
            g.drawText(r.name, rowBounds.reduced(8, 0).toFloat(), juce::Justification::centredLeft, false);
        }
        y += h;
    }
}

void PresetBrowser::resized()
{
    const auto bounds = getLocalBounds();
    const int cardW = juce::jmin(kCardWidthPx, bounds.getWidth() - 2 * kCardMargin);
    const int cardX = (bounds.getWidth() - cardW) / 2;
    const int cardY = kCardMargin;

    closeButton.setBounds(cardX + cardW - 36, cardY + 6, 28, 24);
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e)
{
    const auto bounds = getLocalBounds();
    const int cardW = juce::jmin(kCardWidthPx, bounds.getWidth() - 2 * kCardMargin);
    const int cardH = bounds.getHeight() - 2 * kCardMargin;
    const int cardX = (bounds.getWidth() - cardW) / 2;
    const int cardY = kCardMargin;

    // Click outside the card → dismiss.
    if (e.x < cardX || e.x > cardX + cardW || e.y < cardY || e.y > cardY + cardH)
    {
        setVisible(false);
        return;
    }

    // Hit-test rows. Title bar at top 36px is non-interactive.
    const int rowAreaY = cardY + 36 + 8;
    const int rowAreaH = cardH - 36 - 16;
    if (e.y < rowAreaY || e.y > rowAreaY + rowAreaH) return;

    int y = rowAreaY;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (e.y >= y && e.y < y + h)
        {
            loadPresetAt((int) i);
            return;
        }
        y += h;
    }
}

} // namespace kaigen::phantom
