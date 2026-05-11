// Source/UI/panels/PresetBrowser.cpp
#include "PresetBrowser.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../PresetManager.h"

namespace kaigen::phantom
{

namespace
{
    // Webview palette — silver gradient card with dark-on-light text.
    inline juce::ColourGradient cardGradient(juce::Rectangle<float> b)
    {
        // CSS: linear-gradient(135deg, #BBBDBF 0%, #AEAFB1 100%)
        return { juce::Colour(0xffBBBDBF), b.getX(),     b.getY(),
                 juce::Colour(0xffAEAFB1), b.getRight(), b.getBottom(), false };
    }

    inline juce::ColourGradient sidebarGradient(juce::Rectangle<float> b)
    {
        // Slightly brighter than the card so the sidebar reads as inset.
        return { juce::Colour(0xffC5C7C9), b.getX(),     b.getY(),
                 juce::Colour(0xffBBBDBF), b.getRight(), b.getBottom(), false };
    }

    constexpr juce::uint32 kTextStrong  = 0xd9000000;   // ~85% — title, preset name
    constexpr juce::uint32 kTextBody    = 0xb3000000;   // ~70% — metadata values
    constexpr juce::uint32 kTextLabel   = 0x80000000;   // ~50% — labels, count
    constexpr juce::uint32 kTextDim     = 0x66000000;   // ~40% — placeholders
    constexpr juce::uint32 kRowHover    = 0x0f000000;   // ~6% black
    constexpr juce::uint32 kRowActive   = 0x1a000000;   // ~10% black
    constexpr juce::uint32 kBorderSoft  = 0x1f000000;   // ~12% black
}

PresetBrowser::PresetBrowser(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    closeButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x95"))); // ✕
    closeButton.getProperties().set("phantom-style", "header-glyph");
    closeButton.onClick = [this] { setVisible(false); };
    addAndMakeVisible(closeButton);

    // Search field — live filter. Light "glass" surface (matches the
    // placeholder paint that was here in commit 1).
    searchField.setTextToShowWhenEmpty("Search presets…", juce::Colour(0x66000000));
    searchField.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x99FFFFFF));
    searchField.setColour(juce::TextEditor::outlineColourId,    juce::Colour(0x1f000000));
    searchField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0x33000000));
    searchField.setColour(juce::TextEditor::textColourId,       juce::Colour(0xb3000000));
    searchField.setColour(juce::TextEditor::highlightColourId,  juce::Colour(0x334a90e2));
    searchField.setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(0xd9000000));
    searchField.setColour(juce::TextEditor::shadowColourId,     juce::Colour(0));
    searchField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0x33000000));
    searchField.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
    searchField.setBorder(juce::BorderSize<int>(4, 8, 4, 8));
    searchField.onTextChange = [this] { rebuildRows(); repaint(); };
    addAndMakeVisible(searchField);
}

PresetBrowser::~PresetBrowser() = default;

void PresetBrowser::visibilityChanged()
{
    if (! isVisible())
    {
        rows.clear();
        categories.clear();
        hoverRow = -1;
        hoverCategoryIdx = -1;
        searchField.setText("", juce::dontSendNotification);
        return;
    }
    rebuildCategories();
    rebuildRows();
    repaint();
}

void PresetBrowser::rebuildCategories()
{
    categories.clear();
    categories.push_back({ CategoryKind::Explore,   "Explore",   {} });
    categories.push_back({ CategoryKind::Favorites, "Favorites", {} });

    const auto all = processor.getPresetManager().getAllPresets();
    for (const auto& [packName, presets] : all)
        categories.push_back({ CategoryKind::Pack, packName, packName });

    if (activeCategoryIdx >= (int) categories.size())
        activeCategoryIdx = 0;
}

