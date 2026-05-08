// Source/UI/widgets/RecipeWheel.h
#pragma once
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** RecipeWheel — 7-spoke radial widget for harmonic amplitudes H2-H8.
 *  Each spoke's length encodes one harmonic's normalized value (0..1).
 *  Drag a spoke's head radially to change that harmonic.
 *
 *  Internally owns 7 hidden juce::Slider instances + 7 SliderParameterAttachments
 *  to bind to the per-engine 'a_recipe_h2' through 'a_recipe_h8' parameters.
 *  The sliders are never shown — they exist only as the attachment surface. */
class RecipeWheel : public juce::Component, private juce::Slider::Listener
{
public:
    static constexpr int kSpokes = 7;

    /** Constructor takes apvts and the 7 parameter IDs in H2-H8 order. */
    RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                const std::array<juce::String, kSpokes>& paramIDs);
    ~RecipeWheel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void sliderValueChanged(juce::Slider* s) override;

    /** Hit-test: returns spoke index (0..6) at mouse position, or -1. */
    int hitTestSpoke(juce::Point<float> p) const;

    /** Convert mouse position to a 0..1 spoke value (radial distance from center,
     *  normalized to the wheel's outer radius). */
    float pointToValue(juce::Point<float> p) const;

    std::array<juce::Slider, kSpokes> sliders;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>, kSpokes> attachments;
    std::array<juce::RangedAudioParameter*, kSpokes> params { {} };  // for begin/endChangeGesture
    int activeSpoke { -1 };  // index being dragged, -1 = none

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeWheel)
};

} // namespace kaigen::phantom
