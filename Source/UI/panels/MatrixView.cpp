// Source/UI/panels/MatrixView.cpp
#include "MatrixView.h"
#include "../Theme.h"

namespace kaigen::phantom
{

MatrixView::MatrixView(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    struct ModDef { ModSlot::Type type; const char* modId; const char* label; };
    const ModDef defs[] = {
        // Engine A modulators
        { ModSlot::Type::Macro,  "macro1",  "MAC 1" },
        { ModSlot::Type::Macro,  "macro2",  "MAC 2" },
        { ModSlot::Type::Lfo,    "lfo1",    "LFO 1" },
        { ModSlot::Type::Lfo,    "lfo2",    "LFO 2" },
        { ModSlot::Type::Random, "randomA", "RAND"  },
        // Engine B modulators
        { ModSlot::Type::Macro,  "macro3",  "MAC 3" },
        { ModSlot::Type::Macro,  "macro4",  "MAC 4" },
        { ModSlot::Type::Lfo,    "lfo3",    "LFO 3" },
        { ModSlot::Type::Lfo,    "lfo4",    "LFO 4" },
        { ModSlot::Type::Random, "randomB", "RAND"  },
    };
    for (const auto& def : defs)
    {
        auto* row = new MatrixModRow(processor, def.type, def.modId, def.label);
        addAndMakeVisible(*row);
        modRows.add(row);
    }
}

MatrixView::~MatrixView() = default;

juce::Rectangle<int> MatrixView::getCardBounds() const noexcept
{
    // MatrixView card spans full editor width minus margin (unlike PresetBrowser's
    // fixed 600px card) — Tasks 4-5 need the horizontal real estate for the
    // destination grid columns.
    return getLocalBounds().reduced(kCardMargin);
}

void MatrixView::visibilityChanged()
{
    if (isVisible()) repaint();
}

void MatrixView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xc0000000));
    auto card = getCardBounds();

    g.setColour(Theme::matrixBg);
    g.fillRoundedRectangle(card.toFloat(), 6.0f);
    g.setColour(Theme::panelBorder);
    g.drawRoundedRectangle(card.toFloat(), 6.0f, 1.0f);

    auto titleBar = card.removeFromTop(kTitleBarHeight).reduced(16, 8);
    g.setColour(Theme::textPrimary);
    g.setFont(juce::FontOptions("Space Grotesk", 14.0f, juce::Font::bold));
    g.drawText("Modulation Matrix", titleBar.toFloat(), juce::Justification::centredLeft, false);

    // Right side placeholder until Task 5.
    auto bodyArea = card.withTrimmedLeft(kStripWidth);
    g.setColour(Theme::textDim);
    g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::plain));
    g.drawText("Destination grid (Task 5)", bodyArea.toFloat(),
               juce::Justification::centred, false);
}

void MatrixView::resized()
{
    auto card = getCardBounds();
    card.removeFromTop(kTitleBarHeight);

    auto strip = card.removeFromLeft(kStripWidth).reduced(8);
    const int n = modRows.size();
    if (n == 0) return;

    // Distribute remainder pixels evenly so the bottom row reaches the strip edge.
    const int totalH = strip.getHeight();
    const int x = strip.getX();
    const int w = strip.getWidth();
    for (int i = 0; i < n; ++i)
    {
        const int y0 = strip.getY() + (totalH * i)       / n;
        const int y1 = strip.getY() + (totalH * (i + 1)) / n;
        modRows[i]->setBounds(x, y0, w, y1 - y0);
    }
}

void MatrixView::mouseDown(const juce::MouseEvent& e)
{
    // Click outside the card → dismiss.
    if (! getCardBounds().contains(e.getPosition()))
        setVisible(false);
}

} // namespace kaigen::phantom
