// Source/UI/widgets/PresetDropdown.cpp
#include "PresetDropdown.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../PresetManager.h"

namespace kaigen::phantom
{

namespace
{
    inline juce::ColourGradient cardGradient(juce::Rectangle<float> b)
    {
        return { juce::Colour(0xffBBBDBF), b.getX(),     b.getY(),
                 juce::Colour(0xffAEAFB1), b.getRight(), b.getBottom(), false };
    }
    inline juce::ColourGradient sidebarGradient(juce::Rectangle<float> b)
    {
        return { juce::Colour(0xffC5C7C9), b.getX(),     b.getY(),
                 juce::Colour(0xffBBBDBF), b.getRight(), b.getBottom(), false };
    }

    constexpr juce::uint32 kTextStrong  = 0xd9000000;
    constexpr juce::uint32 kTextBody    = 0xb3000000;
    constexpr juce::uint32 kTextLabel   = 0x80000000;
    constexpr juce::uint32 kTextDim     = 0x66000000;
    constexpr juce::uint32 kRowHover    = 0x0f000000;
    constexpr juce::uint32 kRowActive   = 0x1a000000;
    constexpr juce::uint32 kBorderSoft  = 0x1f000000;
}

PresetDropdown::PresetDropdown(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    // The component itself is the full-editor overlay; only the cardBounds
    // sub-rect actually paints. Backdrop must be transparent so the rest
    // of the editor stays visible behind the dropdown.
    setOpaque(false);
    setVisible(false);
}

PresetDropdown::~PresetDropdown() = default;

void PresetDropdown::anchorBelow(juce::Rectangle<int> anchorInEditor)
{
    const int x = juce::jmax(8,
                  juce::jmin(getWidth() - kCardW - 8,
                             anchorInEditor.getCentreX() - kCardW / 2));
    int y = anchorInEditor.getBottom() + kAnchorGap;
    if (y + kCardH > getHeight() - 8)
        y = juce::jmax(8, anchorInEditor.getY() - kAnchorGap - kCardH);

    cardBounds = { x, y, kCardW, kCardH };
    repaint();
}

void PresetDropdown::visibilityChanged()
{
    if (! isVisible())
    {
        hoverPresetIdx = -1;
        hoverCategoryIdx = -1;
        return;
    }
    rebuildList();
    applyCategoryFilter();
    repaint();
}

void PresetDropdown::rebuildList()
{
    allPresets.clear();
    categories.clear();

    // "All" is always first.
    Category all;
    all.label = "All";
    categories.push_back(all);

    const auto packs = processor.getPresetManager().getAllPresets();
    for (const auto& [packName, presets] : packs)
    {
        Category c;
        c.label      = packName;
        c.packFilter = packName;
        c.count      = (int) presets.size();
        categories.push_back(c);

        for (const auto& p : presets)
            allPresets.push_back({ p.metadata.name, packName });
    }
    categories[0].count = (int) allPresets.size();

    // Clamp the active index in case the list shrank since last open.
    if (activeCategoryIdx >= (int) categories.size())
        activeCategoryIdx = 0;
}

void PresetDropdown::applyCategoryFilter()
{
    filteredPresets.clear();
    if (activeCategoryIdx < 0 || activeCategoryIdx >= (int) categories.size()) return;

    const auto& c = categories[(size_t) activeCategoryIdx];
    if (c.packFilter.isEmpty())
    {
        filteredPresets = allPresets;
    }
    else
    {
        for (const auto& p : allPresets)
            if (p.pack == c.packFilter)
                filteredPresets.push_back(p);
    }
}

void PresetDropdown::loadPresetAt(int filteredIdx)
{
    if (filteredIdx < 0 || filteredIdx >= (int) filteredPresets.size()) return;
    const auto& p = filteredPresets[(size_t) filteredIdx];
    if (processor.getPresetManager().loadPreset(apvts, p.name, p.pack))
    {
        if (onPresetSelected) onPresetSelected(p.name, p.pack);
        setVisible(false);
    }
}

void PresetDropdown::paint(juce::Graphics& g)
{
    if (cardBounds.isEmpty()) return;
    const auto cardF = cardBounds.toFloat();
    constexpr float corner = 4.0f;

    // Drop shadow under the card.
    {
        juce::DropShadow ds(juce::Colour(0x40000000), 12, { 0, 4 });
        juce::Path p; p.addRoundedRectangle(cardF, corner);
        ds.drawForPath(g, p);
    }

    // Card body.
    g.setGradientFill(cardGradient(cardF));
    g.fillRoundedRectangle(cardF, corner);
    g.setColour(juce::Colour(0x26000000));
    g.drawRoundedRectangle(cardF.reduced(0.5f), corner, 1.0f);

    // ── Sidebar ─────────────────────────────────────────────────────────
    auto sidebar = cardBounds.withWidth(kSidebarW);
    g.setGradientFill(sidebarGradient(sidebar.toFloat()));
    g.fillRoundedRectangle(sidebar.toFloat(), corner);
    // Paint over the right two corners so they don't round (sidebar is
    // adjacent to the list — should be a sharp inner edge there).
    g.fillRect(juce::Rectangle<float>((float) (sidebar.getRight() - corner),
                                       (float) sidebar.getY(),
                                       (float) corner,
                                       (float) sidebar.getHeight()));

    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) sidebar.getRight(), (float) sidebar.getY(),
                (float) sidebar.getRight(), (float) sidebar.getBottom(), 1.0f);

    // Category rows.
    {
        auto y = sidebar.getY() + 4;
        for (size_t i = 0; i < categories.size(); ++i)
        {
            const auto& c = categories[i];
            const bool isActive = ((int) i == activeCategoryIdx);
            const bool isHover  = ((int) i == hoverCategoryIdx);

            const auto rowBounds = juce::Rectangle<int>(sidebar.getX() + 4, y,
                                                         sidebar.getWidth() - 8, kCatRowH);

            if (isActive || isHover)
            {
                g.setColour(juce::Colour(isActive ? kRowActive : kRowHover));
                g.fillRoundedRectangle(rowBounds.toFloat(), 3.0f);
            }

            // Active checkmark.
            if (isActive)
            {
                g.setColour(juce::Colour(0xff4a7a4a));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 10.0f, juce::Font::bold));
                g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x93")),
                           rowBounds.withWidth(14).translated(4, 0),
                           juce::Justification::centred, false);
            }

            g.setColour(juce::Colour(isActive ? kTextStrong : kTextBody));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f,
                                         isActive ? juce::Font::bold : juce::Font::plain));
            g.drawText(c.label,
                       rowBounds.withTrimmedLeft(20),
                       juce::Justification::centredLeft, false);

            if (c.count > 0)
            {
                g.setColour(juce::Colour(kTextDim));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 10.0f, juce::Font::plain));
                g.drawText("(" + juce::String(c.count) + ")",
                           rowBounds.withTrimmedRight(8),
                           juce::Justification::centredRight, false);
            }
            y += kCatRowH;
            if (y + kCatRowH > sidebar.getBottom()) break;
        }
    }

    // ── Preset list ─────────────────────────────────────────────────────
    auto list = cardBounds.withTrimmedLeft(kSidebarW);
    if (filteredPresets.empty())
    {
        g.setColour(juce::Colour(kTextDim));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
        g.drawText("No presets in this category",
                   list.reduced(12),
                   juce::Justification::centredTop, true);
        return;
    }

    {
        auto y = list.getY() + 4;
        for (size_t i = 0; i < filteredPresets.size(); ++i)
        {
            const auto rowBounds = juce::Rectangle<int>(list.getX() + 4, y,
                                                         list.getWidth() - 8, kRowH);

            if ((int) i == hoverPresetIdx)
            {
                g.setColour(juce::Colour(kRowHover));
                g.fillRoundedRectangle(rowBounds.toFloat(), 3.0f);
            }
            g.setColour(juce::Colour(kTextStrong));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
            g.drawText(filteredPresets[i].name,
                       rowBounds.reduced(10, 0),
                       juce::Justification::centredLeft, false);

            y += kRowH;
            if (y + kRowH > list.getBottom()) break;     // commit 7 adds scroll
        }
    }
}

