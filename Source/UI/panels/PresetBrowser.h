// Source/UI/panels/PresetBrowser.h
#pragma once
#include <functional>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

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
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override;
    void visibilityChanged() override;

private:
    void loadPresetAt(int rowIndex);
    juce::Rectangle<int> cardBounds() const;
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
    void deleteSelectedPreset();

    /** Visible pack cards in Explore mode. Computed in rebuildPackCards()
     *  from PresetManager::getAllPacks(); updated whenever the row list
     *  rebuilds so card hit-testing stays in sync with rendering. */
    struct PackCard {
        juce::String name;
        juce::String displayName;
        int          presetCount { 0 };
    };
    std::vector<PackCard> packCards;
    int hoverPackCardIdx { -1 };

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
