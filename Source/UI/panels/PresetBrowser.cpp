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
    searchField.setTextToShowWhenEmpty(
        juce::String(juce::CharPointer_UTF8("Search presets\xe2\x80\xa6")),
        juce::Colour(0x66000000));
    searchField.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x99FFFFFF));
    searchField.setColour(juce::TextEditor::outlineColourId,    juce::Colour(0x1f000000));
    searchField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0x33000000));
    searchField.setColour(juce::TextEditor::textColourId,       juce::Colour(0xb3000000));
    searchField.setColour(juce::TextEditor::highlightColourId,  juce::Colour(0x334a90e2));
    searchField.setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(0xd9000000));
    searchField.setColour(juce::TextEditor::shadowColourId,     juce::Colour(0));
    searchField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0x33000000));
    searchField.setFont(juce::FontOptions(Theme::uiFontFamily(), 13.0f, juce::Font::plain));
    searchField.setBorder(juce::BorderSize<int>(6, 12, 6, 12));
    searchField.onTextChange = [this] {
        // In Explore mode the search filters pack cards; otherwise it
        // filters preset rows. Rebuilding both is cheap and keeps state
        // consistent if the user switches modes after typing.
        rebuildPackCards();
        rebuildRows();
        repaint();
    };
    addAndMakeVisible(searchField);

    // Delete button (shown only when a User-pack preset is selected in the
    // preview pane).
    deleteButton.getProperties().set("phantom-style", "header-raised");
    deleteButton.onClick = [this] { deleteSelectedPreset(); };
    deleteButton.setVisible(false);
    addAndMakeVisible(deleteButton);
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
        hoverPackCardIdx = -1;
        selectedPreviewRow = -1;
        sidebarScrollY = 0;
        listScrollY = 0;
        deleteButton.setVisible(false);
        searchField.setText("", juce::dontSendNotification);
        return;
    }
    rebuildCategories();
    rebuildPackCards();
    rebuildRows();
    searchField.setTextToShowWhenEmpty(
        juce::String(juce::CharPointer_UTF8(
            isPacksMode() ? "Search packs\xe2\x80\xa6" : "Search presets\xe2\x80\xa6")),
        juce::Colour(0x66000000));
    repaint();
}

bool PresetBrowser::isPacksMode() const noexcept
{
    return ! categories.empty()
        && activeCategoryIdx >= 0
        && activeCategoryIdx < (int) categories.size()
        && categories[(size_t) activeCategoryIdx].kind == CategoryKind::Packs;
}

void PresetBrowser::rebuildPackCards()
{
    packCards.clear();
    const auto query = searchField.getText().trim().toLowerCase();
    const bool hasQuery = query.isNotEmpty();

    for (const auto& p : processor.getPresetManager().getAllPacks())
    {
        const auto display = p.displayName.isNotEmpty() ? p.displayName : p.name;
        if (hasQuery
            && ! display.toLowerCase().contains(query)
            && ! p.name.toLowerCase().contains(query))
            continue;
        packCards.push_back({ p.name, display, p.presetCount });
    }
    hoverPackCardIdx = -1;
}

