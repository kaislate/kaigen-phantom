// Source/UI/panels/MatrixView.cpp
#include "MatrixView.h"
#include "../Theme.h"

namespace
{
    std::vector<kaigen::phantom::MatrixView::CategoryGroup> buildCategories()
    {
        return {
            { "GHOST",    {{"ghost","GHOST"},{"phantom_threshold","PHTHR"},{"phantom_strength","PSTR"},{"output_gain","OUT"}} },
            { "RECIPE",   {{"recipe_h2","H2"},{"recipe_h3","H3"},{"recipe_h4","H4"},{"recipe_h5","H5"},{"recipe_h6","H6"},{"recipe_h7","H7"},{"recipe_h8","H8"},{"harmonic_saturation","HSAT"}} },
            { "SHAPE",    {{"synth_step","STEP"},{"synth_duty","DUTY"},{"synth_skip","SKIP"}} },
            { "ENV",      {{"env_attack_ms","ATK"},{"env_release_ms","REL"}} },
            { "FILTER",   {{"synth_lpf_hz","LPF"},{"synth_hpf_hz","HPF"}} },
            { "RESYN",    {{"synth_wavelet_length","WVLEN"},{"synth_gate_threshold","GATE"},{"synth_h1","H1"},{"synth_sub","SUB"}} },
            { "PITCH",    {{"synth_min_samples","MINSP"},{"synth_max_samples","MAXSP"},{"tracking_speed","TRACK"},{"punch_amount","PUNCH"},{"synth_boost_threshold","BTHR"},{"synth_boost_amount","BAMT"}} },
            { "STEREO",   {{"binaural_width","BIN"},{"stereo_width","WIDTH"}} },
        };
    }
}

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

    categories = buildCategories();

    struct ModInfo { const char* sourceId; ModSlot::Type type; bool isEngineA; };
    const ModInfo mods[] = {
        {"macro1",  ModSlot::Type::Macro,  true},
        {"macro2",  ModSlot::Type::Macro,  true},
        {"lfo1",    ModSlot::Type::Lfo,    true},
        {"lfo2",    ModSlot::Type::Lfo,    true},
        {"randomA", ModSlot::Type::Random, true},
        {"macro3",  ModSlot::Type::Macro,  false},
        {"macro4",  ModSlot::Type::Macro,  false},
        {"lfo3",    ModSlot::Type::Lfo,    false},
        {"lfo4",    ModSlot::Type::Lfo,    false},
        {"randomB", ModSlot::Type::Random, false},
    };

    for (const auto& mod : mods)
    {
        const juce::String prefix = mod.isEngineA ? "a_" : "b_";
        for (const auto& cat : categories)
            for (const auto& [leaf, abbrev] : cat.leaves)
            {
                const juce::String paramId = prefix + leaf;
                auto* cell = new MatrixCell(processor, mod.sourceId, paramId, mod.type);
                addAndMakeVisible(*cell);
                cells.add(cell);
            }
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

    // Category headers above the grid.
    auto stripAndGrid = card.reduced(8);
    stripAndGrid.removeFromLeft(kStripWidth);
    if (categories.empty()) return;

    int totalCols = 0;
    for (const auto& cat : categories) totalCols += (int) cat.leaves.size();
    if (totalCols == 0) return;
    const int cellW = stripAndGrid.getWidth() / totalCols;

    g.setColour(Theme::textSecondary);
    g.setFont(juce::FontOptions("Space Grotesk", 8.0f, juce::Font::bold));
    int col = 0;
    for (const auto& cat : categories)
    {
        const int span = (int) cat.leaves.size();
        const int x = stripAndGrid.getX() + col * cellW;
        const int w = span * cellW;
        const auto headerBounds = juce::Rectangle<int>(x, stripAndGrid.getY() - 14, w, 12);
        g.drawText(cat.label, headerBounds.toFloat(), juce::Justification::centred, false);
        col += span;
    }
}

void MatrixView::resized()
{
    auto card = getCardBounds();
    card.removeFromTop(kTitleBarHeight);

    // Left strip: modulator rows.
    auto strip = card.removeFromLeft(kStripWidth).reduced(8);
    const int n = modRows.size();
    if (n > 0)
    {
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

    // Right area: cell grid (10 modulator rows × N destination columns).
    auto gridArea = card.reduced(8);
    int totalCols = 0;
    for (const auto& cat : categories) totalCols += (int) cat.leaves.size();
    if (totalCols == 0 || cells.isEmpty()) return;

    const int cellW = gridArea.getWidth() / totalCols;
    const int cellH = gridArea.getHeight() / 10;
    int cellIdx = 0;
    for (int row = 0; row < 10; ++row)
    {
        const int y = gridArea.getY() + row * cellH;
        int colCount = 0;
        for (const auto& cat : categories)
            for (size_t i = 0; i < cat.leaves.size(); ++i)
            {
                if (cellIdx >= cells.size()) break;
                const int x = gridArea.getX() + colCount * cellW;
                cells[cellIdx]->setBounds(x, y, cellW, cellH);
                ++cellIdx;
                ++colCount;
            }
    }
}

void MatrixView::mouseDown(const juce::MouseEvent& e)
{
    // Click outside the card → dismiss.
    if (! getCardBounds().contains(e.getPosition()))
        setVisible(false);
}

} // namespace kaigen::phantom