void PresetBrowser::rebuildRows()
{
    rows.clear();
    hoverRow = -1;
    const auto query = searchField.getText().trim().toLowerCase();
    const bool hasQuery = query.isNotEmpty();

    if (categories.empty()) return;
    const auto& active = categories[(size_t) activeCategoryIdx];

    const auto all = processor.getPresetManager().getAllPresets();
    for (const auto& [packName, presets] : all)
    {
        // Skip whole packs that don't match the active category filter.
        if (active.kind == CategoryKind::Pack && active.packFilter != packName)
            continue;

        std::vector<Row> matched;
        for (const auto& p : presets)
        {
            if (active.kind == CategoryKind::Favorites && ! p.metadata.isFavorite)
                continue;
            if (hasQuery && ! p.metadata.name.toLowerCase().contains(query))
                continue;
            Row r;
            r.name       = p.metadata.name;
            r.pack       = packName;
            r.type       = p.metadata.type;
            r.designer   = p.metadata.designer;
            for (int hi = 0; hi < 7; ++hi) r.h[hi] = p.preview.h[hi];
            r.crossover  = p.preview.crossover;
            r.skip       = p.preview.skip;
            r.isFavorite = p.metadata.isFavorite;
            r.isHeader   = false;
            matched.push_back(std::move(r));
        }
        if (matched.empty()) continue;

        Row header;
        header.pack     = packName;
        header.isHeader = true;
        rows.push_back(std::move(header));
        for (auto& m : matched)
            rows.push_back(std::move(m));
    }
}