void PresetBrowser::rebuildCategories()
{
    categories.clear();
    // Top-level entries — every preset, favorites filter, pack-card grid.
    // Glyph strings must be wrapped in CharPointer_UTF8 so JUCE decodes the
    // raw bytes as UTF-8 rather than the system codepage (CP-1252 on
    // Windows produces mojibake otherwise).
    auto u8 = [](const char* s) { return juce::String(juce::CharPointer_UTF8(s)); };
    categories.push_back({ CategoryKind::Explore,   "Explore",   u8("\xe2\x8a\x95"), {} });   // ⊕
    categories.push_back({ CategoryKind::Favorites, "Favorites", u8("\xe2\x99\xa5"), {} });   // ♥
    categories.push_back({ CategoryKind::Packs,     "Packs",     u8("\xe2\x96\xa6"), {} });   // ▦

    // Direct pack drill-ins below — same data, scoped to a single pack.
    const auto all = processor.getPresetManager().getAllPresets();
    for (const auto& [packName, presets] : all)
        categories.push_back({ CategoryKind::Pack, packName, {}, packName });

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
            r.designer    = p.metadata.designer;
            r.description = p.metadata.description;
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
    // Full-screen browser — the webview did this so text/columns/spectra get
    // maximum resolution. The compact dropdown handles the quick-pick case.
    return getLocalBounds();
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
    // Full-bleed silver background — fills the editor.
    const auto card  = cardBounds();
    const auto cardF = card.toFloat();
    g.setGradientFill(cardGradient(cardF));
    g.fillRect(card);

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
    g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::bold));
    g.setColour(juce::Colour(kTextLabel));
    g.drawText("CATEGORIES",
               sidebarBounds.reduced(14, 12).removeFromTop(14),
               juce::Justification::topLeft, false);

    // Category rows — clipped to the sidebar area below the header label so
    // scroll-out-of-view rows don't bleed into the rest of the chrome.
    {
    juce::Graphics::ScopedSaveState saveSidebar(g);
    g.reduceClipRegion(sidebarBounds.withTrimmedTop(28));
    for (size_t i = 0; i < categories.size(); ++i)
    {
        const auto& c = categories[i];
        const auto rowBounds = sidebarRowBounds((int) i).translated(0, -sidebarScrollY);
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

        // Two-column layout inside the row: [glyph] [label].
        constexpr int kGlyphW = 26;
        const auto glyphCol = juce::Rectangle<int>(rowBounds.getX() + 8, rowBounds.getY(),
                                                    kGlyphW, rowBounds.getHeight());
        const auto labelCol = juce::Rectangle<int>(glyphCol.getRight() + 4, rowBounds.getY(),
                                                    rowBounds.getRight() - glyphCol.getRight() - 8,
                                                    rowBounds.getHeight());

        if (c.glyph.isNotEmpty())
        {
            g.setColour(juce::Colour(isActive ? kTextStrong : kTextBody));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 16.0f, juce::Font::plain));
            g.drawText(c.glyph, glyphCol, juce::Justification::centred, false);
        }

        g.setColour(juce::Colour(isActive ? kTextStrong : kTextBody));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 14.0f,
                                     isActive ? juce::Font::bold : juce::Font::plain));
        g.drawText(c.label, labelCol, juce::Justification::centredLeft, false);
    }
    }   // end sidebar clip

    // ── Preview pane (right) ────────────────────────────────────────────
    g.setGradientFill(sidebarGradient(previewBounds.toFloat()));
    g.fillRect(previewBounds);
    g.setColour(juce::Colour(kBorderSoft));
    g.drawLine((float) previewBounds.getX(), (float) previewBounds.getY(),
                (float) previewBounds.getX(), (float) previewBounds.getBottom(), 1.0f);

    g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::bold));
    g.setColour(juce::Colour(kTextLabel));
    g.drawText("PREVIEW",
               previewBounds.reduced(14, 12).removeFromTop(14),
               juce::Justification::topLeft, false);

    {
        auto previewBox = previewBounds.reduced(10, 0)
                                        .withTrimmedTop(28)
                                        .withTrimmedBottom(10);
        g.setColour(juce::Colour(0x4dFFFFFF));
        g.fillRoundedRectangle(previewBox.toFloat(), 3.0f);
        g.setColour(juce::Colour(kBorderSoft));
        g.drawRoundedRectangle(previewBox.toFloat().reduced(0.5f), 3.0f, 1.0f);

        const bool hasSelection = selectedPreviewRow >= 0
                                && selectedPreviewRow < (int) rows.size()
                                && ! rows[(size_t) selectedPreviewRow].isHeader;

        if (! hasSelection)
        {
            g.setColour(juce::Colour(kTextDim));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 13.0f, juce::Font::plain));
            g.drawText("Select a preset", previewBox.reduced(14),
                        juce::Justification::centredTop, true);
        }
        else
        {
            const auto& r = rows[(size_t) selectedPreviewRow];
            auto inner = previewBox.reduced(14, 14);

            // Spectrum graph at the top — Preview variant (larger to match the
            // bigger pane). Width grows with the preview pane.
            auto specBounds = inner.removeFromTop(72);
            Theme::paintPresetSpectrum(g, specBounds.toFloat(),
                                        r.h, r.crossover, r.skip,
                                        Theme::PresetSpectrumVariant::Preview);
            inner.removeFromTop(12);

            // Name (bold, large).
            g.setColour(juce::Colour(kTextStrong));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 16.0f, juce::Font::bold));
            g.drawText(r.name, inner.removeFromTop(22),
                       juce::Justification::topLeft, true);
            inner.removeFromTop(8);

            // Metadata key/value rows — bumped to 12 px for legibility.
            auto drawKV = [&](const juce::String& label, const juce::String& value) {
                if (value.isEmpty()) return;
                auto row = inner.removeFromTop(18);
                g.setColour(juce::Colour(kTextLabel));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 12.0f, juce::Font::plain));
                const auto labelW = (int) g.getCurrentFont().getStringWidthFloat(label + " ") + 2;
                g.drawText(label, row.removeFromLeft(labelW),
                           juce::Justification::topLeft, false);
                g.setColour(juce::Colour(kTextBody));
                g.drawText(value, row, juce::Justification::topLeft, true);
                inner.removeFromTop(3);
            };
            drawKV("Type:",     r.type);
            drawKV("Designer:", r.designer);
            drawKV("Pack:",     r.pack);

            if (r.description.isNotEmpty())
            {
                inner.removeFromTop(10);
                g.setColour(juce::Colour(kBorderSoft));
                g.drawLine((float) inner.getX(), (float) inner.getY(),
                            (float) inner.getRight(), (float) inner.getY(), 1.0f);
                inner.removeFromTop(10);
                g.setColour(juce::Colour(kTextBody));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 12.0f, juce::Font::plain));
                g.drawFittedText(r.description, inner, juce::Justification::topLeft, 8);
            }
            // The deleteButton positions itself in mouseMove (visible only
            // for User-pack rows); resized() also positions it during layout
            // changes.
        }
    }

    // ── Middle: header + list ───────────────────────────────────────────
    auto headerBar = middleBounds.removeFromTop(kHeaderBarH);
    auto searchBar = middleBounds.removeFromTop(kSearchBarH);
    auto listArea  = middleBounds;

    // Header — title (active category) + total count.
    {
        int totalPresets = 0;
        if (isPacksMode())
            for (const auto& pc : packCards) totalPresets += pc.presetCount;
        else
            totalPresets = (int) std::count_if(rows.begin(), rows.end(),
                                    [](const Row& r) { return ! r.isHeader; });
        const auto title = (! categories.empty() && activeCategoryIdx >= 0
                            && activeCategoryIdx < (int) categories.size())
                            ? categories[(size_t) activeCategoryIdx].label
                            : juce::String("All Presets");
        auto h = headerBar.reduced(18, 0);
        g.setColour(juce::Colour(kTextStrong));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 24.0f, juce::Font::bold));
        g.drawText(title, h.toFloat(), juce::Justification::centredLeft, false);

        const auto countText = juce::String(totalPresets) + " preset" + (totalPresets == 1 ? "" : "s");
        g.setColour(juce::Colour(kTextLabel));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 13.0f, juce::Font::plain));
        g.drawText(countText, h.withTrimmedRight(48).toFloat(),
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

    // Explore mode: pack-card grid replaces the table. Clipped to the
    // middle column area + offset by listScrollY for vertical scrolling.
    if (isPacksMode())
    {
        juce::Graphics::ScopedSaveState saveExplore(g);
        g.reduceClipRegion(listArea);
        for (size_t i = 0; i < packCards.size(); ++i)
        {
            const auto& pc = packCards[i];
            auto card = packCardBounds((int) i).translated(0, -listScrollY);
            if (card.isEmpty()) continue;
            if (card.getBottom() <= listArea.getY() || card.getY() >= listArea.getBottom())
                continue;
            const auto cardOuter = card;   // unmodified copy for hover/border

            // Initial-letter art square.
            auto art = card.removeFromTop(card.getWidth());

            // Deterministic warm/cool gradient seeded by the pack-name hash.
            const auto seed = (juce::uint32) pc.name.hashCode();
            const float hue = (float) (seed % 360) / 360.0f;
            const auto colA = juce::Colour::fromHSV(hue,         0.55f, 0.80f, 1.0f);
            const auto colB = juce::Colour::fromHSV(std::fmod(hue + 0.12f, 1.0f),
                                                     0.45f, 0.55f, 1.0f);
            juce::ColourGradient grad(colA, art.getX(), art.getY(),
                                       colB, art.getRight(), art.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRect(art);

            // First letter of the display name, large and lightweight.
            g.setColour(juce::Colour(0xd9FFFFFF));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(),
                                         (float) art.getHeight() * 0.55f,
                                         juce::Font::plain));
            g.drawText(pc.displayName.substring(0, 1).toUpperCase(),
                       art, juce::Justification::centred, false);

            // Card body — title + count.
            auto body = card;
            g.setColour(juce::Colour(0x4dFFFFFF));
            g.fillRect(body);
            g.setColour(juce::Colour(kBorderSoft));
            g.drawRect(cardOuter, 1);

            auto bodyInner = body.reduced(10, 8);
            g.setColour(juce::Colour(kTextStrong));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 13.0f, juce::Font::bold));
            g.drawText(pc.displayName, bodyInner.removeFromTop(16),
                       juce::Justification::topLeft, true);
            g.setColour(juce::Colour(kTextLabel));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::plain));
            g.drawText(juce::String(pc.presetCount) + " preset"
                        + (pc.presetCount == 1 ? "" : "s"),
                       bodyInner.removeFromTop(13),
                       juce::Justification::topLeft, false);

            // Hover overlay over the entire card.
            if ((int) i == hoverPackCardIdx)
            {
                g.setColour(juce::Colour(0x14000000));
                g.fillRect(cardOuter);
            }
        }
        return;   // skip the table rendering when in Explore.
    }

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
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::bold));
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

    // List rows. Clipped to the rows area + offset by listScrollY so rows
    // scrolled out of view don't bleed into adjacent chrome.
    {
        const auto rowsArea = listArea.reduced(8, 4);
        juce::Graphics::ScopedSaveState saveList(g);
        g.reduceClipRegion(rowsArea);
        int y = rowsArea.getY() - listScrollY;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& r = rows[i];
            const int h = r.isHeader ? kHeaderHeight : kRowHeight;
            // Skip rows entirely above the viewport, stop after the first
            // entirely below.
            if (y + h <= rowsArea.getY()) { y += h; continue; }
            if (y >= rowsArea.getBottom()) break;

            const auto rowBounds = juce::Rectangle<int>(rowsArea.getX(), y,
                                                         rowsArea.getWidth(), h);

            if (r.isHeader)
            {
                g.setColour(juce::Colour(kTextLabel));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 12.0f, juce::Font::bold));
                g.drawText(r.pack.toUpperCase(),
                           rowBounds.reduced(10, 0),
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

                // NAME — bold, larger for at-a-glance Arturia-style read.
                g.setColour(juce::Colour(kTextStrong));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 14.0f, juce::Font::bold));
                g.drawText(r.name, nameCol, juce::Justification::centredLeft, true);

                // TYPE / DESIGNER — body text, bumped to 12 px.
                g.setColour(juce::Colour(kTextBody));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 12.0f, juce::Font::plain));
                g.drawText(r.type,     typeCol,     juce::Justification::centredLeft, true);
                g.drawText(r.designer, designerCol, juce::Justification::centredLeft, true);

                // SHAPE — harmonic spectrum thumbnail (column wider now).
                Theme::paintPresetSpectrum(g,
                    shapeCol.reduced(2, 4).toFloat(),
                    r.h, r.crossover, r.skip,
                    Theme::PresetSpectrumVariant::Thumbnail);

                // SKIP — tabular numerics. Em-dash when skip == 0.
                g.setColour(juce::Colour(kTextBody));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 13.0f, juce::Font::plain));
                const auto skipText = r.skip > 0 ? juce::String(r.skip)
                                                  : juce::String(juce::CharPointer_UTF8("\xE2\x80\x94"));
                g.drawText(skipText, skipCol, juce::Justification::centredRight, false);

                // ♥ — bigger heart glyph for clearer at-a-glance favorite state.
                g.setColour(juce::Colour(r.isFavorite ? 0xffc74a4a : 0x33000000));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 16.0f, juce::Font::plain));
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

