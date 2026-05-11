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
    void visibilityChanged() override;

private:
    void loadPresetAt(int rowIndex);
    juce::Rectangle<int> cardBounds() const;
    juce::Rectangle<int> searchBarBounds() const;
    void rebuildRows();          // (re)compute `rows` from preset list + search

    struct Row { juce::String name; juce::String pack; bool isHeader { false }; };
    std::vector<Row> rows;
    int hoverRow { -1 };

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton closeButton;
    juce::TextEditor searchField;

    // Webview spec: card is 90% × 90% of parent (capped). 3-column layout:
    //   [sidebar 160] [middle flex] [preview 180]
    static constexpr int kRowHeight     = 26;
    static constexpr int kHeaderHeight  = 28;
    static constexpr int kSidebarW      = 160;
    static constexpr int kPreviewW      = 180;
    static constexpr int kHeaderBarH    = 44;
    static constexpr int kSearchBarH    = 36;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};

} // namespace kaigen::phantom