juce::Rectangle<int> PresetBrowser::cardBounds() const
{
    const auto bounds = getLocalBounds();
    // 90% × 90% of the editor, capped so the card doesn't get unwieldy.
    const int w = juce::jmin((int) (bounds.getWidth()  * 0.90f), 920);
    const int h = juce::jmin((int) (bounds.getHeight() * 0.90f), 640);
    return { (bounds.getWidth()  - w) / 2,
             (bounds.getHeight() - h) / 2,
             w, h };
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
    // Backdrop scrim — CSS: rgba(0,0,0,0.4).
    g.fillAll(juce::Colour(0x66000000));

    const auto card = cardBounds();
    const auto cardF = card.toFloat();
    constexpr float corner = 8.0f;

    // Card body — silver gradient (CSS 135deg #BBBDBF → #AEAFB1).
    g.setGradientFill(cardGradient(cardF));
    g.fillRoundedRectangle(cardF, corner);

    // 1px outline (CSS rgba(0,0,0,0.15)).
    g.setColour(juce::Colour(0x26000000));
    g.drawRoundedRectangle(cardF.reduced(0.5f), corner, 1.0f);

    // Inset top highlight for the lifted-card feel.
    g.setColour(juce::Colour(0x99FFFFFF));
    g.drawLine(cardF.getX() + corner, cardF.getY() + 1.0f,
                cardF.getRight() - corner, cardF.getY() + 1.0f, 1.0f);

    // Layout slots inside the card.
    auto inner = card;
    auto sidebarBounds = inner.removeFromLeft(kSidebarW);
    auto previewBounds = inner.removeFromRight(kPreviewW);
    auto middleBounds  = inner;

    // ── Sidebar ─────────────────────────────────────────────────────────
    g.setGradientFill(sidebarGradient(sidebarBounds.toFloat()));
    g.fillRect(sidebarBounds);
    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) sidebarBounds.getRight(), (float) sidebarBounds.getY(),
                (float) sidebarBounds.getRight(), (float) sidebarBounds.getBottom(), 1.0f);

    // Sidebar label.
    g.setFont(juce::FontOptions(Theme::uiFontFamily(), 9.0f, juce::Font::bold));
    g.setColour(juce::Colour(kTextLabel));
    g.drawText("CATEGORIES",
               sidebarBounds.reduced(10, 8).removeFromTop(12),
               juce::Justification::topLeft, false);

    // Category rows.
    for (size_t i = 0; i < categories.size(); ++i)
    {
        const auto& c = categories[i];
        const auto rowBounds = sidebarRowBounds((int) i);
        const bool isActive = ((int) i == activeCategoryIdx);
        const bool isHover  = ((int) i == hoverCategoryIdx);

        if (isActive || isHover)
        {
            g.setColour(juce::Colour(isActive ? kRowActive : kRowHover));
            g.fillRoundedRectangle(rowBounds.toFloat(), 3.0f);
        }
        if (isActive)
        {
            g.setColour(juce::Colour(0x4d000000));
            g.fillRect(juce::Rectangle<float>((float) rowBounds.getX(), (float) rowBounds.getY(),
                                               3.0f, (float) rowBounds.getHeight()));
        }
        g.setColour(juce::Colour(isActive ? kTextStrong : kTextBody));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f,
                                     isActive ? juce::Font::bold : juce::Font::plain));
        g.drawText(c.label,
                   rowBounds.reduced(10, 0),
                   juce::Justification::centredLeft, false);
    }

    // ── Preview pane (right) ────────────────────────────────────────────
    g.setGradientFill(sidebarGradient(previewBounds.toFloat()));
    g.fillRect(previewBounds);
    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) previewBounds.getX(), (float) previewBounds.getY(),
                (float) previewBounds.getX(), (float) previewBounds.getBottom(), 1.0f);

    g.setFont(juce::FontOptions(Theme::uiFontFamily(), 9.0f, juce::Font::bold));
    g.setColour(juce::Colour(kTextLabel));
    g.drawText("PREVIEW",
               previewBounds.reduced(12, 10).removeFromTop(12),
               juce::Justification::topLeft, false);

    {
        auto previewBox = previewBounds.reduced(10, 0)
                                        .withTrimmedTop(28)
                                        .withTrimmedBottom(10);
        g.setColour(juce::Colour(0x4dFFFFFF));
        g.fillRoundedRectangle(previewBox.toFloat(), 3.0f);
        g.setColour(juce::Colour(kBorderSoft));
        g.drawRoundedRectangle(previewBox.toFloat().reduced(0.5f), 3.0f, 1.0f);

        g.setColour(juce::Colour(kTextDim));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
        g.drawText("Select a preset", previewBox.reduced(12),
                    juce::Justification::centredTop, true);
    }

    // ── Middle: header + list ───────────────────────────────────────────
    auto headerBar = middleBounds.removeFromTop(kHeaderBarH);
    auto searchBar = middleBounds.removeFromTop(kSearchBarH);
    auto listArea  = middleBounds;

    // Header — title (active category) + total count.
    {
        const auto totalPresets = (int) std::count_if(rows.begin(), rows.end(),
                                    [](const Row& r) { return ! r.isHeader; });
        const auto title = (! categories.empty() && activeCategoryIdx >= 0
                            && activeCategoryIdx < (int) categories.size())
                            ? categories[(size_t) activeCategoryIdx].label
                            : juce::String("All Presets");
        auto h = headerBar.reduced(14, 0);
        g.setColour(juce::Colour(kTextStrong));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 18.0f, juce::Font::bold));
        g.drawText(title, h.toFloat(), juce::Justification::centredLeft, false);

        const auto countText = juce::String(totalPresets) + " preset" + (totalPresets == 1 ? "" : "s");
        g.setColour(juce::Colour(kTextLabel));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
        g.drawText(countText, h.withTrimmedRight(36).toFloat(),
                    juce::Justification::centredRight, false);
    }

    // Header bottom border.
    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) headerBar.getX(), (float) headerBar.getBottom(),
                (float) headerBar.getRight(), (float) headerBar.getBottom(), 1.0f);

    // Search bar — separator below; the input itself is a real
    // juce::TextEditor (positioned in resized()) painted on top.
    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) searchBar.getX(), (float) searchBar.getBottom(),
                (float) searchBar.getRight(), (float) searchBar.getBottom(), 1.0f);

    // Column header bar — labels above the list (NAME | TYPE | DESIGNER |
    // SHAPE | SKIP | ♥). Sortable indicators arrive in commit 10.
    auto colHeaderBar = listArea.removeFromTop(kColHeaderH);
    {
        const auto cells = listArea.getX() + 12;   // matches row x padding
        const auto right = listArea.getRight() - 12;
        int x = right;

        auto col = [&](int w) {
            const juce::Rectangle<int> r { x - w, colHeaderBar.getY(), w, colHeaderBar.getHeight() };
            x = r.getX() - kColGap;
            return r;
        };

        const auto heartCol    = col(kColHeartW);
        const auto skipCol     = col(kColSkipW);
        const auto shapeCol    = col(kColShapeW);
        const auto designerCol = col(kColDesignerW);
        const auto typeCol     = col(kColTypeW);
        const juce::Rectangle<int> nameCol { cells, colHeaderBar.getY(),
                                              x - cells, colHeaderBar.getHeight() };

        g.setColour(juce::Colour(kTextLabel));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 9.0f, juce::Font::bold));
        g.drawText("NAME",     nameCol,     juce::Justification::centredLeft, false);
        g.drawText("TYPE",     typeCol,     juce::Justification::centredLeft, false);
        g.drawText("DESIGNER", designerCol, juce::Justification::centredLeft, false);
        g.drawText("SHAPE",    shapeCol,    juce::Justification::centredLeft, false);
        g.drawText("SKIP",     skipCol,     juce::Justification::centredRight, false);
        juce::ignoreUnused(heartCol);

        g.setColour(juce::Colour(kBorderSoft));
        g.drawLine((float) colHeaderBar.getX(), (float) colHeaderBar.getBottom(),
                    (float) colHeaderBar.getRight(), (float) colHeaderBar.getBottom(), 1.0f);
    }

    // List rows.
    {
        const auto rowsArea = listArea.reduced(8, 4);
        int y = rowsArea.getY();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& r = rows[i];
            const int h = r.isHeader ? kHeaderHeight : kRowHeight;
            if (y + h > rowsArea.getBottom()) break;     // commit 9 adds scroll

            const auto rowBounds = juce::Rectangle<int>(rowsArea.getX(), y,
                                                         rowsArea.getWidth(), h);

            if (r.isHeader)
            {
                g.setColour(juce::Colour(kTextLabel));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 10.0f, juce::Font::bold));
                g.drawText(r.pack.toUpperCase(),
                           rowBounds.reduced(8, 0),
                           juce::Justification::centredLeft, false);
            }
            else
            {
                if ((int) i == hoverRow)
                {
                    g.setColour(juce::Colour(kRowHover));
                    g.fillRoundedRectangle(rowBounds.toFloat(), 3.0f);
                }

                // Lay columns out right-to-left so NAME flexes to fill what
                // remains. Mirrors the column-header computation above.
                int x = rowBounds.getRight() - 4;
                auto cellRight = [&](int w) {
                    const juce::Rectangle<int> r2 { x - w, rowBounds.getY(), w, rowBounds.getHeight() };
                    x = r2.getX() - kColGap;
                    return r2;
                };

                const auto heartCol    = cellRight(kColHeartW);
                const auto skipCol     = cellRight(kColSkipW);
                const auto shapeCol    = cellRight(kColShapeW);
                const auto designerCol = cellRight(kColDesignerW);
                const auto typeCol     = cellRight(kColTypeW);
                const juce::Rectangle<int> nameCol { rowBounds.getX() + 12, rowBounds.getY(),
                                                      x - (rowBounds.getX() + 12), rowBounds.getHeight() };

                // NAME — stronger weight per CSS spec.
                g.setColour(juce::Colour(kTextStrong));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::bold));
                g.drawText(r.name, nameCol, juce::Justification::centredLeft, true);

                // TYPE / DESIGNER — body text.
                g.setColour(juce::Colour(kTextBody));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
                g.drawText(r.type,     typeCol,     juce::Justification::centredLeft, true);
                g.drawText(r.designer, designerCol, juce::Justification::centredLeft, true);

                // SHAPE — harmonic spectrum thumbnail.
                Theme::paintPresetSpectrum(g,
                    shapeCol.reduced(2, 1).toFloat(),
                    r.h, r.crossover, r.skip,
                    Theme::PresetSpectrumVariant::Thumbnail);

                // SKIP — tabular numerics. Em-dash when skip == 0 (matches the
                // webview's display rule for "no skip set").
                g.setColour(juce::Colour(kTextBody));
                const auto skipText = r.skip > 0 ? juce::String(r.skip)
                                                  : juce::String(juce::CharPointer_UTF8("\xE2\x80\x94"));
                g.drawText(skipText, skipCol, juce::Justification::centredRight, false);

                // ♥ — visual placeholder. Active red if favorited (data is in
                // place; toggle UI lands in commit 11/12).
                g.setColour(juce::Colour(r.isFavorite ? 0xffc74a4a : 0x33000000));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 12.0f, juce::Font::plain));
                g.drawText(juce::String(juce::CharPointer_UTF8(r.isFavorite ? "\xE2\x99\xA5" : "\xE2\x99\xA1")),
                           heartCol, juce::Justification::centred, false);
            }
            y += h;
        }
    }
}