juce::Rectangle<int> PresetBrowser::packCardBounds(int idx) const
{
    auto card = cardBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH).reduced(16, 16);

    constexpr int kCardW = 170;
    constexpr int kArtH  = kCardW;
    constexpr int kBodyH = 48;
    constexpr int kCardH = kArtH + kBodyH;
    constexpr int kGap   = 16;

    if (listArea.getWidth() < kCardW) return {};
    const int cols = juce::jmax(1, (listArea.getWidth() + kGap) / (kCardW + kGap));
    const int row  = idx / cols;
    const int col  = idx % cols;
    const int x    = listArea.getX() + col * (kCardW + kGap);
    const int y    = listArea.getY() + row * (kCardH + kGap);
    if (y + kCardH > listArea.getBottom()) return {};
    return { x, y, kCardW, kCardH };
}

juce::Rectangle<int> PresetBrowser::previewBounds() const
{
    auto card = cardBounds();
    return card.removeFromRight(kPreviewW);
}

juce::Rectangle<int> PresetBrowser::previewDeleteButtonBounds() const
{
    // Pin to the bottom-left of the preview panel's inner card.
    auto box = previewBounds().reduced(10, 0).withTrimmedTop(28).withTrimmedBottom(10).reduced(10, 10);
    return juce::Rectangle<int>(box.getX(), box.getBottom() - 24, 80, 22);
}

