// Source/UI/NativePluginEditor.cpp
#include "NativePluginEditor.h"
#include "../PluginProcessor.h"
#include "Theme.h"

namespace kaigen::phantom
{

NativePluginEditor::NativePluginEditor(PhantomProcessor& p,
                                       juce::AudioProcessorValueTreeState& a)
    : juce::AudioProcessorEditor(&p), processor(p), apvts(a)
{
    setSize(1300, 970);
}

NativePluginEditor::~NativePluginEditor() = default;

void NativePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::deepBg);
}

void NativePluginEditor::resized()
{
    // Phase 7+ will lay out child components here.
}

} // namespace kaigen::phantom
