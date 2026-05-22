// Source/UI/widgets/PitchDisplay.cpp
#include "PitchDisplay.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include <cmath>

namespace kaigen::phantom
{

namespace
{
    constexpr float kCardHeight = 36.0f;
    constexpr float kCardWidth  = 180.0f;
    constexpr float kLabelH     = 11.0f;
    constexpr float kLabelGap   = 2.0f;
    constexpr float kEpsilonHz  = 0.5f;   // changes below this don't repaint
}

PitchDisplay::PitchDisplay(PhantomProcessor& p)
    : processor(p)
{
    startTimerHz(20);
}

PitchDisplay::~PitchDisplay()
{
    stopTimer();
}

juce::String PitchDisplay::hzToNoteName(float hz)
{
    if (hz <= 0.0f) return {};

    // MIDI note = 12 * log2(hz / 440) + 69 → A4 = MIDI 69.
    const float midiF = 12.0f * std::log2(hz / 440.0f) + 69.0f;
    const int   midi  = juce::roundToInt(midiF);

    static const char* kNames[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int noteIndex = ((midi % 12) + 12) % 12;
    const int octave    = (midi / 12) - 1;
    return juce::String(kNames[noteIndex]) + juce::String(octave);
}

void PitchDisplay::timerCallback()
{
    if (! isShowing()) return;

    const float hz = processor.currentPitch.load(std::memory_order_relaxed);
    if (std::abs(hz - lastHz) < kEpsilonHz) return;
    lastHz = hz;

    if (hz > 0.0f)
        currentText = hzToNoteName(hz)
                       + juce::String::fromUTF8(" \xC2\xB7 ")   // middle dot
                       + juce::String(juce::roundToInt(hz))
                       + "Hz";
    else
        currentText = "---";

    repaint();
}

void PitchDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Centre an OLED card horizontally; sit it flush at the top.
    const float cardX = std::round((bounds.getWidth() - kCardWidth) * 0.5f);
    const float cardY = 0.0f;
    const juce::Rectangle<float> cardF(cardX, cardY, kCardWidth, kCardHeight);

    // Reuse the visualiser inset paint helper for the OLED surface so the
    // card matches the spectrum / oscilloscope bezel exactly.
    Theme::paintVisualizerInset(g, cardF.toNearestInt(), 4.0f);

    // OLED text — large Courier centred on the card.
    const float textPx = kCardHeight * 0.55f;
    g.setFont(juce::Font(juce::FontOptions()
                             .withName("Courier New")
                             .withHeight(textPx)));
    g.setColour(juce::Colour::fromFloatRGBA(0.9f, 0.95f, 1.0f, 0.92f));
    g.drawText(currentText, cardF, juce::Justification::centred, false);

    // "FUND" etched label below the card.
    const juce::Rectangle<int> labelBounds(
        (int) cardX,
        (int) std::round(cardY + kCardHeight + kLabelGap),
        (int) kCardWidth,
        (int) kLabelH);

    const auto labelFont = juce::Font(juce::FontOptions("Space Grotesk", 9.0f, juce::Font::bold))
                                .withExtraKerningFactor(0.30f);
    Theme::drawEtchedText(g, "FUND", labelBounds, juce::Justification::centred,
                           labelFont, Theme::textOnLightLabel);
}

} // namespace kaigen::phantom