int PresetBrowser::contentHeightForList() const
{
    if (isPacksMode())
    {
        if (packCards.empty()) return 0;
        // Reverse-engineer rows in the grid from packCardBounds(0..n).
        int maxBottom = 0;
        for (size_t i = 0; i < packCards.size(); ++i)
        {
            const auto b = packCardBounds((int) i);
            if (! b.isEmpty()) maxBottom = juce::jmax(maxBottom, b.getBottom());
        }
        // packCardBounds returns the on-screen rect (already offset by
        // listArea.getY()), so subtract the listArea origin to get content
        // height. Approximate by using the inner trim width.
        const auto card = cardBounds();
        auto inner = card;
        inner.removeFromLeft(kSidebarW);
        inner.removeFromRight(kPreviewW);
        const auto listOriginY = inner.withTrimmedTop(kHeaderBarH + kSearchBarH).reduced(16, 16).getY();
        return juce::jmax(0, maxBottom - listOriginY);
    }
    int total = 0;
    for (const auto& r : rows) total += (r.isHeader ? kHeaderHeight : kRowHeight);
    return total;
}

int PresetBrowser::contentHeightForSidebar() const
{
    constexpr int kSidebarRowH = 30;   // matches sidebarRowBounds
    return (int) categories.size() * kSidebarRowH;
}

