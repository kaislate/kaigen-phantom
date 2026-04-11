#include "RecipeWheel.h"

using namespace juce;

//==============================================================================
RecipeWheel::RecipeWheel()
{
    startTimerHz (15);
}

//==============================================================================
// Public interface
//==============================================================================

void RecipeWheel::updatePitch (float pitchHz)
{
    if (std::abs (pitchHz - currentPitchHz) > 0.1f)
    {
        currentPitchHz = pitchHz;
        repaint();
    }
}

void RecipeWheel::setHarmonicAmplitudes (const std::array<float, 7>& amps)
{
    harmonicAmps = amps;
    repaint();
}

void RecipeWheel::setPresetName (const juce::String& name)
{
    presetName = name;
    repaint();
}

//==============================================================================
// Timer
//==============================================================================

void RecipeWheel::timerCallback()
{
    static constexpr float kSpeeds[6] = { 0.015f, -0.02f, 0.01f, -0.025f, 0.012f, -0.008f };

    for (int i = 0; i < 6; ++i)
        ringRotations[i] += kSpeeds[i];

    scanAngle    += 0.18f;
    shimmerPhase += 0.05f;

    repaint();
}

//==============================================================================
// paint() — delegates to helpers
//==============================================================================

void RecipeWheel::paint (Graphics& g)
{
    const float cx     = (float) getWidth()  * 0.5f;
    const float cy     = (float) getHeight() * 0.5f;
    const float radius = jmin (cx, cy) - 4.0f;

    paintNeumorphicMount  (g, cx, cy, radius);
    paintHolographicRings (g, cx, cy);

    const float innerR = 52.0f;
    const float outerR = 118.0f;
    paintTankPathways     (g, cx, cy, innerR, outerR);
    paintHarmonicLabels   (g, cx, cy, outerR + 14.0f);
    paintCenterOled       (g, cx, cy, 48.0f);
}

//==============================================================================
// paintNeumorphicMount
//==============================================================================

void RecipeWheel::paintNeumorphicMount (Graphics& g, float cx, float cy, float radius)
{
    const Rectangle<int> ellipseBounds (
        (int) (cx - radius), (int) (cy - radius),
        (int) (radius * 2.0f), (int) (radius * 2.0f));

    // Top-left highlight shadow
    DropShadow hlShadow (Colour (0x14ffffff), 16, { -6, -6 });
    {
        juce::Path ellipsePath;
        ellipsePath.addEllipse (ellipseBounds.toFloat());
        hlShadow.drawForPath (g, ellipsePath);
    }

    // Bottom-right dark shadow
    DropShadow dkShadow (Colour (0x80000000), 20, { 6, 6 });
    {
        juce::Path ellipsePath;
        ellipsePath.addEllipse (ellipseBounds.toFloat());
        dkShadow.drawForPath (g, ellipsePath);
    }

    // Radial gradient surface
    const float lightX = cx - radius * 0.5f;
    const float lightY = cy - radius * 0.5f;
    ColourGradient grad (Colour (0x14ffffff), lightX, lightY,
                         Colour (0x80000000), cx + radius * 0.5f, cy + radius * 0.5f,
                         true);  // radial
    g.setGradientFill (grad);
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Border ring
    Path border;
    border.addEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (Colour (0x0dffffff));
    g.strokePath (border, PathStrokeType (2.0f));
}

//==============================================================================
// paintHolographicRings
//==============================================================================

void RecipeWheel::paintHolographicRings (Graphics& g, float cx, float cy)
{
    static constexpr float kRadii    [6] = { 120.0f, 100.0f, 78.0f, 52.0f, 28.0f, 126.0f };
    static constexpr float kWidths   [6] = {   0.8f,   0.6f,  0.5f,  0.6f,  0.8f,   0.3f };
    static constexpr float kOpacities[6] = {  0.10f,  0.07f, 0.05f, 0.08f, 0.13f, 0.035f };

    const float twoPi = MathConstants<float>::twoPi;

    for (int i = 0; i < 6; ++i)
    {
        const float r   = kRadii[i];
        const float rot = ringRotations[i];

        Path ring;
        ring.addArc (cx - r, cy - r, r * 2.0f, r * 2.0f,
                     rot, rot + twoPi, true);

        g.setColour (PhantomColours::phosphorWhite.withAlpha (kOpacities[i]));
        g.strokePath (ring, PathStrokeType (kWidths[i]));
    }

    // Rotating scan line
    const float scanR = 122.0f;
    const float sx    = cx + scanR * std::cos (scanAngle);
    const float sy    = cy + scanR * std::sin (scanAngle);
    g.setColour (PhantomColours::phosphorWhite.withAlpha (0.05f));
    g.drawLine (cx, cy, sx, sy, 1.0f);
}

//==============================================================================
// paintTankPathways
//==============================================================================

