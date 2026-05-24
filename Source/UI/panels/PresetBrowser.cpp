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

    // Line-art icons drawn as JUCE Paths. ~20×20 logical units, stroked
    // at 1.5 px with rounded joins. Inspired by Arturia Analog Lab's
    // sidebar (magnifying glass / heart / stacked-layers).
    enum class SidebarIcon { Explore, Favorites, Packs, User };
    void paintSidebarIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                          SidebarIcon icon, juce::Colour stroke)
    {
        if (bounds.isEmpty()) return;
        const auto inset = bounds.reduced(bounds.getWidth() * 0.18f,
                                           bounds.getHeight() * 0.18f);
        const float cx = inset.getCentreX();
        const float cy = inset.getCentreY();
        const float r  = juce::jmin(inset.getWidth(), inset.getHeight()) * 0.5f;

        juce::Path p;
        const juce::PathStrokeType strokeT(1.5f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded);

        switch (icon)
        {
            case SidebarIcon::Explore:
            {
                // Magnifying glass — circle in upper-left, diagonal handle.
                const float circR  = r * 0.62f;
                const float ccx    = cx - r * 0.18f;
                const float ccy    = cy - r * 0.18f;
                p.addEllipse(ccx - circR, ccy - circR, circR * 2.0f, circR * 2.0f);

                // Handle from the circle's bottom-right tangent outward.
                const float a = juce::MathConstants<float>::pi * 0.25f;
                const float hx0 = ccx + std::cos(a) * circR;
                const float hy0 = ccy + std::sin(a) * circR;
                const float hx1 = ccx + std::cos(a) * (circR + r * 0.55f);
                const float hy1 = ccy + std::sin(a) * (circR + r * 0.55f);
                p.startNewSubPath(hx0, hy0);
                p.lineTo(hx1, hy1);
                break;
            }
            case SidebarIcon::Favorites:
            {
                // Heart — direct translation of a clean Material-style heart
                // SVG (100×100 viewBox, geometric centre ~(50, 47.5)). Six
                // cubics close the path; gives proper rounded lobes + a
                // tapered tip, matches typical heart-icon expectations.
                const float u = r * 0.030f;
                auto pt = [&](float px, float py) {
                    return juce::Point<float>(cx + (px - 50.0f) * u,
                                               cy + (py - 47.5f) * u);
                };

                p.startNewSubPath(pt(50, 90));
                p.cubicTo(pt(35,  80), pt(0,   55), pt(0,   30));
                p.cubicTo(pt(0,   15), pt(12,   5), pt(25,   5));
                p.cubicTo(pt(35,   5), pt(42,  12), pt(50,  25));
                p.cubicTo(pt(58,  12), pt(65,   5), pt(75,   5));
                p.cubicTo(pt(88,   5), pt(100, 15), pt(100, 30));
                p.cubicTo(pt(100, 55), pt(65,  80), pt(50,  90));
                p.closeSubPath();
                break;
            }
            case SidebarIcon::Packs:
            {
                // Stacked layers — three flattened diamonds (Arturia-style).
                const float layerW   = r * 1.7f;
                const float layerH   = r * 0.55f;
                const float layerGap = r * 0.32f;
                for (int i = 0; i < 3; ++i)
                {
                    const float yc = cy - layerGap + i * layerGap;
                    p.startNewSubPath(cx - layerW * 0.5f, yc);
                    p.lineTo(cx,                          yc - layerH * 0.5f);
                    p.lineTo(cx + layerW * 0.5f,          yc);
                    p.lineTo(cx,                          yc + layerH * 0.5f);
                    p.closeSubPath();
                }
                break;
            }
            case SidebarIcon::User:
            {
                // Person bust — circular head + U-shaped shoulders.
                const float headR  = r * 0.35f;
                const float headCY = cy - r * 0.38f;
                p.addEllipse(cx - headR, headCY - headR, headR * 2.0f, headR * 2.0f);

                // Shoulders: smooth arc from below the head out to the
                // edges of the bust, open at the bottom.
                const float shouldersTop    = headCY + headR + r * 0.10f;
                const float shouldersBottom = cy + r * 0.80f;
                const float shouldersHalfW  = r * 0.80f;
                p.startNewSubPath(cx - shouldersHalfW, shouldersBottom);
                p.cubicTo(cx - shouldersHalfW, shouldersTop,
                          cx + shouldersHalfW, shouldersTop,
                          cx + shouldersHalfW, shouldersBottom);
                break;
            }
        }

        g.setColour(stroke);
        g.strokePath(p, strokeT);
    }
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

    // Animated GIF cover player — added but hidden until a pack with a
    // cover.gif is single-clicked in Packs mode.
    addChildComponent(coverGifPlayer);

    // Back-to-Packs arrow — only visible when drilled into a single pack.
    // UTF-8 "‹" lightweight angle for a clean Arturia-style chevron.
    backToPacksButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xE2\x80\xB9")));
    backToPacksButton.getProperties().set("phantom-style", "header-raised");
    backToPacksButton.onClick = [this] {
        // Find the Packs pseudo-category and switch to it.
        for (size_t ci = 0; ci < categories.size(); ++ci)
        {
            if (categories[ci].kind == CategoryKind::Packs)
            {
                activeCategoryIdx = (int) ci;
                selectedPackCardIdx = -1;
                listScrollY = 0;
                rebuildRows();
                syncCoverGifPlayer();
                resized();
                repaint();
                return;
            }
        }
    };
    addChildComponent(backToPacksButton);

