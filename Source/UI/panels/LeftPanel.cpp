// Source/UI/panels/LeftPanel.cpp
#include "LeftPanel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        const auto labelFont = juce::Font(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold))
                                   .withExtraKerningFactor(0.25f);
        Theme::drawEtchedText(g, title.toUpperCase(), bounds, juce::Justification::centredLeft,
                              labelFont, Theme::textOnLightLabel);
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

    // Filter LinkButton wiring: when linked, dragging LPF mirrors HPF and vice versa.
    // Listener installs unconditionally; the listener body checks isLinked() before acting.
    lpfKnob.getSlider().addListener(this);
    hpfKnob.getSlider().addListener(this);
}

LeftPanel::~LeftPanel()
{
    lpfKnob.getSlider().removeListener(this);
    hpfKnob.getSlider().removeListener(this);
}

void LeftPanel::sliderValueChanged(juce::Slider* s)
{
    if (filterLinkUpdating) return;       // guard against recursion
    if (! filterLinkBtn.isLinked()) return;

    juce::ScopedValueSetter<bool> guard(filterLinkUpdating, true);

    // Mirror by NORMALIZED position (their Hz ranges differ).
    if (s == &lpfKnob.getSlider())
    {
        const auto n = lpfKnob.getSlider().getNormalisableRange().convertTo0to1(lpfKnob.getSlider().getValue());
        const auto target = hpfKnob.getSlider().getNormalisableRange().convertFrom0to1(n);
        hpfKnob.getSlider().setValue(target, juce::sendNotificationSync);
    }
    else if (s == &hpfKnob.getSlider())
    {
        const auto n = hpfKnob.getSlider().getNormalisableRange().convertTo0to1(hpfKnob.getSlider().getValue());
        const auto target = lpfKnob.getSlider().getNormalisableRange().convertFrom0to1(n);
        lpfKnob.getSlider().setValue(target, juce::sendNotificationSync);
    }
}

void LeftPanel::paint(juce::Graphics& g)
{
    // Silver panel surface — replaces the flat panelBg fill.
    Theme::paintSilverPanel(g, getLocalBounds());

    // Sub-section inset cards (neumorphic dish beneath each section's controls).
    // Recipe wheel area is excluded — it gets its own treatment in Task 8.
    if (! ghostCardBounds.isEmpty())
        Theme::paintInsetCard(g, ghostCardBounds, 14.0f);
    if (! filterCardBounds.isEmpty())
        Theme::paintInsetCard(g, filterCardBounds, 14.0f);

    // Section header labels (etched text, established in Task 3).
    drawSectionHeader(g, juce::Rectangle<int>(ghostCardBounds.getX() + 4, ghostCardBounds.getY() + 6, 200, 16), "Ghost");
    drawSectionHeader(g, juce::Rectangle<int>(filterCardBounds.getX() + 4, filterCardBounds.getY() + 6, 200, 16), "Filter");
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

    // Compute inset card bounds from the section layouts above.
    // Ghost card: from header row (30px above ghostY) to bottom of toggle (ghostY+110+26) + 8px padding.
    constexpr int cardPadX = 8;
    const int panelW       = getWidth();

    const int ghostCardTop    = ghostY - 30;            // header label sits ~24px above the knobs
    const int ghostCardBottom = ghostY + 110 + 26 + 8;  // bottom of ghostModeToggle + padding
    ghostCardBounds = juce::Rectangle<int>(cardPadX, ghostCardTop,
                                           panelW - cardPadX * 2,
                                           ghostCardBottom - ghostCardTop);

    const int filterCardTop    = filterY - 30;
    const int filterCardBottom = filterY + 110 + 26 + 8;
    filterCardBounds = juce::Rectangle<int>(cardPadX, filterCardTop,
                                             panelW - cardPadX * 2,
                                             filterCardBottom - filterCardTop);
}

} // namespace kaigen::phantom
