// Source/UI/widgets/LevelReadout.cpp
#include "LevelReadout.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include <cmath>

namespace kaigen::phantom
{

namespace
{
    constexpr float kCardCorner = 4.0f;
    constexpr int   kPadX       = 6;
}

LevelReadout::LevelReadout(PhantomProcessor& p)
    : processor(p)
{
    startTimerHz(20);
}

LevelReadout::~LevelReadout()
{
    stopTimer();
}

juce::String LevelReadout::dbToText(float linearPeak)
{
    if (linearPeak < 1.0e-5f) return "-inf";
    const float db = juce::Decibels::gainToDecibels(linearPeak, -60.0f);
    return juce::String(std::round(db * 10.0f) * 0.1f, 1);
}

void LevelReadout::timerCallback()
{
    if (! isShowing()) return;

    const float inL  = processor.peakInL .load(std::memory_order_relaxed);
    const float inR  = processor.peakInR .load(std::memory_order_relaxed);
    const float outL = processor.peakOutL.load(std::memory_order_relaxed);
    const float outR = processor.peakOutR.load(std::memory_order_relaxed);

    const float inPeak  = juce::jmax(inL,  inR);
    const float outPeak = juce::jmax(outL, outR);

    const float inDb  = (inPeak  < 1.0e-5f) ? -200.0f : juce::Decibels::gainToDecibels(inPeak,  -60.0f);
    const float outDb = (outPeak < 1.0e-5f) ? -200.0f : juce::Decibels::gainToDecibels(outPeak, -60.0f);

    const bool inSame  = std::abs(inDb  - displayedInDb)  < 0.05f;
    const bool outSame = std::abs(outDb - displayedOutDb) < 0.05f;
    if (inSame && outSame) return;

    displayedInDb  = inDb;
    displayedOutDb = outDb;
    inText  = dbToText(inPeak);
    outText = dbToText(outPeak);
    repaint();
}

void LevelReadout::resized()
{
    cardBounds = getLocalBounds();
    if (cardBounds.isEmpty()) return;

    const int innerX = cardBounds.getX() + kPadX;
    const int innerW = cardBounds.getWidth() - 2 * kPadX;
    const int innerY = cardBounds.getY() + 2;
    const int innerH = cardBounds.getHeight() - 4;
    const int rowH   = innerH / 2;

    // Label column is fixed-width to keep numbers in a stable position.
    constexpr int kLabelW = 24;
    inLabelRect  = { innerX,           innerY,        kLabelW,         rowH };
    inValueRect  = { innerX + kLabelW, innerY,        innerW - kLabelW, rowH };
    outLabelRect = { innerX,           innerY + rowH, kLabelW,         rowH };
    outValueRect = { innerX + kLabelW, innerY + rowH, innerW - kLabelW, rowH };
}

void LevelReadout::paint(juce::Graphics& g)
{
    if (cardBounds.isEmpty()) return;

    Theme::paintVisualizerInset(g, cardBounds, kCardCorner);

    // "IN" / "OUT" etched labels (Space Grotesk, small caps).
    const auto labelFont = juce::Font(juce::FontOptions("Space Grotesk", 8.5f, juce::Font::bold))
                                .withExtraKerningFactor(0.20f);
    g.setFont(labelFont);
    g.setColour(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.40f));
    g.drawText("IN",  inLabelRect.toFloat(),  juce::Justification::centredLeft, false);
    g.drawText("OUT", outLabelRect.toFloat(), juce::Justification::centredLeft, false);

    // Numeric values — Courier, right-aligned, matches OLED text colour.
    const float valuePx = (float) inValueRect.getHeight() * 0.78f;
    g.setFont(juce::Font(juce::FontOptions()
                             .withName("Courier New")
                             .withHeight(valuePx)));
    g.setColour(juce::Colour::fromFloatRGBA(0.9f, 0.95f, 1.0f, 0.92f));
    g.drawText(inText,  inValueRect.toFloat(),  juce::Justification::centredRight, false);
    g.drawText(outText, outValueRect.toFloat(), juce::Justification::centredRight, false);
}

} // namespace kaigen::phantom