#if DEVELOPER_MODE
    auto setupAuthoringButton = [this](juce::TextButton& b)
    {
        b.setLookAndFeel(&closeButton.getLookAndFeel());  // reuse the same flat look
        addAndMakeVisible(b);
    };
    setupAuthoringButton(newPackButton);
    setupAuthoringButton(saveIntoPackButton);
    setupAuthoringButton(editPackMetaButton);
    setupAuthoringButton(setCoverButton);
    setupAuthoringButton(renamePackButton);
    setupAuthoringButton(deletePackButton);
    setupAuthoringButton(exportPackButton);
    setupAuthoringButton(importPackButton);

    // === Button callbacks ===

    newPackButton.onClick = [this]
    {
        auto* aw = new juce::AlertWindow("New Pack",
            "Create a new pack:", juce::AlertWindow::NoIcon);
        aw->addTextEditor("name",        "",  "Name:");
        aw->addTextEditor("description", "",  "Description:");
        aw->addTextEditor("designer",    "",  "Designer:");
        aw->addButton("Create", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw](int r)
            {
                if (r == 1)
                {
                    processor.getPresetManager().createPack(
                        aw->getTextEditorContents("name"),
                        aw->getTextEditorContents("description"),
                        aw->getTextEditorContents("designer"));
                }
            }), true);
    };

    saveIntoPackButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        auto* aw = new juce::AlertWindow("Save Into " + packName,
            "Preset name:", juce::AlertWindow::NoIcon);
        aw->addTextEditor("name", "", "Name:");
        aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw, packName](int r)
            {
                if (r == 1)
                {
                    processor.getPresetManager().savePresetIntoPack(
                        apvts, packName,
                        aw->getTextEditorContents("name"),
                        "Experimental", "User", "");
                }
            }), true);
    };

    editPackMetaButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        const auto packs = processor.getPresetManager().getAllPacks();
        auto it = std::find_if(packs.begin(), packs.end(),
            [&](const kaigen::phantom::PackInfo& p) { return p.name == packName; });
        if (it == packs.end()) return;

        auto* aw = new juce::AlertWindow("Edit Pack",
            "Edit metadata for " + packName, juce::AlertWindow::NoIcon);
        aw->addTextEditor("description", it->description, "Description:");
        aw->addTextEditor("designer",    it->designer,    "Designer:");
        aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw, packName](int r)
            {
                if (r == 1)
                {
                    processor.getPresetManager().setPackMetadata(packName,
                        aw->getTextEditorContents("description"),
                        aw->getTextEditorContents("designer"));
                }
            }), true);
    };

    setCoverButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        auto chooser = std::make_shared<juce::FileChooser>(
            "Choose cover art",
            juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
            "*.png;*.jpg;*.jpeg;*.gif");
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
            [this, packName, chooser](const juce::FileChooser& fc)
            {
                const auto src = fc.getResult();
                if (src.existsAsFile())
                    processor.getPresetManager().setPackCover(packName, src);
            });
    };

    renamePackButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        auto* aw = new juce::AlertWindow("Rename Pack",
            "New name for " + packName + ":", juce::AlertWindow::NoIcon);
        aw->addTextEditor("name", packName, "Name:");
        aw->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw, packName](int r)
            {
                if (r == 1)
                {
                    processor.getPresetManager().renamePack(packName,
                        aw->getTextEditorContents("name"));
                }
            }), true);
    };

    deletePackButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Delete Pack")
                .withMessage("Delete pack \"" + packName
                    + "\" and all its presets? This cannot be undone.")
                .withButton("Delete")
                .withButton("Cancel"),
            [this, packName](int r)
            {
                if (r == 1)
                    processor.getPresetManager().deletePack(packName);
            });
    };

    exportPackButton.onClick = [this]
    {
        const auto packName = activePackName();
        if (packName.isEmpty()) return;

        auto chooser = std::make_shared<juce::FileChooser>(
            "Export " + packName + " as .kaipack",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile(packName + ".kaipack"),
            "*.kaipack");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles,
            [this, packName, chooser](const juce::FileChooser& fc)
            {
                const auto dest = fc.getResult();
                if (dest != juce::File{})
                    processor.getPresetManager().exportPack(packName, dest);
            });
    };

    importPackButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import .kaipack",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.kaipack");
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                const auto src = fc.getResult();
                if (src.existsAsFile())
                    processor.getPresetManager().importPack(src, /*overwrite*/ false);
            });
    };
#endif

    processor.getPresetManager().addChangeListener(this);
}

PresetBrowser::~PresetBrowser()
{
    processor.getPresetManager().removeChangeListener(this);
}

void PresetBrowser::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (! isVisible()) return;
    rebuildCategories();
    rebuildPackCards();
    rebuildRows();
    // A rescan may have replaced/removed the selected pack's cover file
    // (setPackCover, deletePack, etc.) — drop the cached GIF so the next
    // sync reloads from disk.
    coverGifPlayer.clear();
    coverGifPackName.clear();
    syncCoverGifPlayer();
    repaint();
}

