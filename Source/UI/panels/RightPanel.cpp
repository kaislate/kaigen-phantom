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
      skipKnob      (apvts, "a_synth_skip",          PhantomKnob::Size::Medium, "Skip"),
      widthKnob     (apvts, "a_stereo_width",        PhantomKnob::Size::Medium, "Width"),
      inGainKnob    (apvts, "input_gain",            PhantomKnob::Size::Medium, "In"),
      outGainKnob   (apvts, "a_output_gain",         PhantomKnob::Size::Medium, "Out")
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(shapeKnob);
    addAndMakeVisible(skipKnob);
    addAndMakeVisible(widthKnob);
    addAndMakeVisible(inGainKnob);
    addAndMakeVisible(outGainKnob);
}

RightPanel::~RightPanel() = default;

void RightPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    drawSectionHeader(g, juce::Rectangle<int>(12,   8, 200, 16), "Harmonic Engine");
    drawSectionHeader(g, juce::Rectangle<int>(310,  8, 100, 16), "Stereo");
    drawSectionHeader(g, juce::Rectangle<int>(420,  8, 200, 16), "Levels");
}

void RightPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(28);

    auto knobRow = area.removeFromTop(80).reduced(12, 0);
    const int knobWidth = 80;
    const int gap       = 8;
    const int sectionGap = 24;

    auto layoutKnob = [&](PhantomKnob& k) {
        k.setBounds(knobRow.removeFromLeft(knobWidth));
        knobRow.removeFromLeft(gap);
    };

    // Harmonic Engine
    layoutKnob(saturationKnob);
    layoutKnob(shapeKnob);
    layoutKnob(skipKnob);

    knobRow.removeFromLeft(sectionGap);

    // Stereo
    layoutKnob(widthKnob);

    knobRow.removeFromLeft(sectionGap);

    // Levels
    layoutKnob(inGainKnob);
    layoutKnob(outGainKnob);
}

} // namespace kaigen::phantom
