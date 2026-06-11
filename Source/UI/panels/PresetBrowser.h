// Source/UI/panels/PresetBrowser.h
#pragma once
#include <functional>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../widgets/PackGifCache.h"

class PhantomProcessor;

namespace kaigen::phantom
{

/** Overlay component: fills the editor when visible. Lists all presets
 *  grouped by pack; click a row to load that preset. Close button
 *  dismisses without loading.
 *
 *  When the user picks a preset, fires `onPresetSelected(name, pack)`
 *  callback (NativePluginEditor wires this to update the PresetSelector's
 *  current-preset display). */
class PresetBrowser : public juce::Component,
                       public juce::ChangeListener
{
public:
    PresetBrowser(PhantomProcessor& processor,
                  juce::AudioProcessorValueTreeState& apvts);
    ~PresetBrowser() override;

    /** Re-fetch the preset list when PresetManager broadcasts a change
     *  (another instance saved / deleted, or a manual rescan). */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    /** Fires when a preset row is clicked: (name, pack). */
    std::function<void(juce::String, juce::String)> onPresetSelected;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override;
    void visibilityChanged() override;

private:
    void loadPresetAt(int rowIndex);
    juce::Rectangle<int> cardBounds() const;
    // Effective content card bounds — same as cardBounds() but in
    // DEVELOPER_MODE builds, reserves kAuthoringStripH at the bottom for
    // the AUTHORING strip so the list / preview / sidebar don't overlap
    // the buttons.
    juce::Rectangle<int> contentBounds() const;
    juce::Rectangle<int> searchBarBounds() const;
    juce::Rectangle<int> sidebarRowBounds(int categoryIdx) const;
    void rebuildCategories();    // build sidebar entries from preset packs
    void rebuildRows();          // build the visible row list for the active category

    struct Row {
        juce::String name;
        juce::String pack;
        juce::String type;          // "Synth" / "Bass" / "Experimental" / ...
        juce::String designer;
        juce::String description;
        float        h[7]      { 0, 0, 0, 0, 0, 0, 0 };  // recipe_h2..h8 (0..1)
        float        crossover { 120.0f };               // phantom_threshold Hz
        int          skip      { 0 };
        bool         isFavorite{ false };
        bool         isHeader  { false };
    };
    std::vector<Row> rows;
    int hoverRow             { -1 };
    int selectedPreviewRow   { -1 };   // last row hovered/clicked, drives preview pane

    /** Column-header sort state. Clicking a header column sorts asc;
     *  clicking the same column again flips direction. Persisted only for
     *  the lifetime of the browser; reset on visibilityChanged. */
    enum class SortColumn { None, Name, Type, Designer, Shape, Skip };
    SortColumn sortColumn   { SortColumn::None };
    bool       sortAscending{ true };
    int        hoverHeaderColumn { -1 };

    juce::Rectangle<int> columnHeaderBounds(SortColumn col) const;

    juce::Rectangle<int> previewBounds() const;
    juce::Rectangle<int> previewDeleteButtonBounds() const;
    juce::Rectangle<int> packPreviewCoverBounds() const;
    void deleteSelectedPreset();

    // Resolves the cover image to draw for a pack: GIFs route through the
    // shared gifCache (giving the current animation frame), static images
    // route through juce::ImageCache for cheap repeated paints. Returns
    // an invalid Image when the pack has no cover.
    juce::Image getPackCoverFrame(const juce::String& packName);

    // Paints a cover image into dest. When the image has translucent
    // pixels (alpha < ~250 anywhere in a 5-point sample), fills a solid
    // dark backdrop first so transparency shows dark instead of washing
    // out against whatever component background sits behind. Opaque
    // images skip the backdrop and draw directly.
    //
    // packName is used to look up an optional cover.crop.json sidecar
    // (GIF covers); when present, the crop is applied as a clip+
    // transform mask so the saved-as-is GIF only paints its cropped
    // region. Static covers ignore the sidecar (their crop is baked).
    void drawPackCover(juce::Graphics& g,
                        const juce::String& packName,
                        const juce::Image& img,
                        juce::Rectangle<int> dest);

    /** Visible pack cards in Explore mode. Computed in rebuildPackCards()
     *  from PresetManager::getAllPacks(); updated whenever the row list
     *  rebuilds so card hit-testing stays in sync with rendering. */
    struct PackCard {
        juce::String name;
        juce::String displayName;
        int          presetCount { 0 };
    };
    std::vector<PackCard> packCards;
    int hoverPackCardIdx    { -1 };
    int selectedPackCardIdx { -1 };   // single-click in Packs mode

    void rebuildPackCards();
    juce::Rectangle<int> packCardBounds(int idx) const;

    /** True when the active category is "Packs" — the middle column shows
     *  the pack-card grid instead of the preset table. */
    bool isPacksMode() const noexcept;

    /** Sidebar entries.
     *    Explore   — every preset across every pack, list-mode table.
     *    Favorites — filter to PresetMetadata.isFavorite.
     *    Packs     — special view that swaps the table for the pack-card grid.
     *    Pack      — direct drill-in to a single pack (Factory, User, ...).
     */
    enum class CategoryKind { Explore, Favorites, Packs, Pack };
    struct Category
    {
        CategoryKind kind;
        juce::String label;
        juce::String packFilter;   // valid when kind == Pack
    };
    std::vector<Category> categories;
    int activeCategoryIdx { 0 };   // 0 = Explore by default
    int hoverCategoryIdx  { -1 };

    // Scroll offsets in pixels for the sidebar and the main list/grid. Both
    // are hard-clamped to [0, max(0, contentHeight - viewHeight)] when the
    // mouse wheel changes them.
    int sidebarScrollY { 0 };
    int listScrollY    { 0 };

    int contentHeightForList() const;
    int contentHeightForSidebar() const;

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton closeButton;
    juce::TextButton deleteButton { "Delete" };
    juce::TextEditor searchField;

    // Shared GIF frame cache + advance timer. Tiles, preview pane, and
    // the drill-in banner all read frames from it during paint. Decoding
    // is lazy (first request per pack) and cached for the browser's
    // lifetime; invalidate(packName) is called when a rescan replaces a
    // cover; clearAll() runs when the browser hides.
    PackGifCache gifCache;

    // Back-to-Packs button — visible only when drilled into a specific
    // pack (CategoryKind::Pack). Click returns to the Packs grid.
    juce::TextButton backToPacksButton;

    // Returns kPackBannerH when the active category is a single-pack
    // drill-in, 0 otherwise. Every layout helper adds this to the top
    // strip so the row table sits below the banner.
    int  packBannerHeight() const noexcept;
    bool isPackDrillIn()   const noexcept;
    juce::Rectangle<int> packBannerBounds() const;
    juce::Rectangle<int> packBannerCoverBounds() const;
    juce::Rectangle<int> packBannerBackButtonBounds() const;

    static constexpr int kPackBannerH = 92;

#if DEVELOPER_MODE
public:
    /** Wired by NativePluginEditor — opens the CoverEditorOverlay for
     *  the given pack. PresetBrowser invokes it from the Set Cover
     *  button's onClick handler. */
    std::function<void(juce::String)> onSetCoverRequested;

private:
    juce::TextButton newPackButton       { "+ New Pack" };
    juce::TextButton saveIntoPackButton  { "Save Into Pack" };
    juce::TextButton editPackMetaButton  { "Edit Metadata" };
    juce::TextButton setCoverButton      { "Set Cover" };
    juce::TextButton renamePackButton    { "Rename" };
    juce::TextButton deletePackButton    { "Delete Pack" };
    juce::TextButton exportPackButton    { "Export Pack" };
    juce::TextButton importPackButton    { "Import Pack" };

    // Bounds of the AUTHORING strip — laid out under the row table.
    juce::Rectangle<int> authoringStripBounds() const;

    // Returns the currently-selected pack from the sidebar, or empty when
    // the active sidebar entry is Explore / Favorites / Packs (not a
    // specific pack drill-in).
    juce::String activePackName() const;

    static constexpr int kAuthoringStripH = 56;
#endif

    // Webview spec: card is 90% × 90% of parent (capped). 3-column layout:
    //   [sidebar 160] [middle flex] [preview 180]
    // Arturia-style proportions: taller rows + larger text for at-a-glance
    // readability. The full-screen browser layout has plenty of room.
    static constexpr int kRowHeight     = 36;
    static constexpr int kHeaderHeight  = 32;
    static constexpr int kColHeaderH    = 28;
    static constexpr int kSidebarW      = 200;
    static constexpr int kPreviewW      = 240;
    static constexpr int kHeaderBarH    = 56;
    static constexpr int kSearchBarH    = 44;

    // Wider columns to match the bigger metadata text.
    static constexpr int kColTypeW      = 110;
    static constexpr int kColDesignerW  = 110;
    static constexpr int kColShapeW     = 180;
    static constexpr int kColSkipW      = 50;
    static constexpr int kColHeartW     = 36;
    static constexpr int kColGap        = 12;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};

} // namespace kaigen::phantom
