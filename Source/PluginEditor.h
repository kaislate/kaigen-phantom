#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/PhantomLookAndFeel.h"
#include "UI/HeaderBar.h"
#include "UI/FooterBar.h"
#include "UI/GlowSeam.h"

class PhantomEditor : public juce::AudioProcessorEditor
{
public:
    explicit PhantomEditor(PhantomProcessor&);
    ~PhantomEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PhantomProcessor& processor;
    PhantomLookAndFeel phantomLnf;

    HeaderBar  headerBar;
    FooterBar  footerBar;
    GlowSeam   headerSeam { GlowSeam::Orientation::Horizontal };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomEditor)
};