void PresetBrowser::visibilityChanged()
{
    if (! isVisible())
    {
        rows.clear();
        categories.clear();
        hoverRow = -1;
        hoverCategoryIdx = -1;
        hoverPackCardIdx = -1;
        selectedPackCardIdx = -1;
        selectedPreviewRow = -1;
        sidebarScrollY = 0;
        listScrollY = 0;
        sortColumn = SortColumn::None;
        sortAscending = true;
        deleteButton.setVisible(false);
        coverGifPlayer.clear();
        coverGifPlayer.setVisible(false);
        coverGifPackName.clear();
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
    // Icons are drawn from juce::Path in paintSidebarIcon, keyed by kind.
    categories.push_back({ CategoryKind::Explore,   "Explore",   {} });
    categories.push_back({ CategoryKind::Favorites, "Favorites", {} });
    categories.push_back({ CategoryKind::Packs,     "Packs",     {} });

    // Direct pack drill-ins below — same data, scoped to a single pack.
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

    // Collect every matching row first; group-by-pack header insertion is
    // applied at the end only when no column sort is active.
    std::vector<Row> all;
    const auto packs = processor.getPresetManager().getAllPresets();
    for (const auto& [packName, presets] : packs)
    {
        if (active.kind == CategoryKind::Pack && active.packFilter != packName)
            continue;

        for (const auto& p : presets)
        {
            if (active.kind == CategoryKind::Favorites && ! p.metadata.isFavorite)
                continue;
            if (hasQuery && ! p.metadata.name.toLowerCase().contains(query))
                continue;
            Row r;
            r.name        = p.metadata.name;
            r.pack        = packName;
            r.type        = p.metadata.type;
            r.designer    = p.metadata.designer;
            r.description = p.metadata.description;
            for (int hi = 0; hi < 7; ++hi) r.h[hi] = p.preview.h[hi];
            r.crossover   = p.preview.crossover;
            r.skip        = p.preview.skip;
            r.isFavorite  = p.metadata.isFavorite;
            r.isHeader    = false;
            all.push_back(std::move(r));
        }
    }
    if (all.empty()) return;

    if (sortColumn != SortColumn::None)
    {
        // Spectral centroid for SHAPE sort — weighted-avg harmonic frequency.
        auto centroid = [](const Row& r) {
            const float fund = juce::jmax(0.01f, r.crossover / std::pow(2.0f, (float) r.skip));
            float num = 0.0f, den = 0.0f;
            for (int i = 0; i < 7; ++i) { num += r.h[i] * (float) (i + 2) * fund; den += r.h[i]; }
            return den > 0.0f ? num / den : 0.0f;
        };

        const bool asc = sortAscending;
        std::sort(all.begin(), all.end(), [&](const Row& a, const Row& b) {
            int cmp = 0;
            switch (sortColumn)
            {
                case SortColumn::Name:
                    cmp = a.name.compareIgnoreCase(b.name); break;
                case SortColumn::Type:
                    cmp = a.type.compareIgnoreCase(b.type); break;
                case SortColumn::Designer:
                    cmp = a.designer.compareIgnoreCase(b.designer); break;
                case SortColumn::Skip:
                    cmp = (a.skip < b.skip) ? -1 : (a.skip > b.skip) ? 1 : 0; break;
                case SortColumn::Shape:
                {
                    const float ca = centroid(a), cb = centroid(b);
                    cmp = (ca < cb) ? -1 : (ca > cb) ? 1 : 0;
                    break;
                }
                case SortColumn::None: break;
            }
            if (cmp == 0) cmp = a.name.compareIgnoreCase(b.name);   // stable secondary
            return asc ? (cmp < 0) : (cmp > 0);
        });

        // Sorted view is flat — pack name becomes implicit via DESIGNER /
        // metadata, no group headers.
        rows.reserve(all.size());
        for (auto& r : all) rows.push_back(std::move(r));
        return;
    }

    // No sort active — re-group by pack with header rows above each block.
    juce::String currentPack;
    for (auto& r : all)
    {
        if (r.pack != currentPack)
        {
            currentPack = r.pack;
            Row header;
            header.pack     = currentPack;
            header.isHeader = true;
            rows.push_back(std::move(header));
        }
        rows.push_back(std::move(r));
    }
}

juce::Rectangle<int> PresetBrowser::cardBounds() const
{
    // Full-screen browser — the webview did this so text/columns/spectra get
    // maximum resolution. The compact dropdown handles the quick-pick case.
    return getLocalBounds();
}

juce::Rectangle<int> PresetBrowser::contentBounds() const
{
    // Card area reserved for browsable content (list / preview / sidebar /
    // pack grid). In DEV builds the AUTHORING strip lives at the bottom of
    // the card, so layouts that draw or hit-test content must shrink to
    // avoid overlapping it. In ship builds the strip doesn't exist and
    // this is identical to cardBounds().
    auto c = cardBounds();
#if DEVELOPER_MODE
    c.removeFromBottom(kAuthoringStripH);
#endif
    return c;
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
    // Full-bleed silver background — fills the editor. The frame fill uses
    // the full card; the content layout below uses contentBounds() so the
    // DEV AUTHORING strip isn't covered by sidebar / preview / list paint.
    const auto frame  = cardBounds();
    const auto frameF = frame.toFloat();
    g.setGradientFill(cardGradient(frameF));
    g.fillRect(frame);

    // Layout slots inside the card.
    auto inner = contentBounds();
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

        // Two-column layout inside the row: [icon] [label]. Top-level
        // entries get a line-art icon; per-pack drill-ins get an indented
        // label only.
        constexpr int kIconW = 28;
        const auto iconCol  = juce::Rectangle<int>(rowBounds.getX() + 8, rowBounds.getY(),
                                                    kIconW, rowBounds.getHeight());
        const auto labelCol = juce::Rectangle<int>(iconCol.getRight() + 6, rowBounds.getY(),
                                                    rowBounds.getRight() - iconCol.getRight() - 8,
                                                    rowBounds.getHeight());

        const auto strokeColour = juce::Colour(isActive ? kTextStrong : kTextBody);
        const bool isUserPackRow = (c.kind == CategoryKind::Pack
                                    && c.packFilter == "User");

        if (c.kind == CategoryKind::Explore)
            paintSidebarIcon(g, iconCol.toFloat(), SidebarIcon::Explore, strokeColour);
        else if (c.kind == CategoryKind::Favorites)
        {
            // Favorites uses the same unicode heart glyph as the pill, so
            // both places read as one "heart = favorite" affordance.
            g.setColour(strokeColour);
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 18.0f, juce::Font::plain));
            g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x99\xA5")),
                        iconCol, juce::Justification::centred, false);
        }
        else if (c.kind == CategoryKind::Packs)
            paintSidebarIcon(g, iconCol.toFloat(), SidebarIcon::Packs, strokeColour);
        else if (isUserPackRow)
            paintSidebarIcon(g, iconCol.toFloat(), SidebarIcon::User, strokeColour);
        // Other per-pack entries (Factory, etc) have no icon — indented label only.

        g.setColour(strokeColour);
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 14.0f,
                                     isActive ? juce::Font::bold : juce::Font::plain));

        // User-pack uses the standard [icon][label] columns; other per-pack
        // entries indent under the Packs row.
        const bool indentSubPack = (c.kind == CategoryKind::Pack && ! isUserPackRow);
        const auto textArea = indentSubPack
                                ? rowBounds.withTrimmedLeft(20).withTrimmedRight(8)
                                : labelCol;
        g.drawText(c.label, textArea, juce::Justification::centredLeft, false);
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

        // Packs-mode selection branch — show pack metadata + cover when
        // the user has single-clicked a pack tile. Wins over the preset
        // selection branch since Packs mode hides the row table anyway.
        const bool packSelected = isPacksMode()
                                && selectedPackCardIdx >= 0
                                && selectedPackCardIdx < (int) packCards.size();

        const bool hasSelection = selectedPreviewRow >= 0
                                && selectedPreviewRow < (int) rows.size()
                                && ! rows[(size_t) selectedPreviewRow].isHeader;

        if (packSelected)
        {
            const auto& pc = packCards[(size_t) selectedPackCardIdx];
            const auto packInfos = processor.getPresetManager().getAllPacks();
            auto pit = std::find_if(packInfos.begin(), packInfos.end(),
                [&](const kaigen::phantom::PackInfo& p) { return p.name == pc.name; });

            auto inner = previewBox.reduced(14, 14);

            // Cover image — large portrait at the top, square aspect, centred.
            const int coverSize = juce::jmin(inner.getWidth(), 220);
            auto coverArea = inner.removeFromTop(coverSize);
            inner.removeFromTop(12);

            const auto coverFile = processor.getPresetManager()
                                            .getPackCoverFile(pc.name);
            const bool isGifCover = coverFile.existsAsFile()
                && coverFile.getFileExtension().equalsIgnoreCase(".gif");
            if (coverFile.existsAsFile() && ! isGifCover)
            {
                auto img = juce::ImageFileFormat::loadFrom(coverFile);
                if (img.isValid())
                    g.drawImage(img, coverArea.toFloat(),
                                juce::RectanglePlacement::fillDestination);
            }
            // For GIF covers the GifPlayer child component paints this
            // rect on top of whatever the preview pane background is.
            else
            {
                // Fallback initial-letter art (matches pack-tile style).
                const auto seed = (juce::uint32) pc.name.hashCode();
                const float hue = (float) (seed % 360) / 360.0f;
                juce::ColourGradient grad(
                    juce::Colour::fromHSV(hue, 0.55f, 0.80f, 1.0f),
                    (float) coverArea.getX(), (float) coverArea.getY(),
                    juce::Colour::fromHSV(std::fmod(hue + 0.12f, 1.0f),
                                            0.45f, 0.55f, 1.0f),
                    (float) coverArea.getRight(), (float) coverArea.getBottom(),
                    false);
                g.setGradientFill(grad);
                g.fillRect(coverArea);
                g.setColour(juce::Colour(0xd9FFFFFF));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(),
                    (float) coverArea.getHeight() * 0.55f, juce::Font::plain));
                g.drawText(pc.displayName.substring(0, 1).toUpperCase(),
                           coverArea, juce::Justification::centred, false);
            }

            // Name (bold, large)
            g.setColour(juce::Colour(kTextStrong));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 18.0f, juce::Font::bold));
            g.drawText(pc.displayName, inner.removeFromTop(24),
                       juce::Justification::topLeft, true);
            inner.removeFromTop(6);

            // Preset count.
            g.setColour(juce::Colour(kTextLabel));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 12.0f, juce::Font::plain));
            g.drawText(juce::String(pc.presetCount) + " preset"
                        + (pc.presetCount == 1 ? "" : "s"),
                       inner.removeFromTop(16),
                       juce::Justification::topLeft, false);
            inner.removeFromTop(8);

            // Designer + description from PackInfo.
            if (pit != packInfos.end())
            {
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
                drawKV("Designer:", pit->designer);

                if (pit->description.isNotEmpty())
                {
                    inner.removeFromTop(10);
                    g.setColour(juce::Colour(kBorderSoft));
                    g.drawLine((float) inner.getX(), (float) inner.getY(),
                                (float) inner.getRight(), (float) inner.getY(), 1.0f);
                    inner.removeFromTop(10);
                    g.setColour(juce::Colour(kTextBody));
                    g.setFont(juce::FontOptions(Theme::uiFontFamily(), 12.0f, juce::Font::plain));
                    g.drawFittedText(pit->description, inner, juce::Justification::topLeft, 8);
                }
            }
        }
        else if (! hasSelection)
        {
            g.setColour(juce::Colour(kTextDim));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(), 13.0f, juce::Font::plain));
            g.drawText(isPacksMode() ? "Click a pack to inspect" : "Select a preset",
                       previewBox.reduced(14),
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
    auto packBanner = isPackDrillIn() ? middleBounds.removeFromTop(kPackBannerH)
                                       : juce::Rectangle<int>{};
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

    // Pack drill-in banner — cover thumb + BANK label + name + count.
    // The back-arrow itself is a real juce::TextButton positioned in
    // resized(), so we just leave its rect blank.
    if (isPackDrillIn() && ! packBanner.isEmpty())
    {
        const auto& cat = categories[(size_t) activeCategoryIdx];
        const auto packInfos = processor.getPresetManager().getAllPacks();
        auto pit = std::find_if(packInfos.begin(), packInfos.end(),
            [&](const kaigen::phantom::PackInfo& p) { return p.name == cat.packFilter; });
        const auto displayName = (pit != packInfos.end()) ? pit->displayName : cat.label;

        // Cover thumbnail.
        const auto coverFile = processor.getPresetManager()
                                        .getPackCoverFile(cat.packFilter);
        juce::Image thumb;
        if (coverFile.existsAsFile())
            thumb = juce::ImageFileFormat::loadFrom(coverFile);

        const auto coverRect = packBannerCoverBounds();
        if (thumb.isValid())
        {
            g.drawImage(thumb, coverRect.toFloat(),
                        juce::RectanglePlacement::fillDestination);
        }
        else
        {
            // Gradient + letter fallback (matches the tile style).
            const auto seed = (juce::uint32) cat.packFilter.hashCode();
            const float hue = (float) (seed % 360) / 360.0f;
            juce::ColourGradient grad(
                juce::Colour::fromHSV(hue, 0.55f, 0.80f, 1.0f),
                (float) coverRect.getX(), (float) coverRect.getY(),
                juce::Colour::fromHSV(std::fmod(hue + 0.12f, 1.0f),
                                        0.45f, 0.55f, 1.0f),
                (float) coverRect.getRight(), (float) coverRect.getBottom(),
                false);
            g.setGradientFill(grad);
            g.fillRect(coverRect);
            g.setColour(juce::Colour(0xd9FFFFFF));
            g.setFont(juce::FontOptions(Theme::uiFontFamily(),
                (float) coverRect.getHeight() * 0.55f, juce::Font::plain));
            g.drawText(displayName.substring(0, 1).toUpperCase(),
                       coverRect, juce::Justification::centred, false);
        }

        // Text column to the right of the cover thumb.
        auto textArea = packBanner.reduced(16, 12);
        textArea.removeFromLeft(28 + 12 + 64 + 16);  // back arrow + gap + cover + gap

        g.setColour(juce::Colour(kTextLabel));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 10.0f, juce::Font::bold));
        g.drawText("BANK", textArea.removeFromTop(14),
                   juce::Justification::topLeft, false);
        textArea.removeFromTop(2);

        g.setColour(juce::Colour(kTextStrong));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 22.0f, juce::Font::bold));
        g.drawText(displayName, textArea.removeFromTop(28),
                   juce::Justification::topLeft, true);
        textArea.removeFromTop(2);

        const int presetCount = (int) std::count_if(rows.begin(), rows.end(),
                                    [](const Row& r) { return ! r.isHeader; });
        g.setColour(juce::Colour(kTextLabel));
        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 12.0f, juce::Font::plain));
        g.drawText(juce::String(presetCount) + " preset" + (presetCount == 1 ? "" : "s"),
                   textArea.removeFromTop(16), juce::Justification::topLeft, false);

        // Banner bottom border.
        g.setColour(juce::Colour(kBorderSoft));
        g.drawLine((float) packBanner.getX(), (float) packBanner.getBottom(),
                   (float) packBanner.getRight(), (float) packBanner.getBottom(), 1.0f);
    }

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

            // Cover-image art square: use cover.png/.jpg/.gif when present
            // (GIFs render the first frame only — animating every tile would
            // be a CPU/memory tax for marginal benefit), otherwise fall back
            // to a deterministic gradient + initial-letter mark.
            auto art = card.removeFromTop(card.getWidth());

            const auto coverFile = processor.getPresetManager()
                                            .getPackCoverFile(pc.name);
            juce::Image cover;
            if (coverFile.existsAsFile())
                cover = juce::ImageFileFormat::loadFrom(coverFile);

            if (cover.isValid())
            {
                // fillDestination crops to avoid letterbox bars on
                // portrait/landscape covers in the square tile. No
                // backdrop fill — transparent PNG pixels show the card
                // surface; opaque pixels show true colour.
                g.drawImage(cover, art.toFloat(),
                            juce::RectanglePlacement::fillDestination);
            }
            else
            {
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

                g.setColour(juce::Colour(0xd9FFFFFF));
                g.setFont(juce::FontOptions(Theme::uiFontFamily(),
                                             (float) art.getHeight() * 0.55f,
                                             juce::Font::plain));
                g.drawText(pc.displayName.substring(0, 1).toUpperCase(),
                           art, juce::Justification::centred, false);
            }

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

            // Persistent selection ring — single-click selects a pack
            // without drilling in (double-click drills in).
            if ((int) i == selectedPackCardIdx)
            {
                g.setColour(juce::Colour(0xffFFFFFF));
                g.drawRect(cardOuter, 2);
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

        g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::bold));

        // Draw each header label; tint stronger if it's the active sort
        // column, and append a small arrow indicating direction.
        auto drawHeader = [&](const juce::String& label,
                              juce::Rectangle<int> col,
                              SortColumn which,
                              juce::Justification just) {
            const bool isActive = (sortColumn == which);
            g.setColour(juce::Colour(isActive ? kTextStrong : kTextLabel));
            g.drawText(label, col, just, false);
            if (isActive)
            {
                const auto arrow = sortAscending
                    ? juce::String(juce::CharPointer_UTF8("\xe2\x96\xb2"))    // ▲
                    : juce::String(juce::CharPointer_UTF8("\xe2\x96\xbc"));   // ▼
                const auto textW = (int) g.getCurrentFont().getStringWidthFloat(label) + 4;
                auto arrowRect = (just == juce::Justification::centredRight)
                    ? juce::Rectangle<int>(col.getRight() - textW - 10, col.getY(), 10, col.getHeight())
                    : juce::Rectangle<int>(col.getX() + textW, col.getY(), 10, col.getHeight());
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 8.0f, juce::Font::plain));
                g.drawText(arrow, arrowRect, juce::Justification::centred, false);
                g.setFont(juce::FontOptions(Theme::uiFontFamily(), 11.0f, juce::Font::bold));
            }
        };

        drawHeader("NAME",     nameCol,     SortColumn::Name,     juce::Justification::centredLeft);
        drawHeader("TYPE",     typeCol,     SortColumn::Type,     juce::Justification::centredLeft);
        drawHeader("DESIGNER", designerCol, SortColumn::Designer, juce::Justification::centredLeft);
        drawHeader("SHAPE",    shapeCol,    SortColumn::Shape,    juce::Justification::centredLeft);
        drawHeader("SKIP",     skipCol,     SortColumn::Skip,     juce::Justification::centredRight);
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

