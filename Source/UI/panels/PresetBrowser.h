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
class PresetBrowser : public juce::Component
{
public:
    PresetBrowser(PhantomProcessor& processor,
                  juce::AudioProcessorValueTreeState& apvts);
    ~PresetBrowser() override;

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
    bool isExploreActive() const noexcept;

    /** Sidebar entries. `Explore` is a special view (pack-card grid in
     *  commit 8); for now treats as "show all presets in list mode".
     *  `Favorites` filters by metadata.isFavorite. Otherwise `packFilter`
     *  matches against PresetMetadata.packName. */
    enum class CategoryKind { Explore, Favorites, Pack };
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
    static constexpr int kRowHeight     = 26;
    static constexpr int kHeaderHeight  = 28;
    static constexpr int kColHeaderH    = 22;
    static constexpr int kSidebarW      = 160;
    static constexpr int kPreviewW      = 180;
    static constexpr int kHeaderBarH    = 44;
    static constexpr int kSearchBarH    = 36;

    // Webview spec: grid-template-columns: 1fr 72px 72px 140px 40px 30px
    // Same widths here; NAME flexes to fill leftover middle-column space.
    static constexpr int kColTypeW      = 72;
    static constexpr int kColDesignerW  = 72;
    static constexpr int kColShapeW     = 140;
    static constexpr int kColSkipW      = 40;
    static constexpr int kColHeartW     = 30;
    static constexpr int kColGap        = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};

} // namespace kaigen::phantom
