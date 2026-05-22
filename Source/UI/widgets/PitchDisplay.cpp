// Source/UI/widgets/PitchDisplay.cpp
#include "PitchDisplay.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../EngineFocus.h"
#include "../../Parameters.h"
#include <cmath>

namespace kaigen::phantom
{

namespace
{
    constexpr float kCardCorner   = 4.0f;
    constexpr float kFieldPadX    = 8.0f;

    // EMA in log-Hz space (alpha 0.25 ≈ 250 ms settling at 20 Hz polling).
    // Snap when the new reading is more than a semitone (5.9 %) away from
    // the smoothed value — that's a new note, not noisy retracking.
    constexpr float kSmoothAlpha = 0.25f;
    constexpr float kSnapRatio   = 1.0594631f;   // 2^(1/12)

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
        smoothedHz = rawHz;
    }
    else
    {
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

    // Output peak (max of L/R) → dB, rounded to one decimal.
    const float peakL   = processor.peakOutL.load(std::memory_order_relaxed);
    const float peakR   = processor.peakOutR.load(std::memory_order_relaxed);
    const float peak    = juce::jmax(peakL, peakR);
    const float peakDb  = juce::Decibels::gainToDecibels(peak, -60.0f);
    const float roundedDb = std::round(peakDb * 10.0f) * 0.1f;

    // Active engine's recipe preset.
    const auto activeTab = processor.getEngineFocus().activeTab;
    const juce::String recipeId =
        (activeTab == kaigen::phantom::ActiveTab::B)
            ? juce::String(ParamID::B_RECIPE_PRESET)
            : juce::String(ParamID::A_RECIPE_PRESET);
    int recipeIndex = -1;
    juce::String recipeName;
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            processor.apvts.getParameter(recipeId)))
    {
        recipeIndex = choice->getIndex();
        recipeName  = choice->getCurrentChoiceName().toUpperCase();
    }

    // Gate repaints.
    const bool hzSame     = std::abs(smoothedHz - displayedHz) < kRepaintEpsilonHz;
    const bool dbSame     = std::abs(roundedDb  - displayedDb) < 0.05f;
    const bool recipeSame = (recipeIndex == displayedRecipe);
    if (hzSame && dbSame && recipeSame) return;

    displayedHz     = smoothedHz;
    displayedDb     = roundedDb;
    displayedRecipe = recipeIndex;

    // Recompose each field independently.
    if (smoothedHz > 0.0f)
    {
        const int cents = hzToCents(smoothedHz);
        const juce::String centsStr =
            (cents >= 0 ? juce::String("+") : juce::String())
            + juce::String(cents) + juce::String::fromUTF8("\xC2\xA2");   // ¢
        fieldPitch = hzToNoteName(smoothedHz) + " " + centsStr;
        fieldHz    = juce::String(juce::roundToInt(smoothedHz)) + " Hz";
    }
    else
    {
        fieldPitch = "---";
        fieldHz    = "--- Hz";
    }

    fieldRecipe = recipeName.isEmpty() ? juce::String("---") : recipeName;

    if (peak < 1.0e-5f)
        fieldDb = "-inf dB";
    else
        fieldDb = juce::String(roundedDb, 1) + " dB";

    repaint();
}

void PitchDisplay::resized()
{
    cardBounds = getLocalBounds();
    if (cardBounds.isEmpty()) return;

    // Split the card into 4 equal-width fields (left to right):
    // pitch, hz, recipe, dB. Each field is centred-text within its sub-rect
    // so changes to one don't shift the others.
    const int innerLeft  = cardBounds.getX()      + (int) kFieldPadX;
    const int innerRight = cardBounds.getRight()  - (int) kFieldPadX;
    const int innerTop   = cardBounds.getY();
    const int innerH     = cardBounds.getHeight();
    const int innerW     = innerRight - innerLeft;
    const int fieldW     = innerW / 4;

    pitchRect  = { innerLeft,                  innerTop, fieldW, innerH };
    hzRect     = { innerLeft +     fieldW,     innerTop, fieldW, innerH };
    recipeRect = { innerLeft + 2 * fieldW,     innerTop, fieldW, innerH };
    dbRect     = { innerLeft + 3 * fieldW,     innerTop,
                    innerRight - (innerLeft + 3 * fieldW), innerH };
}

void PitchDisplay::paint(juce::Graphics& g)
{
    if (cardBounds.isEmpty()) return;

    // OLED bezel + dark surface (matches the spectrum / oscilloscope bezel).
    Theme::paintVisualizerInset(g, cardBounds, kCardCorner);

    // Faint divider lines between fields.
    g.setColour(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.06f));
    for (auto x : { hzRect.getX(), recipeRect.getX(), dbRect.getX() })
        g.drawLine((float) x, (float) cardBounds.getY() + 4.0f,
                    (float) x, (float) cardBounds.getBottom() - 4.0f, 1.0f);

    // OLED text — Courier centred in each field, sized off card height.
    const float textPx = (float) cardBounds.getHeight() * 0.5f;
    g.setFont(juce::Font(juce::FontOptions()
                             .withName("Courier New")
                             .withHeight(textPx)));
    g.setColour(juce::Colour::fromFloatRGBA(0.9f, 0.95f, 1.0f, 0.92f));

    g.drawText(fieldPitch,  pitchRect.toFloat(),  juce::Justification::centred, false);
    g.drawText(fieldHz,     hzRect.toFloat(),     juce::Justification::centred, false);
    g.drawText(fieldRecipe, recipeRect.toFloat(), juce::Justification::centred, false);
    g.drawText(fieldDb,     dbRect.toFloat(),     juce::Justification::centred, false);
}

} // namespace kaigen::phantom
