// Source/UI/panels/SettingsOverlay.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../widgets/WordSelector.h"
#include "../widgets/PhantomKnob.h"
#include "../widgets/EtchedToggle.h"

namespace kaigen::phantom
{

/** Modal settings overlay — native port of the WebView's #settings-overlay
 *  card. Three sections: Binaural (mode + width), Envelope Source, MIDI
 *  Triggering (two toggles). Dismissed via click-on-backdrop or × button.
 *  Lifetime: owned by NativePluginEditor, added with setVisible(false) by
 *  default. */
class SettingsOverlay : public juce::Component
{
public:
    SettingsOverlay(juce::AudioProcessorValueTreeState& apvts);
    ~SettingsOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    /** Per-engine widgets retarget to the active engine's prefix. Called
     *  by NativePluginEditor::applyEngineFocus. */
    void setEnginePrefix(const juce::String& activePrefix,
                         const juce::String& mirrorPrefix = {});

    /** Called when the overlay should be dismissed (close button or
     *  backdrop click). Wires back to the editor's hide logic. */
    std::function<void()> onDismiss;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Card bounds computed in resized(); used by mouseDown to discriminate
    // backdrop clicks from clicks on the card content.
    juce::Rectangle<int> cardBounds;

    // Binaural section.
    WordSelector binauralModeSelector;
    PhantomKnob  binauralWidthKnob;

    // Envelope source section.
    WordSelector envSourceSelector;

    // MIDI triggering section.
    EtchedToggle midiTriggerToggle;
    EtchedToggle midiGateReleaseToggle;

    // Close button.
    juce::TextButton closeButton { "\xC3\x97" };   // UTF-8 ×

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsOverlay)
};

} // namespace kaigen::phantom