void PresetBrowser::mouseWheelMove(const juce::MouseEvent& e,
                                     const juce::MouseWheelDetails& wheel)
{
    const auto card = cardBounds();
    if (! card.contains(e.getPosition())) return;

    const int dy = juce::roundToInt(wheel.deltaY * 60.0f
                                     * (wheel.isReversed ? 1.0f : -1.0f));
    if (dy == 0) return;

    // Sidebar vs. list: pick by where the cursor is.
    auto sidebar = card.withWidth(kSidebarW);
    auto inner   = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    const auto listView = inner.withTrimmedTop(kHeaderBarH + kSearchBarH
                                                + (isPacksMode() ? 0 : kColHeaderH))
                                .reduced(8, 4);

    auto clamp = [](int& scroll, int contentH, int viewH) {
        const int maxScroll = juce::jmax(0, contentH - viewH);
        scroll = juce::jlimit(0, maxScroll, scroll);
    };

    if (sidebar.contains(e.getPosition()))
    {
        sidebarScrollY += dy;
        clamp(sidebarScrollY, contentHeightForSidebar(), sidebar.getHeight() - 28);
        repaint();
        return;
    }
    if (listView.contains(e.getPosition()))
    {
        listScrollY += dy;
        clamp(listScrollY, contentHeightForList(), listView.getHeight());
        repaint();
        return;
    }
}

void PresetBrowser::deleteSelectedPreset()
{
    if (selectedPreviewRow < 0 || selectedPreviewRow >= (int) rows.size()) return;
    const auto& r = rows[(size_t) selectedPreviewRow];
    if (r.isHeader || r.pack != "User") return;

    if (! processor.getPresetManager().deletePreset(r.name, r.pack)) return;

    // Refresh — the deleted row is gone, so reset preview state.
    selectedPreviewRow = -1;
    deleteButton.setVisible(false);
    rebuildCategories();
    rebuildRows();
    repaint();
}

juce::Rectangle<int> PresetBrowser::sidebarRowBounds(int categoryIdx) const
{
    constexpr int kSidebarTopOffset = 36;   // below the "CATEGORIES" label
    constexpr int kSidebarRowH      = 30;
    const auto card = cardBounds();
    auto sidebar = card.withWidth(kSidebarW).reduced(8, 0);
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
    if (deleteButton.isVisible())
        deleteButton.setBounds(previewDeleteButtonBounds());
}

