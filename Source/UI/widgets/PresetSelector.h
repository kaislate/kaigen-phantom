// Source/UI/widgets/PresetSelector.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Compact preset selector for the TopBar.
 *  - Center: current preset name (text label)
 *  - Left: prev / next buttons (iterate flattened preset list)
 *  - Right: Browse button (opens PresetBrowser overlay) + Save button
 *           (opens juce::AlertWindow text-input dialog)
 *
 *  Owns the `currentPresetName` and `currentPresetPack` state — the C++
 *  PresetManager doesn't track which preset is currently loaded. */
class PresetSelector : public juce::Component
{
public:
    PresetSelector(PhantomProcessor& processor,
                   juce::AudioProcessorValueTreeState& apvts);
    ~PresetSelector() override;

    /** Called when the user clicks "Browse". NativePluginEditor wires this
     *  to show its PresetBrowser overlay. */
    std::function<void()> onBrowseRequested;

    /** Updates the displayed preset name. Called by PresetBrowser when the
     *  user picks a preset there. */
    void setCurrentPreset(const juce::String& name, const juce::String& pack);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void prevPreset();
    void nextPreset();
    void saveDialog();

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::String currentPresetName;
    juce::String currentPresetPack;

    juce::TextButton prevButton  { "<"     };
    juce::TextButton nextButton  { ">"     };
    juce::TextButton browseButton{ "Browse" };
    juce::TextButton saveButton  { "Save"   };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetSelector)
};

} // namespace kaigen::phantom