#if DEVELOPER_MODE
    {
        const auto strip = authoringStripBounds();
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRect(strip.getX(), strip.getY(), strip.getWidth(), 1);
    }
#endif
}

bool PresetBrowser::isPackDrillIn() const noexcept
{
    return ! categories.empty()
        && activeCategoryIdx >= 0
        && activeCategoryIdx < (int) categories.size()
        && categories[(size_t) activeCategoryIdx].kind == CategoryKind::Pack;
}

int PresetBrowser::packBannerHeight() const noexcept
{
    return isPackDrillIn() ? kPackBannerH : 0;
}

juce::Rectangle<int> PresetBrowser::packBannerBounds() const
{
    if (! isPackDrillIn()) return {};
    auto card = contentBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    inner.removeFromTop(kHeaderBarH + kSearchBarH);
    return inner.removeFromTop(kPackBannerH);
}

juce::Rectangle<int> PresetBrowser::packBannerBackButtonBounds() const
{
    auto banner = packBannerBounds();
    if (banner.isEmpty()) return {};
    auto inner = banner.reduced(16, 12);
    return inner.removeFromLeft(28).withSizeKeepingCentre(28, 32);
}

juce::Rectangle<int> PresetBrowser::packBannerCoverBounds() const
{
    auto banner = packBannerBounds();
    if (banner.isEmpty()) return {};
    auto inner = banner.reduced(16, 12);
    inner.removeFromLeft(28 + 12);  // skip back arrow + gap
    return inner.removeFromLeft(64).withSizeKeepingCentre(64, 64);
}

