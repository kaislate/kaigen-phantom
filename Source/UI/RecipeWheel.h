#pragma once
#include <JuceHeader.h>
#include <array>
#include "PhantomColours.h"
#include "PhantomLookAndFeel.h"

//==============================================================================
/**  RecipeWheel
 *
 *   Holographic recipe visualisation for the Kaigen Phantom plugin.
 *   Renders animated concentric rings, harmonic tank pathways, and a centre
 *   OLED frequency display.
 */
class RecipeWheel : public juce::Component,
                    private juce::Timer
{
public:
    RecipeWheel();
    ~RecipeWheel() override = default;

    //==========================================================================
    void paint (juce::Graphics& g) override;

    /** Called from the editor timer whenever the tracked pitch changes. */
    void updatePitch (float pitchHz);

    /** Set all 7 harmonic amplitudes (H2-H8). Values should be 0-1. */
    void setHarmonicAmplitudes (const std::array<float, 7>& amps);

    /** Set the current preset name shown in the centre OLED. */
    void setPresetName (const juce::String& name);

private:
    //==========================================================================
    // Animation state
    float ringRotations[6]  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float scanAngle         { 0.0f };
    float shimmerPhase      { 0.0f };

    // Data
    std::array<float, 7> harmonicAmps { 0.8f, 0.7f, 0.5f, 0.35f, 0.2f, 0.12f, 0.07f };
    float          currentPitchHz { -1.0f };
    juce::String   presetName     { "Warm" };

    //==========================================================================
    // Timer
    void timerCallback() override;

    //==========================================================================
    // Paint helpers
    void paintNeumorphicMount      (juce::Graphics& g, float cx, float cy, float radius);
    void paintHolographicRings     (juce::Graphics& g, float cx, float cy);
    void paintTankPathways         (juce::Graphics& g, float cx, float cy,
                                    float innerR, float outerR);
    void paintCenterOled           (juce::Graphics& g, float cx, float cy, float radius);
    void paintHarmonicLabels       (juce::Graphics& g, float cx, float cy, float radius);

    //==========================================================================
    // Static helpers
    static juce::String pitchToNoteName (float hz);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecipeWheel)
};
