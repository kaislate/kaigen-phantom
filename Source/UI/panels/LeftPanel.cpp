// Source/UI/panels/LeftPanel.cpp
#include "LeftPanel.h"
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

LeftPanel::LeftPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      recipeWheel(apvts,
                  std::array<juce::String, 7>{
                      "a_recipe_h2", "a_recipe_h3", "a_recipe_h4",
                      "a_recipe_h5", "a_recipe_h6", "a_recipe_h7", "a_recipe_h8"
                  }),
      ghostAmountKnob  (apvts, "a_ghost",              PhantomKnob::Size::Large,  "Amount"),
      crossoverKnob    (apvts, "a_phantom_threshold",  PhantomKnob::Size::Medium, "Crossover"),
      strengthKnob     (apvts, "a_phantom_strength",   PhantomKnob::Size::Medium, "Strength"),
      ghostModeToggle  (apvts, "a_ghost_mode", { "Replace", "Combine", "Phantom Only" }),
      lpfKnob          (apvts, "a_synth_lpf_hz",       PhantomKnob::Size::Medium, "LPF"),
      hpfKnob          (apvts, "a_synth_hpf_hz",       PhantomKnob::Size::Medium, "HPF"),
      filterSlopeToggle(apvts, "a_synth_filter_slope", { "-6 dB/oct", "-12 dB/oct", "-24 dB/oct" })
{
    addAndMakeVisible(recipeWheel);
    addAndMakeVisible(ghostAmountKnob);
    addAndMakeVisible(crossoverKnob);
    addAndMakeVisible(strengthKnob);
    addAndMakeVisible(ghostModeToggle);

    addAndMakeVisible(lpfKnob);
    addAndMakeVisible(hpfKnob);
    addAndMakeVisible(filterLinkBtn);
    addAndMakeVisible(filterSlopeToggle);
}

LeftPanel::~LeftPanel() = default;

void LeftPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    // Ghost section header
    drawSectionHeader(g, juce::Rectangle<int>(12, 376, 200, 16), "Ghost");

    // Filter section header -- below ghost section.
    drawSectionHeader(g, juce::Rectangle<int>(12, 550, 200, 16), "Filter");
}

void LeftPanel::resized()
{
    recipeWheel.setBounds(8, 8, getWidth() - 16, 360);

    constexpr int ghostY = 400;
    ghostAmountKnob.setBounds(12,  ghostY, 90, 100);
    crossoverKnob  .setBounds(110, ghostY, 80, 100);
    strengthKnob   .setBounds(200, ghostY, 80, 100);
    ghostModeToggle.setBounds(12, ghostY + 110, 270, 26);

    constexpr int filterY = 574;
    lpfKnob.setBounds          (12,  filterY,       80, 100);
    filterLinkBtn.setBounds    (98,  filterY + 30,  28, 28);
    hpfKnob.setBounds          (130, filterY,       80, 100);
    filterSlopeToggle.setBounds(12,  filterY + 110, 200, 26);
}

} // namespace kaigen::phantom