juce::Rectangle<int> PresetBrowser::searchBarBounds() const
{
    // Top-anchored, so the bottom-of-card strip doesn't change placement,
    // but route through contentBounds() so any future bottom-anchored math
    // here stays correct.
    auto card = contentBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    inner.removeFromTop(kHeaderBarH);   // skip the title bar
    return inner.removeFromTop(kSearchBarH);
}

juce::Rectangle<int> PresetBrowser::columnHeaderBounds(SortColumn col) const
{
    // Top-anchored; uses contentBounds() for consistency with the rest of
    // the middle-column layout (so sort-column hit-tests line up with the
    // headers paint() draws).
    auto card = contentBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    auto middle = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + packBannerHeight());
    auto colBar = middle.removeFromTop(kColHeaderH);

    const auto cells = colBar.getX() + 12;
    int x = colBar.getRight() - 12;
    auto take = [&](int w) {
        const juce::Rectangle<int> r { x - w, colBar.getY(), w, colBar.getHeight() };
        x = r.getX() - kColGap;
        return r;
    };
    /* heartCol  = */ take(kColHeartW);
    const auto skipCol     = take(kColSkipW);
    const auto shapeCol    = take(kColShapeW);
    const auto designerCol = take(kColDesignerW);
    const auto typeCol     = take(kColTypeW);
    const juce::Rectangle<int> nameCol { cells, colBar.getY(),
                                          x - cells, colBar.getHeight() };

    switch (col)
    {
        case SortColumn::Name:     return nameCol;
        case SortColumn::Type:     return typeCol;
        case SortColumn::Designer: return designerCol;
        case SortColumn::Shape:    return shapeCol;
        case SortColumn::Skip:     return skipCol;
        case SortColumn::None:
        default:                   return {};
    }
}

