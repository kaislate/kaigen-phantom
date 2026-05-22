// Source/UI/widgets/RecipeWheel.h
#pragma once
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** RecipeWheel — 7-spoke holographic radial widget for harmonic amplitudes H2-H8.
 *
 *  Full canvas port of Source/WebUI/recipe-wheel.js.
 *  Visual layers (draw order):
 *    1. Background radial gradient
 *    2. 6 holographic rings (rotating independently)
 *    3. Spoke dim tracks
 *    4. Spoke glow halo + fill gradient + cap dot + outer node + label
 *    5. 140 particles (20 per spoke)
 *    6. Rotating scan line
 *    7. Pulsing centre glow
 *
 *  Internally owns 7 hidden juce::Slider instances + 7 SliderParameterAttachments
 *  to bind to 'a_recipe_h2' through 'a_recipe_h8'.
 *  A 60 Hz Timer drives animation; repaint is throttled to ~30 fps (every other tick). */
class RecipeWheel : public juce::Component,
                     private juce::Timer,
                     private juce::ChangeListener
{
public:
    static constexpr int kSpokes            = 7;
    static constexpr int kParticlesPerSpoke = 20;
    static constexpr int kNumRings          = 6;

    /** @param apvts      The plugin APVTS.
     *  @param paramIDs   7 H param IDs (H2..H8) in order.
     *  @param processor  Optional pointer for wheel-lock subscription;
     *                    pass nullptr if the wheel is used outside of the
     *                    main editor (no flash on lock-reject). */
    RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                const std::array<juce::String, kSpokes>& paramIDs,
                ::PhantomProcessor* processor = nullptr);
    ~RecipeWheel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    /** Returns spoke index (0-6) if point is near a spoke, else -1.
     *  Matches JS hitSpoke(): perpendicular-distance approach. */
    int hitSpoke(juce::Point<float> p) const;

    /** Project mouse position onto spoke spokeIdx → normalised amplitude [0,1].
     *  Matches JS pointerToAmp(). */
    float pointerToAmp(juce::Point<float> p, int spokeIdx) const;

    /** Spoke angle for index i: (i/7)*2π - π/2 (top = spoke 0). */
    static float spokeAngle(int i) noexcept;

    /** Get normalised [0,1] amplitude for spoke i from its hidden slider. */
    float getSpokeAmp(int i) const noexcept;

    /** Write a new amplitude via the hidden slider (triggers APVTS). */
    void setSpokeAmp(int spokeIdx, float amp01);

    // ── APVTS wiring ─────────────────────────────────────────────────────
    std::array<juce::Slider, kSpokes>                                   sliders;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>, kSpokes> attachments;
    std::array<juce::RangedAudioParameter*, kSpokes>                    params { {} };

    // ── Animation state ───────────────────────────────────────────────────
    std::array<float, kNumRings>                          ringRot         {};
    std::array<float, kSpokes * kParticlesPerSpoke>       particleProgress {};
    float scanAngle  { 0.0f };
    float shimmerT   { 0.0f };
    int   timerTick  { 0 };   // frame parity: repaint only on even ticks

    // ── Interaction state ─────────────────────────────────────────────────
    int dragSpoke  { -1 };
    int hoverSpoke { -1 };

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Lock-flash overlay state. Set true by changeListenerCallback when the
    // processor rejects an H edit; cleared after ~120 ms by the existing
    // animation timer.
    bool  lockFlashActive  { false };
    int   lockFlashFramesRemaining { 0 };

    ::PhantomProcessor* processorRef { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeWheel)
};

} // namespace kaigen::phantom
