// Source/UI/panels/RightPanel.cpp
#include "RightPanel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        g.setColour(Theme::textSecondary);
        g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::bold));
        g.drawText(title, bounds.toFloat(), juce::Justification::centredLeft, false);
    }
}

RightPanel::RightPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      saturationKnob(apvts, "a_harmonic_saturation", PhantomKnob::Size::Medium, "Saturation"),
      shapeKnob     (apvts, "a_synth_step",          PhantomKnob::Size::Medium, "Shape"),
      skipKnob      (apvts, "a_synth_skip",          PhantomKnob::Size::Medium, "Skip")
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(shapeKnob);
    addAndMakeVisible(skipKnob);
}

RightPanel::~RightPanel() = default;

void RightPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    // Section header above the knob row.
    auto sectionHeader = juce::Rectangle<int>(12, 8, 200, 16);
    drawSectionHeader(g, sectionHeader, "Harmonic Engine");
}

void RightPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(28);  // space for section header

    // Three medium knobs in a row at the top.
    auto knobRow = area.removeFromTop(80).reduced(12, 0);
    const int knobWidth = 80;
    auto layoutKnob = [&](PhantomKnob& k) {
        k.setBounds(knobRow.removeFromLeft(knobWidth));
        knobRow.removeFromLeft(8);
    };
    layoutKnob(saturationKnob);
    layoutKnob(shapeKnob);
    layoutKnob(skipKnob);
}

} // namespace kaigen::phantom