juce::Rectangle<int> PresetBrowser::packCardBounds(int idx) const
{
    // Pack cards lay out from top with a bottom-clip — must use
    // contentBounds() so the bottom row isn't pushed under the DEV strip.
    auto card = contentBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + packBannerHeight()).reduced(16, 16);

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
    // contentBounds() so the preview pane's bottom edge sits above the DEV
    // strip — otherwise the strip would paint over the delete button and
    // the bottom of the metadata area.
    auto card = contentBounds();
    return card.removeFromRight(kPreviewW);
}

juce::Rectangle<int> PresetBrowser::previewDeleteButtonBounds() const
{
    // Pin to the bottom-left of the preview panel's inner card.
    auto box = previewBounds().reduced(10, 0).withTrimmedTop(28).withTrimmedBottom(10).reduced(10, 10);
    return juce::Rectangle<int>(box.getX(), box.getBottom() - 24, 80, 22);
}

juce::Rectangle<int> PresetBrowser::packPreviewCoverBounds() const
{
    // Mirrors the cover-area math inside the Packs-mode paint branch:
    // inner = previewBox.reduced(14,14), then a square inner.removeFromTop
    // bounded by min(width, 220). Keep both code paths in sync.
    auto previewBox = previewBounds().reduced(10, 0).withTrimmedTop(28).withTrimmedBottom(10);
    auto inner = previewBox.reduced(14, 14);
    const int coverSize = juce::jmin(inner.getWidth(), 220);
    return inner.removeFromTop(coverSize);
}

