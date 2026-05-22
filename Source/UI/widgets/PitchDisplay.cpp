// Source/UI/widgets/PitchDisplay.cpp
#include "PitchDisplay.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include <cmath>

namespace kaigen::phantom
{

namespace
{
    // Card sizing.
    constexpr int   kCardSidePad = 24;     // gap from edge of the component
    constexpr float kCardCorner  = 4.0f;

    // Smoothing.
    //
    // Polling at 20 Hz; alpha 0.25 = ~5-tick (≈ 250 ms) settling.
    // Smooth in log-Hz space so jumps blend musically rather than linearly,
    // and snap when the new value is more than a semitone (5.9 %) away from
    // the smoothed value — that's the threshold for "this is a new note",
    // not "this is a noisy reading of the same note."
    constexpr float kSmoothAlpha     = 0.25f;
    constexpr float kSnapSemitones   = 1.0f;
    constexpr float kSnapRatio       = 1.0594631f;  // 2^(1/12)

    // Don't repaint on changes smaller than this (in displayed Hz).
    constexpr float kRepaintEpsilonHz = 0.25f;
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

int PitchDisplay::hzToCents(float hz)
{
    if (hz <= 0.0f) return 0;

    // Distance in semitones from A4, take fractional part, scale to cents.
    const float midiF = 12.0f * std::log2(hz / 440.0f) + 69.0f;
    const float frac  = midiF - std::round(midiF);     // -0.5 .. +0.5
    return juce::roundToInt(frac * 100.0f);
}

void PitchDisplay::timerCallback()
{
    if (! isShowing()) return;

    const float rawHz = processor.currentPitch.load(std::memory_order_relaxed);

    // Smoothing pass.
    if (rawHz <= 0.0f)
    {
        smoothedHz = -1.0f;
    }
    else if (smoothedHz <= 0.0f)
    {
        // First reading after silence — snap.
        smoothedHz = rawHz;
    }
    else
    {
        // If the new reading is more than a semitone away, it's a new note —
        // snap instantly. Otherwise EMA in log-Hz space.
        const float ratio = rawHz / smoothedHz;
        if (ratio > kSnapRatio || ratio < (1.0f / kSnapRatio))
        {
            smoothedHz = rawHz;
        }
        else
        {
            const float logSmooth = std::log(smoothedHz);
            const float logRaw    = std::log(rawHz);
            smoothedHz = std::exp(logSmooth + kSmoothAlpha * (logRaw - logSmooth));
        }
    }

    // Gate repaints to meaningful changes.
    if (std::abs(smoothedHz - displayedHz) < kRepaintEpsilonHz) return;
    displayedHz = smoothedHz;

    if (smoothedHz > 0.0f)
    {
        const int cents = hzToCents(smoothedHz);
        const juce::String centsStr =
            (cents >= 0 ? juce::String("+") : juce::String())
            + juce::String(cents) + juce::String::fromUTF8("\xC2\xA2");   // ¢

        currentText = hzToNoteName(smoothedHz)
                       + "  " + centsStr
                       + juce::String::fromUTF8(" \xC2\xB7 ")             // middle dot
                       + juce::String(juce::roundToInt(smoothedHz))
                       + " Hz";
    }
    else
    {
        currentText = "---";
    }

    repaint();
}

void PitchDisplay::resized()
{
    // Card fills the component horizontally minus side padding; vertically
    // takes the full height.
    cardBounds = getLocalBounds().reduced(kCardSidePad, 0);
}

void PitchDisplay::paint(juce::Graphics& g)
{
    if (cardBounds.isEmpty()) return;

    // OLED bezel — same helper used by spectrum / oscilloscope for visual
    // consistency.
    Theme::paintVisualizerInset(g, cardBounds, kCardCorner);

    // OLED text — large Courier centred on the card.
    const float textPx = (float) cardBounds.getHeight() * 0.55f;
    g.setFont(juce::Font(juce::FontOptions()
                             .withName("Courier New")
                             .withHeight(textPx)));
    g.setColour(juce::Colour::fromFloatRGBA(0.9f, 0.95f, 1.0f, 0.92f));
    g.drawText(currentText, cardBounds.toFloat(),
                juce::Justification::centred, false);
}

} // namespace kaigen::phantom
