// Source/UI/widgets/PresetDropdown.h
#pragma once
#include <functional>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Compact Arturia-style preset picker that drops down from the top-bar
 *  preset pill. 420 × 320, two-column layout (140 px categories sidebar +
 *  flex preset list). Click outside or pick a preset to dismiss.
 *
 *  Lives as a full-editor overlay so the card can be positioned anywhere
 *  without being clipped by panel boundaries; only the card paints, the
 *  backdrop is fully transparent (no scrim — distinct from the full
 *  PresetBrowser modal).
 *
 *  Usage:
 *    presetDropdown.setBoundsAnchoredBelow(pillScreenBounds);
 *    presetDropdown.setVisible(true);
 */
class PresetDropdown : public juce::Component,
                        public juce::ChangeListener
{
public:
    PresetDropdown(PhantomProcessor& processor,
                   juce::AudioProcessorValueTreeState& apvts);
    ~PresetDropdown() override;

    /** Re-fetch the preset list when PresetManager broadcasts a change. */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    /** Fires when a preset row is clicked: (name, pack). */
    std::function<void(juce::String, juce::String)> onPresetSelected;

    /** Position the dropdown card so it sits just under `anchorInEditor`,
     *  horizontally centered on the anchor. The component itself fills the
     *  whole editor; this just sets the cardBounds inside it. */
    void anchorBelow(juce::Rectangle<int> anchorInEditor);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void visibilityChanged() override;

private:
    void rebuildList();
    void applyCategoryFilter();
    void loadPresetAt(int filteredIdx);

    /** Sidebar entry. Filtering happens by `kind`:
     *    All        — every preset.
     *    Favorites  — entries where metadata.isFavorite.
     *    Type       — entries where metadata.type matches `filter`.
     *    Pack       — entries where pack matches `filter`. */
    enum class CategoryKind { All, Favorites, Type, Pack };
    struct Category
    {
        CategoryKind kind { CategoryKind::All };
        juce::String label;      // displayed
        juce::String filter;     // type or pack name; empty for All / Favorites
        int          count { 0 };
    };
    struct PresetEntry
    {
        juce::String name;
        juce::String pack;
        juce::String type;
        bool         isFavorite { false };
    };

    std::vector<Category>    categories;
    std::vector<PresetEntry> allPresets;
    std::vector<PresetEntry> filteredPresets;

    int activeCategoryIdx { 0 };
    int hoverCategoryIdx  { -1 };
    int hoverPresetIdx    { -1 };

    juce::Rectangle<int> cardBounds;

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    static constexpr int kCardW       = 420;
    static constexpr int kCardH       = 320;
    static constexpr int kSidebarW    = 140;
    static constexpr int kRowH        = 22;
    static constexpr int kCatRowH     = 22;
    static constexpr int kAnchorGap   = 4;     // space between anchor + card

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetDropdown)
};

} // namespace kaigen::phantom