void PresetBrowser::syncCoverGifPlayer()
{
    const bool packSelected = isPacksMode()
                            && selectedPackCardIdx >= 0
                            && selectedPackCardIdx < (int) packCards.size();

    if (! packSelected)
    {
        if (coverGifPackName.isNotEmpty())
        {
            coverGifPlayer.clear();
            coverGifPlayer.setVisible(false);
            coverGifPackName.clear();
        }
        return;
    }

    const auto& pc = packCards[(size_t) selectedPackCardIdx];
    const auto coverFile = processor.getPresetManager().getPackCoverFile(pc.name);
    const bool isGif = coverFile.existsAsFile()
        && coverFile.getFileExtension().equalsIgnoreCase(".gif");

    if (! isGif)
    {
        if (coverGifPackName.isNotEmpty())
        {
            coverGifPlayer.clear();
            coverGifPlayer.setVisible(false);
            coverGifPackName.clear();
        }
        return;
    }

    if (coverGifPackName != pc.name)
    {
        coverGifPlayer.load(coverFile);
        coverGifPackName = pc.name;
    }
    coverGifPlayer.setBounds(packPreviewCoverBounds());
    coverGifPlayer.setVisible(true);
    coverGifPlayer.toFront(false);
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
        // height. Must mirror packCardBounds' use of contentBounds() so
        // listOriginY matches the actual card layout.
        const auto card = contentBounds();
        auto inner = card;
        inner.removeFromLeft(kSidebarW);
        inner.removeFromRight(kPreviewW);
        const auto listOriginY = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + packBannerHeight()).reduced(16, 16).getY();
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
    // Use contentBounds() so wheel events over the DEV AUTHORING strip
    // don't scroll the list/sidebar (the strip is non-scrollable chrome).
    const auto card = contentBounds();
    if (! card.contains(e.getPosition())) return;

    const int dy = juce::roundToInt(wheel.deltaY * 60.0f
                                     * (wheel.isReversed ? 1.0f : -1.0f));
    if (dy == 0) return;

    // Sidebar vs. list: pick by where the cursor is.
    auto sidebar = card.withWidth(kSidebarW);
    auto inner   = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    const auto listView = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + packBannerHeight()
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
    // contentBounds() so sidebar row hit-tests don't extend over the DEV
    // strip and steal clicks meant for the AUTHORING buttons.
    const auto card = contentBounds();
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
    if (coverGifPlayer.isVisible())
        coverGifPlayer.setBounds(packPreviewCoverBounds());

    const bool drilledIn = isPackDrillIn();
    backToPacksButton.setVisible(drilledIn);
    if (drilledIn)
        backToPacksButton.setBounds(packBannerBackButtonBounds());

#if DEVELOPER_MODE
    {
        auto strip = authoringStripBounds().reduced(8);
        const int gap = 6;
        const int buttons = 8;
        const int btnW = (strip.getWidth() - gap * (buttons - 1)) / buttons;

        auto place = [&](juce::TextButton& b)
        {
            b.setBounds(strip.removeFromLeft(btnW));
            strip.removeFromLeft(gap);
        };
        place(newPackButton);
        place(saveIntoPackButton);
        place(editPackMetaButton);
        place(setCoverButton);
        place(renamePackButton);
        place(deletePackButton);
        place(exportPackButton);
        place(importPackButton);
    }
#endif
}