void PresetBrowser::mouseMove(const juce::MouseEvent& e)
{
    const auto card = cardBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + kColHeaderH).reduced(8, 4);

    int newRowHover = -1;
    int y = listArea.getY() - listScrollY;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (y + h <= listArea.getY()) { y += h; continue; }
        if (y >= listArea.getBottom()) break;
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
        if (sidebarRowBounds((int) i).translated(0, -sidebarScrollY)
                .contains(e.getPosition()))
        {
            newCatHover = (int) i;
            break;
        }
    }

    int newPackHover = -1;
    if (isPacksMode())
    {
        for (size_t i = 0; i < packCards.size(); ++i)
        {
            if (packCardBounds((int) i).translated(0, -listScrollY)
                    .contains(e.getPosition()))
            {
                newPackHover = (int) i;
                break;
            }
        }
    }

    if (newRowHover != hoverRow || newCatHover != hoverCategoryIdx
        || newPackHover != hoverPackCardIdx)
    {
        hoverPackCardIdx = newPackHover;
        hoverRow = newRowHover;
        hoverCategoryIdx = newCatHover;
        if (newRowHover >= 0)
        {
            // Hovering a preset row updates the preview pane. We keep the
            // last hovered row "sticky" so the preview doesn't blank out
            // when the cursor leaves the list area.
            selectedPreviewRow = newRowHover;
            const auto& r = rows[(size_t) newRowHover];
            const bool isUserPreset = (r.pack == "User");
            deleteButton.setVisible(isUserPreset);
            if (isUserPreset)
                deleteButton.setBounds(previewDeleteButtonBounds());
        }
        repaint();
    }
}

void PresetBrowser::mouseExit(const juce::MouseEvent&)
{
    if (hoverRow != -1 || hoverCategoryIdx != -1 || hoverPackCardIdx != -1)
    {
        hoverRow = -1;
        hoverCategoryIdx = -1;
        hoverPackCardIdx = -1;
        repaint();
    }
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e)
{
    // Browser fills the editor; the only way to dismiss is the ✕ button or
    // the back-to-webview shift+click on the PHANTOM logo (TopBar).
    juce::ignoreUnused(e);

    // Sidebar category click → switch active category and refilter.
    for (size_t i = 0; i < categories.size(); ++i)
    {
        if (sidebarRowBounds((int) i).translated(0, -sidebarScrollY)
                .contains(e.getPosition()))
        {
            if ((int) i != activeCategoryIdx)
            {
                activeCategoryIdx = (int) i;
                listScrollY = 0;
                rebuildRows();
                searchField.setTextToShowWhenEmpty(
                    juce::String(juce::CharPointer_UTF8(
                        isPacksMode() ? "Search packs\xe2\x80\xa6"
                                       : "Search presets\xe2\x80\xa6")),
                    juce::Colour(0x66000000));
                repaint();
            }
            return;
        }
    }

    // Explore mode: clicking a pack card → switch sidebar to that pack's
    // category and rebuild as a list view.
    if (isPacksMode())
    {
        for (size_t i = 0; i < packCards.size(); ++i)
        {
            if (packCardBounds((int) i).translated(0, -listScrollY)
                    .contains(e.getPosition()))
            {
                const auto& pc = packCards[i];
                for (size_t ci = 0; ci < categories.size(); ++ci)
                {
                    if (categories[ci].kind == CategoryKind::Pack
                        && categories[ci].packFilter == pc.name)
                    {
                        activeCategoryIdx = (int) ci;
                        rebuildRows();
                        repaint();
                        break;
                    }
                }
                return;
            }
        }
    }

    // List area: hit test the preset rows (with scroll offset).
    auto inner = cardBounds();
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    const auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + kColHeaderH).reduced(8, 4);
    if (! listArea.contains(e.getPosition())) return;

    int y = listArea.getY() - listScrollY;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (y + h <= listArea.getY()) { y += h; continue; }
        if (y >= listArea.getBottom()) break;
        if (e.y >= y && e.y < y + h)
        {
            loadPresetAt((int) i);
            return;
        }
        y += h;
    }
}

} // namespace kaigen::phantom
