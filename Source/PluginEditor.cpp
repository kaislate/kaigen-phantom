#include "PluginEditor.h"

PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(900, 550);
}

void PhantomEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0820));
    g.setColour(juce::Colours::white);
    g.setFont(28.0f);
    g.drawFittedText("KAIGEN | PHANTOM", getLocalBounds(), juce::Justification::centred, 1);
}