void PresetBrowser::mouseMove(const juce::MouseEvent& e)
{
    // contentBounds() so row hover hit-tests stop above the DEV strip.
    const auto card = contentBounds();
    auto inner = card;
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + packBannerHeight() + kColHeaderH).reduced(8, 4);

    int newRowHover = -1;
    if (! isPacksMode())
    {
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

void PresetBrowser::mouseDoubleClick(const juce::MouseEvent& e)
{
    // In Packs mode, double-click on a pack tile drills into the
    // pack's preset list. (Single-click selects in mouseDown.)
    if (! isPacksMode()) return;

    for (size_t i = 0; i < packCards.size(); ++i)
    {
        if (! packCardBounds((int) i).translated(0, -listScrollY)
                .contains(e.getPosition())) continue;

        const auto& pc = packCards[i];
        for (size_t ci = 0; ci < categories.size(); ++ci)
        {
            if (categories[ci].kind == CategoryKind::Pack
                && categories[ci].packFilter == pc.name)
            {
                activeCategoryIdx = (int) ci;
                selectedPackCardIdx = -1;
                listScrollY = 0;
                rebuildRows();
                syncCoverGifPlayer();
                resized();
                repaint();
                return;
            }
        }
        return;
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
                selectedPackCardIdx = -1;
                rebuildRows();
                syncCoverGifPlayer();
                resized();   // back button + banner layout depend on drill-in state
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

    // Packs mode: single-click selects a pack tile (populates preview pane
    // + activates AUTHORING buttons). Double-click drills into the pack
    // (handled in mouseDoubleClick). Clicking empty space deselects.
    if (isPacksMode())
    {
        int hit = -1;
        for (size_t i = 0; i < packCards.size(); ++i)
        {
            if (packCardBounds((int) i).translated(0, -listScrollY)
                    .contains(e.getPosition()))
            {
                hit = (int) i;
                break;
            }
        }
        if (hit != selectedPackCardIdx)
        {
            selectedPackCardIdx = hit;
            syncCoverGifPlayer();
            repaint();
        }
        return;
    }

    // Column-header click → toggle sort. Cycle per column:
    //   None → Asc → Desc → None
    {
        struct H { SortColumn col; };
        for (auto h : { H{ SortColumn::Name },     H{ SortColumn::Type },
                        H{ SortColumn::Designer }, H{ SortColumn::Shape },
                        H{ SortColumn::Skip } })
        {
            if (columnHeaderBounds(h.col).contains(e.getPosition()))
            {
                if (sortColumn != h.col)        { sortColumn = h.col; sortAscending = true; }
                else if (sortAscending)         { sortAscending = false; }
                else                            { sortColumn = SortColumn::None; }
                listScrollY = 0;
                rebuildRows();
                repaint();
                return;
            }
        }
    }

    // In Packs mode the middle column shows pack cards, not preset rows,
    // so row hit-testing is skipped entirely — the rows vector is still
    // populated (rebuildRows doesn't clear it) and paint() also skips
    // drawing them, so without this guard clicks would land on invisible
    // rows and load presets.
    if (isPacksMode()) return;

    // List area: hit test the preset rows (with scroll offset).
    // contentBounds() so clicks in the bottom DEV strip don't get
    // interpreted as preset-row clicks.
    auto inner = contentBounds();
    inner.removeFromLeft(kSidebarW);
    inner.removeFromRight(kPreviewW);
    const auto listArea = inner.withTrimmedTop(kHeaderBarH + kSearchBarH + packBannerHeight() + kColHeaderH).reduced(8, 4);
    if (! listArea.contains(e.getPosition())) return;

    // The heart column sits at the rightmost ~30 px of each row; clicks
    // there toggle the favorite for that preset instead of loading it.
    const auto heartColRect = columnHeaderBounds(SortColumn::Skip).isEmpty()
        ? juce::Rectangle<int>()
        : juce::Rectangle<int>(listArea.getRight() - kColHeartW,
                                listArea.getY(),
                                kColHeartW,
                                listArea.getHeight());

    int y = listArea.getY() - listScrollY;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (y + h <= listArea.getY()) { y += h; continue; }
        if (y >= listArea.getBottom()) break;
        if (e.y >= y && e.y < y + h)
        {
            if (! r.isHeader
                && heartColRect.getX() <= e.x
                && e.x <  heartColRect.getRight())
            {
                processor.getPresetManager()
                    .setFavorite(r.name, r.pack, ! r.isFavorite);
                // PresetManager broadcasts → changeListenerCallback rebuilds
                // rows + repaints with the updated heart.
                return;
            }
            loadPresetAt((int) i);
            return;
        }
        y += h;
    }
}

#if DEVELOPER_MODE
juce::String PresetBrowser::activePackName() const
{
    if (activeCategoryIdx < 0 || activeCategoryIdx >= (int) categories.size())
        return {};
    const auto& cat = categories[(size_t) activeCategoryIdx];
    if (cat.kind == CategoryKind::Pack) return cat.packFilter;

    // In Packs mode, the single-click selection acts as the active pack
    // for the AUTHORING buttons without forcing a drill-in.
    if (cat.kind == CategoryKind::Packs
        && selectedPackCardIdx >= 0
        && selectedPackCardIdx < (int) packCards.size())
        return packCards[(size_t) selectedPackCardIdx].name;

    return {};
}

juce::Rectangle<int> PresetBrowser::authoringStripBounds() const
{
    const auto card = cardBounds();
    return { card.getX(), card.getBottom() - kAuthoringStripH,
             card.getWidth(), kAuthoringStripH };
}
#endif

} // namespace kaigen::phantom