void RecipeWheel::paintTankPathways (Graphics& g, float cx, float cy,
                                     float innerR, float outerR)
{
    const float twoPi  = MathConstants<float>::twoPi;
    const float halfPi = MathConstants<float>::halfPi;

    for (int i = 0; i < 7; ++i)
    {
        const float angle = (float) i * (twoPi / 7.0f) - halfPi;
        const float cosA  = std::cos (angle);
        const float sinA  = std::sin (angle);
        const float amp   = harmonicAmps[(size_t) i];

        // Track end-points
        const float x0 = cx + innerR * cosA;
        const float y0 = cy + innerR * sinA;
        const float x1 = cx + outerR * cosA;
        const float y1 = cy + outerR * sinA;

        // Dim track background
        g.setColour (PhantomColours::trackDim);
        g.drawLine (x0, y0, x1, y1, 5.0f);

        // Fill length proportional to amplitude
        const float fillR   = innerR + (outerR - innerR) * amp;
        const float xFill   = cx + fillR * cosA;
        const float yFill   = cy + fillR * sinA;

        // Glow halo around fill line
        g.setColour (PhantomColours::phosphorWhite.withAlpha (amp * 0.2f));
        g.drawLine (x0, y0, xFill, yFill, 6.0f);

        // Sharp fill line
        g.setColour (PhantomColours::phosphorWhite.withAlpha (0.15f + amp * 0.4f));
        g.drawLine (x0, y0, xFill, yFill, 3.5f);

        // Cap dot at fill endpoint
        const float capR = 3.0f;   // radius of the 6px-diameter dot
        g.setColour (PhantomColours::phosphorWhite.withAlpha (amp * 0.65f));
        g.fillEllipse (xFill - capR, yFill - capR, capR * 2.0f, capR * 2.0f);

        // Node shimmer glow at spoke end (outerR)
        const float nodeSize = 6.0f + amp * 12.0f;
        const float nodeHalf = nodeSize * 0.5f;
        g.setColour (PhantomColours::phosphorWhite.withAlpha (amp * 0.12f));
        g.fillEllipse (x1 - nodeHalf, y1 - nodeHalf, nodeSize, nodeSize);
    }
}

//==============================================================================
// paintCenterOled
//==============================================================================

void RecipeWheel::paintCenterOled (Graphics& g, float cx, float cy, float radius)
{
    // Black OLED well
    g.setColour (PhantomColours::oledBlack);
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Ridge lip
    const Rectangle<float> oledBounds (cx - radius, cy - radius,
                                        radius * 2.0f, radius * 2.0f);
    PhantomLookAndFeel::drawOledRidge (g, oledBounds);

    // Text layout constants
    const float lineH = 14.0f;
    const float halfH = lineH * 0.5f;

    // "FUND" label — small dim text, centred just above middle
    {
        const Font  labelFont ("Courier New", 6.0f, Font::plain);
        const auto  labelArea = Rectangle<float> (cx - radius, cy - halfH - lineH - 2.0f,
                                                   radius * 2.0f, lineH);
        g.setColour (PhantomColours::textDim.withAlpha (0.3f));
        g.setFont (labelFont);
        g.drawText ("FUND", labelArea, Justification::centred, false);
    }

    // Frequency / note name — glow text, centred at middle
    {
        const Font glowFont ("Courier New", 14.0f, Font::bold);
        const auto glowArea = Rectangle<float> (cx - radius, cy - halfH,
                                                 radius * 2.0f, lineH);

        juce::String freqText;
        if (currentPitchHz > 0.0f)
        {
            freqText = pitchToNoteName (currentPitchHz)
                       + " \xc2\xb7 "
                       + String (currentPitchHz, 1) + "Hz";
        }
        else
        {
            freqText = "-- \xc2\xb7 --";
        }

        PhantomLookAndFeel::drawGlowText (g, freqText, glowArea, glowFont,
                                           Justification::centred);
    }

    // Preset name — small phosphor text below middle
    {
        const Font  presetFont ("Courier New", 7.0f, Font::plain);
        const auto  presetArea = Rectangle<float> (cx - radius, cy + halfH + 2.0f,
                                                    radius * 2.0f, lineH);
        g.setColour (PhantomColours::phosphorWhite.withAlpha (0.7f));
        g.setFont (presetFont);
        g.drawText (presetName, presetArea, Justification::centred, false);
    }
}

//==============================================================================
// paintHarmonicLabels
//==============================================================================

void RecipeWheel::paintHarmonicLabels (Graphics& g, float cx, float cy, float radius)
{
    const float twoPi  = MathConstants<float>::twoPi;
    const float halfPi = MathConstants<float>::halfPi;

    const Font labelFont ("Courier New", 8.0f, Font::bold);
    g.setColour (PhantomColours::textDim);
    g.setFont (labelFont);

    for (int i = 0; i < 7; ++i)
    {
        const float angle = (float) i * (twoPi / 7.0f) - halfPi;
        const float lx    = cx + radius * std::cos (angle);
        const float ly    = cy + radius * std::sin (angle);

        const String label = "H" + String (i + 2);  // H2 … H8
        const float  boxW  = 24.0f;
        const float  boxH  = 12.0f;

        g.drawText (label,
                    Rectangle<float> (lx - boxW * 0.5f, ly - boxH * 0.5f, boxW, boxH),
                    Justification::centred, false);
    }
}

//==============================================================================
// Static helper — pitch to note name
//==============================================================================

juce::String RecipeWheel::pitchToNoteName (float hz)
{
    if (hz <= 0.0f)
        return "--";

    static const char* const kNoteNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const float semitone = 12.0f * std::log2 (hz / 440.0f) + 69.0f;
    const int   midi     = juce::roundToInt (semitone);
    const int   note     = ((midi % 12) + 12) % 12;   // guard against negative mod
    const int   octave   = midi / 12 - 1;

    return juce::String (kNoteNames[note]) + juce::String (octave);
}
