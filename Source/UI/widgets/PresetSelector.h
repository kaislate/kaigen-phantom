// Source/UI/widgets/PresetSelector.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Top-bar preset selector — webview-faithful layout.
 *
 *  Layout (left to right):
 *    [|||]  glyph button — opens preset browser
 *    [♡  Preset Name  *]  glass pill — clickable opens browser; heart/asterisk
 *                          are visual placeholders until favorites/modified
 *                          tracking is wired
 *    [▲]   prev preset
 *    [▼]   next preset
 *    [💾]  save
 *
 *  Owns the `currentPresetName` and `currentPresetPack` state — the C++
 *  PresetManager doesn't track which preset is currently loaded. */
class PresetSelector : public juce::Component,
                        private juce::ValueTree::Listener,
                        private juce::ChangeListener
{
public:
    PresetSelector(PhantomProcessor& processor,
                   juce::AudioProcessorValueTreeState& apvts);
    ~PresetSelector() override;

    /** Called when the user clicks the |||  library glyph. NativePluginEditor
     *  wires this to show the full PresetBrowser modal. */
    std::function<void()> onBrowseRequested;

    /** Called when the user clicks the preset name pill. NativePluginEditor
     *  wires this to show the compact Arturia-style PresetDropdown anchored
     *  beneath the pill. The callback receives the pill's bounds in
     *  PresetSelector-local coordinates so the editor can position the
     *  dropdown correctly. */
    std::function<void(juce::Rectangle<int> pillBoundsLocal)> onQuickPickRequested;

    /** Updates the displayed preset name. Called by PresetBrowser when the
     *  user picks a preset there. */
    void setCurrentPreset(const juce::String& name, const juce::String& pack);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void prevPreset();
    void nextPreset();
    void saveDialog();

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::String currentPresetName;
    juce::String currentPresetPack;
    bool         currentPresetModified { false };   // any APVTS change since last load/save

    // ValueTree::Listener — fires when any APVTS param changes. Sets the
    // modified flag and triggers a repaint so the red asterisk appears.
    void valueTreePropertyChanged(juce::ValueTree& tree,
                                   const juce::Identifier& property) override;

    // ChangeListener — fires when PresetManager state changes (favorite
    // toggled, new file watched in). Refreshes heart glyph + repaints.
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void refreshHeartGlyph();   // updates heartButton text from isFavorite

    // Glyph buttons — etched, no background (phantom-style "header-glyph").
    // Glyph text assigned in the ctor body so we can use UTF-8 for the unicode
    // characters without depending on source-file encoding.
    juce::TextButton libraryButton;   // |||
    juce::TextButton heartButton;     // ♡
    juce::TextButton prevButton;      // ▲
    juce::TextButton nextButton;      // ▼
    juce::TextButton saveButton;      // 💾

    // Computed in resized(), used in paint() for the pill background and in
    // mouseDown() to decide whether a click on the parent's empty space
    // should open the browser.
    juce::Rectangle<int> pillBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetSelector)
};

} // namespace kaigen::phantom
