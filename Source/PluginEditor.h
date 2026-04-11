#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/PhantomLookAndFeel.h"
#include "UI/HeaderBar.h"
#include "UI/FooterBar.h"
#include "UI/GlowSeam.h"
#include "UI/HarmonicEnginePanel.h"
#include "UI/GhostPanel.h"
#include "UI/OutputPanel.h"
#include "UI/PitchTrackerPanel.h"
#include "UI/SidechainPanel.h"
#include "UI/StereoPanel.h"
#include "UI/DeconflictionPanel.h"
#include "UI/RecipeWheelPanel.h"

class PhantomEditor : public juce::AudioProcessorEditor,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::Timer
{
public:
    explicit PhantomEditor(PhantomProcessor&);
    ~PhantomEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    PhantomProcessor& processor;
    PhantomLookAndFeel phantomLnf;

    HeaderBar  headerBar;
    FooterBar  footerBar;
    GlowSeam   headerSeam { GlowSeam::Orientation::Horizontal };
    GlowSeam   bodyVSeam  { GlowSeam::Orientation::Vertical };
    GlowSeam   row1Seam   { GlowSeam::Orientation::Horizontal };

    HarmonicEnginePanel harmonicPanel;
    GhostPanel          ghostPanel;
    OutputPanel         outputPanel;

    RecipeWheelPanel   recipeWheelPanel;

    GlowSeam           row2Seam { GlowSeam::Orientation::Horizontal };
    PitchTrackerPanel  pitchTrackerPanel;
    SidechainPanel     sidechainPanel;
    StereoPanel        stereoPanel;
    DeconflictionPanel deconflictionPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomEditor)
};
