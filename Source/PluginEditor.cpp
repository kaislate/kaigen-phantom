#include "PluginEditor.h"
#include "UI/PhantomColours.h"

PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      headerBar(p.apvts),
      harmonicPanel(p.apvts),
      ghostPanel(p.apvts),
      outputPanel(p.apvts)
{
    setLookAndFeel(&phantomLnf);
    setSize(920, 620);

    addAndMakeVisible(headerBar);
    addAndMakeVisible(footerBar);
    addAndMakeVisible(headerSeam);
    addAndMakeVisible(bodyVSeam);
    addAndMakeVisible(row1Seam);
    addAndMakeVisible(harmonicPanel);
    addAndMakeVisible(ghostPanel);
    addAndMakeVisible(outputPanel);
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

    // Body: left recipe wheel area (316px) | seam | right panel
    auto body = area;
    auto leftArea = body.removeFromLeft(316);  // Reserved for RecipeWheelPanel (Phase 4)
    juce::ignoreUnused(leftArea);
    bodyVSeam.setBounds(body.removeFromLeft(3));
    auto rightArea = body;

    // Right panel: Row 1 (knob panels) | seam | rest
    auto row1 = rightArea.removeFromTop(130);
    row1Seam.setBounds(rightArea.removeFromTop(3));

    // Row 1: HarmonicEngine (42%) | Ghost (35%) | Output (rest)
    int heW = (int)(row1.getWidth() * 0.42f);
    int ghW = (int)(row1.getWidth() * 0.35f);

    harmonicPanel.setBounds(row1.removeFromLeft(heW));
    ghostPanel.setBounds(row1.removeFromLeft(ghW));
    outputPanel.setBounds(row1);
}
