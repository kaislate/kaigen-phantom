#include "PluginEditor.h"
#include "UI/PhantomColours.h"

PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      headerBar(p.apvts)
{
    setLookAndFeel(&phantomLnf);
    setSize(920, 620);

    addAndMakeVisible(headerBar);
    addAndMakeVisible(footerBar);
    addAndMakeVisible(headerSeam);
}

PhantomEditor::~PhantomEditor()
{
    setLookAndFeel(nullptr);
}

void PhantomEditor::paint(juce::Graphics& g)
{
    g.fillAll(PhantomColours::background);
}

void PhantomEditor::resized()
{
    auto area = getLocalBounds();

    headerBar.setBounds(area.removeFromTop(50));
    headerSeam.setBounds(area.removeFromTop(3));
    footerBar.setBounds(area.removeFromBottom(38));

    // Remaining area is for the body panel — will be filled in Phase 2+
}
