// Source/UI/panels/SettingsOverlay.cpp
#include "SettingsOverlay.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    constexpr int kCardWidth  = 480;
    constexpr int kCardHeight = 380;

    void drawSectionLabel(juce::Graphics& g, juce::Rectangle<int> bounds,
                          const juce::String& text)
    {
        const auto font = juce::Font(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold))
                              .withExtraKerningFactor(0.25f);
        Theme::drawEtchedText(g, text.toUpperCase(), bounds, juce::Justification::centredLeft,
                              font, Theme::textOnLightLabel);
    }
}

SettingsOverlay::SettingsOverlay(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      binauralModeSelector(apvts, "a_binaural_mode",
                            juce::StringArray{ "Off", "Spread" }),
      binauralWidthKnob   (apvts, "a_binaural_width",
                            PhantomKnob::Size::Medium, "Width"),
      envSourceSelector   (apvts, "a_env_source",
                            juce::StringArray{ "Input", "Sidechain" }),
      midiTriggerToggle   (apvts, "a_midi_trigger_enabled", "Trigger"),
      midiGateReleaseToggle(apvts, "a_midi_gate_release",   "Gate Release")
{
    addAndMakeVisible(binauralModeSelector);
    addAndMakeVisible(binauralWidthKnob);
    addAndMakeVisible(envSourceSelector);
    addAndMakeVisible(midiTriggerToggle);
    addAndMakeVisible(midiGateReleaseToggle);

    closeButton.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    closeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    closeButton.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff656769));
    closeButton.setColour(juce::TextButton::textColourOnId,   juce::Colour(0xff656769));
    closeButton.onClick = [this] { if (onDismiss) onDismiss(); };
    addAndMakeVisible(closeButton);
}

SettingsOverlay::~SettingsOverlay() = default;

void SettingsOverlay::setEnginePrefix(const juce::String& activePrefix,
                                      const juce::String& mirrorPrefix)
{
    binauralModeSelector.setEnginePrefix(activePrefix, mirrorPrefix);
    binauralWidthKnob   .setEnginePrefix(activePrefix, mirrorPrefix);
    envSourceSelector   .setEnginePrefix(activePrefix, mirrorPrefix);
    // EtchedToggle has no setEnginePrefix — the param it binds to is per-
    // engine but rebinding requires reconstructing the attachment. For now
    // both MIDI toggles stay bound to a_*. (Same status quo as the rest of
    // the editor's toggle widgets — addressed in a future sub-commit.)
}

void SettingsOverlay::paint(juce::Graphics& g)
{
    // Backdrop — dimmed full-component.
    g.fillAll(juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.40f));

    // Card surface — neumorphic light plastic with subtle shadow.
    if (cardBounds.isEmpty()) return;

    juce::Path cardShadow;
    cardShadow.addRoundedRectangle(cardBounds.toFloat(), 8.0f);
    juce::DropShadow(juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.25f), 18, { 0, 6 })
        .drawForPath(g, cardShadow);

    juce::ColourGradient grad(juce::Colour(0xffBBBDBF), cardBounds.toFloat().getTopLeft(),
                              juce::Colour(0xffAEAFB1), cardBounds.toFloat().getBottomRight(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(cardBounds.toFloat(), 8.0f);
    g.setColour(juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.15f));
    g.drawRoundedRectangle(cardBounds.toFloat(), 8.0f, 1.0f);

    // Header — "SETTINGS" etched label.
    const auto headerBounds = cardBounds.withHeight(28).reduced(16, 4);
    {
        const auto headerFont = juce::Font(juce::FontOptions("Space Grotesk", 13.0f, juce::Font::bold))
                                    .withExtraKerningFactor(0.30f);
        Theme::drawEtchedText(g, "SETTINGS", headerBounds, juce::Justification::centredLeft,
                              headerFont, Theme::textOnLightLabel);
    }

    // Section labels above each row (computed positions match resized()).
    const int colX     = cardBounds.getX() + 24;
    const int colW     = cardBounds.getWidth() - 48;
    int       sectionY = cardBounds.getY() + 40;
    constexpr int kSectionGap = 12;
    constexpr int kLabelH     = 14;
    constexpr int kRowH       = 56;

    drawSectionLabel(g, { colX, sectionY, colW, kLabelH }, "BINAURAL");
    sectionY += kLabelH + kRowH + kSectionGap;

    drawSectionLabel(g, { colX, sectionY, colW, kLabelH }, "ENVELOPE SOURCE");
    sectionY += kLabelH + kRowH + kSectionGap;

    drawSectionLabel(g, { colX, sectionY, colW, kLabelH }, "MIDI TRIGGERING");
}

void SettingsOverlay::resized()
{
    // Centre the card in the overlay.
    const int x = (getWidth()  - kCardWidth)  / 2;
    const int y = (getHeight() - kCardHeight) / 2;
    cardBounds = { x, y, kCardWidth, kCardHeight };

    // Close button — top-right of card.
    constexpr int kCloseSize = 22;
    closeButton.setBounds(cardBounds.getRight() - kCloseSize - 8,
                          cardBounds.getY() + 6,
                          kCloseSize, kCloseSize);

    const int colX = cardBounds.getX() + 24;
    const int colW = cardBounds.getWidth() - 48;
    int       sectionY = cardBounds.getY() + 40;
    constexpr int kLabelH     = 14;
    constexpr int kRowH       = 56;
    constexpr int kSectionGap = 12;

    // Binaural row: mode selector on the left, width knob on the right.
    {
        sectionY += kLabelH + 2;
        const int knobW = 136;   // PhantomKnob Medium natural width
        const int knobH = 136;
        binauralModeSelector.setBounds(colX, sectionY + (kRowH - 18) / 2,
                                        colW - knobW - 16, 18);
        // Knob is taller than the row — let it overflow vertically; visually OK because
        // section spacing accounts for the shadow halo.
        binauralWidthKnob.setBounds(cardBounds.getRight() - 24 - knobW,
                                     sectionY + (kRowH - knobH) / 2,
                                     knobW, knobH);
        sectionY += kRowH + kSectionGap;
    }

    // Envelope source row: selector only.
    {
        sectionY += kLabelH + 2;
        envSourceSelector.setBounds(colX, sectionY + (kRowH - 18) / 2, colW, 18);
        sectionY += kRowH + kSectionGap;
    }

    // MIDI triggering row: two toggles side by side.
    {
        sectionY += kLabelH + 2;
        constexpr int kToggleW = 100;
        constexpr int kToggleH = 18;
        midiTriggerToggle    .setBounds(colX,
                                         sectionY + (kRowH - kToggleH) / 2,
                                         kToggleW, kToggleH);
        midiGateReleaseToggle.setBounds(colX + kToggleW + 24,
                                         sectionY + (kRowH - kToggleH) / 2,
                                         kToggleW + 20, kToggleH);
    }
}

void SettingsOverlay::mouseDown(const juce::MouseEvent& e)
{
    // Click outside the card = dismiss.
    if (! cardBounds.contains(e.getPosition()))
    {
        if (onDismiss) onDismiss();
    }
}

} // namespace kaigen::phantom
