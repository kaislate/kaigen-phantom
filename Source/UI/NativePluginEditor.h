// Source/UI/NativePluginEditor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "panels/RightPanel.h"

class PhantomProcessor;

namespace kaigen::phantom
{

class NativePluginEditor : public juce::AudioProcessorEditor,
                           private juce::Button::Listener
{
public:
    NativePluginEditor(PhantomProcessor& processor, juce::AudioProcessorValueTreeState& apvts);
    ~NativePluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* b) override;

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton backToWebViewButton { "<- WebView2" };
    RightPanel rightPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativePluginEditor)
};

} // namespace kaigen::phantom
