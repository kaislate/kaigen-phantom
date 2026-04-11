#include "PluginEditor.h"
#include "UI/PhantomColours.h"
#include "Parameters.h"

PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      headerBar(p.apvts),
      recipeWheelPanel(p.apvts),
      harmonicPanel(p.apvts),
      ghostPanel(p.apvts),
      outputPanel(p.apvts),
      pitchTrackerPanel(p.apvts),
      sidechainPanel(p.apvts),
      stereoPanel(p.apvts),
      deconflictionPanel(p.apvts)
{
    setLookAndFeel(&phantomLnf);
    setSize(920, 620);

    addAndMakeVisible(recipeWheelPanel);
    addAndMakeVisible(headerBar);
    addAndMakeVisible(footerBar);
    addAndMakeVisible(headerSeam);
    addAndMakeVisible(bodyVSeam);
    addAndMakeVisible(row1Seam);
    addAndMakeVisible(harmonicPanel);
    addAndMakeVisible(ghostPanel);
    addAndMakeVisible(outputPanel);
    addAndMakeVisible(row2Seam);
    addAndMakeVisible(pitchTrackerPanel);
    addAndMakeVisible(sidechainPanel);
    addAndMakeVisible(stereoPanel);
    addAndMakeVisible(deconflictionPanel);

    deconflictionPanel.setVisible(false);   // Effect mode is default

    processor.apvts.addParameterListener(ParamID::MODE, this);

    startTimerHz(30);
}

PhantomEditor::~PhantomEditor()
{
    stopTimer();
    processor.apvts.removeParameterListener(ParamID::MODE, this);
    setLookAndFeel(nullptr);
}

void PhantomEditor::timerCallback()
{
    recipeWheelPanel.updatePitch(processor.currentPitch.load(std::memory_order_relaxed));
}

void PhantomEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::MODE)
    {
        bool isEffect = (newValue < 0.5f);
        juce::MessageManager::callAsync([this, isEffect]()
        {
            pitchTrackerPanel.setVisible(isEffect);
            deconflictionPanel.setVisible(!isEffect);
            resized();
        });
    }
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
    auto leftArea = body.removeFromLeft(316);
    recipeWheelPanel.setBounds(leftArea);
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

    // Row 2 (mode-switched)
    auto row2 = rightArea.removeFromTop(130);
    row2Seam.setBounds(rightArea.removeFromTop(3));

    // First panel: PitchTracker (Effect) or Deconfliction (Instrument) — same bounds
    auto row2Left = row2.removeFromLeft((int)(row2.getWidth() * 0.30f));
    pitchTrackerPanel.setBounds(row2Left);
    deconflictionPanel.setBounds(row2Left);

    // Sidechain
    auto row2Mid = row2.removeFromLeft((int)(row2.getWidth() * 0.58f));
    sidechainPanel.setBounds(row2Mid);

    // Stereo
    stereoPanel.setBounds(row2);
}