juce::Rectangle<int> PresetBrowser::searchBarBounds() const
{
    auto card = cardBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    inner.removeFromTop(kHeaderBarH);   // skip the title bar
    return inner.removeFromTop(kSearchBarH);
}

juce::Rectangle<int> PresetBrowser::sidebarRowBounds(int categoryIdx) const
{
    constexpr int kSidebarTopOffset = 28;   // below the "CATEGORIES" label
    constexpr int kSidebarRowH      = 24;
    const auto card = cardBounds();
    auto sidebar = card.withWidth(kSidebarW).reduced(6, 0);
    return { sidebar.getX(),
             card.getY() + kSidebarTopOffset + categoryIdx * kSidebarRowH,
             sidebar.getWidth(),
             kSidebarRowH };
}

void PresetBrowser::resized()
{
    const auto card = cardBounds();
    closeButton.setBounds(card.getRight() - 38, card.getY() + 8, 28, 24);
    searchField.setBounds(searchBarBounds().reduced(12, 6));
}

void PresetBrowser::mouseMove(const juce::MouseEvent& e)
{
    const auto card = cardBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + kColHeaderH).reduced(8, 4);

    int newRowHover = -1;
    int y = listArea.getY();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (y + h > listArea.getBottom()) break;
        if (! r.isHeader
            && e.x >= listArea.getX() && e.x < listArea.getRight()
            && e.y >= y               && e.y < y + h)
        {
            newRowHover = (int) i;
            break;
        }
        y += h;
    }

    int newCatHover = -1;
    for (size_t i = 0; i < categories.size(); ++i)
    {
        if (sidebarRowBounds((int) i).contains(e.getPosition()))
        {
            newCatHover = (int) i;
            break;
        }
    }

    if (newRowHover != hoverRow || newCatHover != hoverCategoryIdx)
    {
        hoverRow = newRowHover;
        hoverCategoryIdx = newCatHover;
        repaint();
    }
}

void PresetBrowser::mouseExit(const juce::MouseEvent&)
{
    if (hoverRow != -1 || hoverCategoryIdx != -1)
    {
        hoverRow = -1;
        hoverCategoryIdx = -1;
        repaint();
    }
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e)
{
    const auto card = cardBounds();

    // Click outside the card → dismiss.
    if (! card.contains(e.getPosition()))
    {
        setVisible(false);
        return;
    }

    // Sidebar category click → switch active category and refilter.
    for (size_t i = 0; i < categories.size(); ++i)
    {
        if (sidebarRowBounds((int) i).contains(e.getPosition()))
        {
            if ((int) i != activeCategoryIdx)
            {
                activeCategoryIdx = (int) i;
                rebuildRows();
                repaint();
            }
            return;
        }
    }

    // List area only (preview/header inert until commits 7-8).
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    const auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + kColHeaderH).reduced(8, 4);
    if (! listArea.contains(e.getPosition())) return;

    int y = listArea.getY();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (y + h > listArea.getBottom()) break;
        if (e.y >= y && e.y < y + h)
        {
            loadPresetAt((int) i);
            return;
        }
        y += h;
    }
}

} // namespace kaigen::phantom
