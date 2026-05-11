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
        Theme::drawEtchedText(g, title.toUpperCase(), bounds, juce::Justification::centred,
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
      recipePresetSelector(apvts, "a_recipe_preset",
                           { "Warm", "Aggr", "Hollow",
                             "Dense", "Stable", "Weird",
                             "Cust 1", "Cust 2", "Cust 3" },
                           /*numRows*/ 3),
      ghostAmountKnob  (apvts, "a_ghost",              PhantomKnob::Size::Large,  "Amount"),
      crossoverKnob    (apvts, "a_phantom_threshold",  PhantomKnob::Size::Medium, "Crossover"),
      strengthKnob     (apvts, "a_phantom_strength",   PhantomKnob::Size::Medium, "Strength"),
      ghostModeToggle  (apvts, "a_ghost_mode", { "Replace", "Combine", "Phantom Only" }),
      lpfKnob          (apvts, "a_synth_lpf_hz",       PhantomKnob::Size::Medium, "LPF"),
      hpfKnob          (apvts, "a_synth_hpf_hz",       PhantomKnob::Size::Medium, "HPF"),
      filterSlopeToggle(apvts, "a_synth_filter_slope", { "-6 dB/oct", "-12 dB/oct", "-24 dB/oct" })
{
    addAndMakeVisible(recipeWheel);
    addAndMakeVisible(recipePresetSelector);
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

    // Sub-section inset cards with title-notch at top center. Notch width
    // sized to fully encompass the title text (with breathing room).
    if (! recipeCardBounds.isEmpty())
        Theme::paintInsetCardWithNotch(g, recipeCardBounds, 14.0f, 130.0f, 16.0f);
    if (! ghostCardBounds.isEmpty())
        Theme::paintInsetCardWithNotch(g, ghostCardBounds, 14.0f, 130.0f, 16.0f);
    if (! filterCardBounds.isEmpty())
        Theme::paintInsetCardWithNotch(g, filterCardBounds, 14.0f, 130.0f, 16.0f);

    // Section titles — centered in the flat-bottomed notch (vertical centre
    // of the dip is at cardY + dipDepth/2 = cardY + 8).
    drawSectionHeader(g, juce::Rectangle<int>(recipeCardBounds.getX(), recipeCardBounds.getY() + 1, recipeCardBounds.getWidth(), 14), "Recipe");
    drawSectionHeader(g, juce::Rectangle<int>(ghostCardBounds.getX(),  ghostCardBounds.getY() + 1, ghostCardBounds.getWidth(),  14), "Ghost");
    drawSectionHeader(g, juce::Rectangle<int>(filterCardBounds.getX(), filterCardBounds.getY() + 1, filterCardBounds.getWidth(), 14), "Filter");

    // ── H2..H8 spoke labels (etched in the silver around the wheel) ───
    // The wheel component is square at the top of the panel; we paint the
    // labels in the silver area just outside the wheel's circumference.
    if (! recipeWheel.getBounds().isEmpty())
    {
        const auto wb = recipeWheel.getBounds().toFloat();
        const auto wcentre = wb.getCentre();
        const float wRadius = juce::jmin(wb.getWidth(), wb.getHeight()) * 0.5f - 4.0f;
        const float labelR  = wRadius + 14.0f;       // just outside the dark wheel
        const auto labelFont = juce::Font(juce::FontOptions("Courier New", 9.0f, juce::Font::bold))
                                    .withExtraKerningFactor(0.05f);
        for (int i = 0; i < 7; ++i)
        {
            // Spoke 0 points up (-π/2), 7 spokes evenly spaced.
            const float a = (float) i * juce::MathConstants<float>::twoPi / 7.0f
                                - juce::MathConstants<float>::halfPi;
            const float lx = wcentre.x + labelR * std::cos(a);
            const float ly = wcentre.y + labelR * std::sin(a);
            const juce::String hText = "H" + juce::String(i + 2);
            const juce::Rectangle<int> labelBounds((int) lx - 14, (int) ly - 8, 28, 14);
            Theme::drawEtchedText(g, hText, labelBounds, juce::Justification::centred,
                                   labelFont, Theme::textOnLightLabel);
        }
    }
}

void LeftPanel::resized()
{
    const int panelW = getWidth();

    // Recipe wheel: square aspect so the radial gradients render circularly.
    constexpr int kWheelTop  = 8;
    constexpr int kWheelSize = 320;
    const int wheelX = (panelW - kWheelSize) / 2;
    recipeWheel.setBounds(wheelX, kWheelTop, kWheelSize, kWheelSize);

    // Recipe preset selector — wrapped in its own inset card ("tray") to
    // visually match the Ghost / Filter sections below. The card's content
    // inset matches the Ghost / Filter pattern (8 px above the content,
    // 8 px below).
    constexpr int kPresetRowH    = 22;
    constexpr int kPresetH       = kPresetRowH * 3;
    constexpr int kRecipeCardTop  = 328;   // touches bottom of wheel
    constexpr int kRecipeContentY = kRecipeCardTop + 8;
    recipePresetSelector.setBounds(16, kRecipeContentY, panelW - 32, kPresetH);

    // Knob component natural sizes (body + shadow padding × 2).
    //   Large:  114 + 32*2 = 178
    //   Medium:  88 + 24*2 = 136
    constexpr int kLarge  = 178;
    constexpr int kMedium = 136;

    // ── Ghost section ──────────────────────────────────────────────────
    // Pushed down a hair to clear the new recipe preset card below the wheel.
    constexpr int ghostY  = 426;   // was 420 (+6 to clear the recipe tray below)
    const int ghostTotal  = kLarge + kMedium + kMedium;
    const int ghostOverlap = (ghostTotal - panelW + 16) / 2;
    int gx = 8;
    ghostAmountKnob.setBounds(gx, ghostY, kLarge, kLarge);
    gx += kLarge - ghostOverlap;
    crossoverKnob.setBounds(gx, ghostY, kMedium, kMedium);
    gx += kMedium - ghostOverlap;
    strengthKnob.setBounds(gx, ghostY, kMedium, kMedium);

    // Ghost mode words (Replace / Combine / Phantom Only) — centered across
    // the full Ghost section (matches Filter dB/oct layout below).
    ghostModeToggle.setBounds(12, ghostY + kLarge + 4, panelW - 24, 22);

    // ── Filter section (more vertical gap from Ghost) ──────────────────
    constexpr int filterY = 696;   // tracks the bumped Ghost Y (was 690, +6)
    const int filterTotal = kMedium + 40 + kMedium;
    int fx = (panelW - filterTotal) / 2;
    lpfKnob.setBounds(fx, filterY, kMedium, kMedium);
    filterLinkBtn.setBounds(fx + kMedium + 6, filterY + (kMedium - 28) / 2, 28, 28);
    hpfKnob.setBounds(fx + kMedium + 40, filterY, kMedium, kMedium);

    filterSlopeToggle.setBounds(12, filterY + kMedium + 4, panelW - 24, 22);

    // ── Card bounds — title sits IN the notch above the card top ──────
    constexpr int cardPadX = 8;

    // Recipe card wraps the preset selector below the wheel.
    const int recipeCardBottom = kRecipeContentY + kPresetH + 8;
    recipeCardBounds = juce::Rectangle<int>(cardPadX, kRecipeCardTop,
                                             panelW - cardPadX * 2,
                                             recipeCardBottom - kRecipeCardTop);

    const int ghostCardTop    = ghostY - 8;    // small inset above knobs
    const int ghostCardBottom = ghostY + kLarge + 4 + 26 + 8;
    ghostCardBounds = juce::Rectangle<int>(cardPadX, ghostCardTop,
                                           panelW - cardPadX * 2,
                                           ghostCardBottom - ghostCardTop);

    const int filterCardTop    = filterY - 8;
    const int filterCardBottom = filterY + kMedium + 4 + 22 + 8;
    filterCardBounds = juce::Rectangle<int>(cardPadX, filterCardTop,
                                             panelW - cardPadX * 2,
                                             filterCardBottom - filterCardTop);
}

} // namespace kaigen::phantom
