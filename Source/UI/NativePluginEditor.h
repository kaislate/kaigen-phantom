// Source/UI/NativePluginEditor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PhantomLookAndFeel.h"
#include "panels/LeftPanel.h"
#include "panels/RightPanel.h"
#include "panels/TopBar.h"
#include "panels/PresetBrowser.h"
#include "widgets/PresetDropdown.h"
#include "panels/ModulationPanel.h"
#include "panels/MatrixView.h"

class PhantomProcessor;

namespace kaigen::phantom
{

class NativePluginEditor : public juce::AudioProcessorEditor,
                            public juce::ChangeListener
{
public:
    NativePluginEditor(PhantomProcessor& processor, juce::AudioProcessorValueTreeState& apvts);
    ~NativePluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** ChangeListener — fired when PhantomProcessor::engineFocus changes. */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    void applyEngineFocus();        // pushes the current focus to all per-engine widgets


    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    PhantomLookAndFeel lookAndFeel;
    RightPanel rightPanel;
    LeftPanel leftPanel;
    TopBar topBar;
    PresetBrowser presetBrowser;
    PresetDropdown presetDropdown;
    ModulationPanel modulationPanel;
    MatrixView matrixView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativePluginEditor)
};

} // namespace kaigen::phantom
