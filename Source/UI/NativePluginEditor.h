// Source/UI/NativePluginEditor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PhantomProcessor;  // forward declaration

namespace kaigen::phantom
{

/** Native (non-WebView2) plugin editor for the Phantom plugin.
 *  Phase 0: blank wireframe placeholder. Phase 1+ progressively populates
 *  with native widgets (knobs, visualizers, matrix view, etc.). */
class NativePluginEditor : public juce::AudioProcessorEditor
{
public:
    NativePluginEditor(PhantomProcessor& processor, juce::AudioProcessorValueTreeState& apvts);
    ~NativePluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativePluginEditor)
};

} // namespace kaigen::phantom