void PresetDropdown::resized()
{
    // Component fills the editor; cardBounds is set by anchorBelow().
}

void PresetDropdown::mouseMove(const juce::MouseEvent& e)
{
    int newCat = -1;
    int newPreset = -1;

    if (cardBounds.contains(e.getPosition()))
    {
        // Sidebar?
        const auto sidebar = cardBounds.withWidth(kSidebarW);
        if (sidebar.contains(e.getPosition()))
        {
            int y = sidebar.getY() + 4;
            for (size_t i = 0; i < categories.size(); ++i)
            {
                if (e.y >= y && e.y < y + kCatRowH) { newCat = (int) i; break; }
                y += kCatRowH;
            }
        }
        else
        {
            const auto list = cardBounds.withTrimmedLeft(kSidebarW);
            int y = list.getY() + 4;
            for (size_t i = 0; i < filteredPresets.size(); ++i)
            {
                if (e.y >= y && e.y < y + kRowH) { newPreset = (int) i; break; }
                y += kRowH;
            }
        }
    }

    if (newCat != hoverCategoryIdx || newPreset != hoverPresetIdx)
    {
        hoverCategoryIdx = newCat;
        hoverPresetIdx   = newPreset;
        repaint();
    }
}

void PresetDropdown::mouseExit(const juce::MouseEvent&)
{
    if (hoverCategoryIdx != -1 || hoverPresetIdx != -1)
    {
        hoverCategoryIdx = -1;
        hoverPresetIdx   = -1;
        repaint();
    }
}

void PresetDropdown::mouseDown(const juce::MouseEvent& e)
{
    if (! cardBounds.contains(e.getPosition()))
    {
        setVisible(false);
        return;
    }

    const auto sidebar = cardBounds.withWidth(kSidebarW);
    if (sidebar.contains(e.getPosition()))
    {
        int y = sidebar.getY() + 4;
        for (size_t i = 0; i < categories.size(); ++i)
        {
            if (e.y >= y && e.y < y + kCatRowH)
            {
                if ((int) i != activeCategoryIdx)
                {
                    activeCategoryIdx = (int) i;
                    applyCategoryFilter();
                    hoverPresetIdx = -1;
                    repaint();
                }
                return;
            }
            y += kCatRowH;
        }
        return;
    }

    const auto list = cardBounds.withTrimmedLeft(kSidebarW);
    int y = list.getY() + 4;
    for (size_t i = 0; i < filteredPresets.size(); ++i)
    {
        if (e.y >= y && e.y < y + kRowH)
        {
            loadPresetAt((int) i);
            return;
        }
        y += kRowH;
    }
}

} // namespace kaigen::phantom
