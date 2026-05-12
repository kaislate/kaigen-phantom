// Source/UI/widgets/PhantomKnob.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Native rotary knob widget — faithful canvas port of the WebView2 <phantom-knob>
 *  SVG component (Source/WebUI/knob.js).
 *
 *  Visual layers (paint order):
 *    1. Neumorphic raised body — radial gradient + 4 offset DropShadow passes
 *    2. OLED well — black fill + 3 concentric bezel strokes
 *    3. Arc track — faint 270° background arc
 *    4. Glow halo arc — blurred indicator arc (approximated via wide alpha stroke)
 *    5. Sharp value arc — crisp white indicator arc
 *    6. Value text — 3-pass layered shadow (alpha 0.30 / 0.60 / 1.00)
 *
 *  The internal juce::Slider is hidden (not rendered). It exists solely so that
 *  juce::SliderParameterAttachment can write to the APVTS parameter when the user
 *  drags or double-clicks.  All painting and mouse interaction is handled here.  */
class PhantomKnob : public juce::Component,
                     public juce::Slider::Listener
{
public:
    enum class Size { Large, Medium, Small };

    PhantomKnob(juce::AudioProcessorValueTreeState& apvts,
                juce::StringRef paramID,
                Size size,
                const juce::String& label = {});
    ~PhantomKnob() override;

    /** Returns the hidden slider — used by LeftPanel for filter-link mirroring. */
    juce::Slider& getSlider() noexcept { return slider; }

    /** True iff this knob was constructed with an "a_" or "b_" prefixed
     *  param ID — i.e., a per-engine parameter that can be retargeted at
     *  runtime. Non-per-engine knobs (e.g., morph_amount) return false and
     *  ignore setEnginePrefix(). */
    bool isPerEngine() const noexcept { return enginePrefix.isNotEmpty(); }

    /** Rebind the active SliderParameterAttachment to `<activePrefix><leaf>`,
     *  and optionally mirror writes to `<mirrorPrefix><leaf>` (used for LINK
     *  mode). Pass empty `mirrorPrefix` to disable mirroring.
     *  No-op for non-per-engine knobs. */
    void setEnginePrefix(const juce::String& activePrefix,
                          const juce::String& mirrorPrefix = {});

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool hitTest(int x, int y) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    // ── Geometry helpers ───────────────────────────────────────────────────
    int   diameter() const;
    int   inset()    const;
    int   shadowPadding() const;
    float textPx(bool dragging) const;

    // ── Paint layers ───────────────────────────────────────────────────────
    void paintBody(juce::Graphics& g, juce::Point<float> centre, float radius);
    void paintOLED(juce::Graphics& g, juce::Point<float> centre, float oledR);
    void paintArcTrack(juce::Graphics& g, juce::Point<float> centre, float arcR,
                       float arcStart, float arcSweep);
    void paintIndicatorArc(juce::Graphics& g, juce::Point<float> centre, float arcR,
                           float arcStart, float arcSweep, float value01);
    void paintValueText(juce::Graphics& g, juce::Point<float> centre,
                        const juce::String& text, float oledR);

    juce::String formatValue();

    // ── Static layer cache (shared per size variant) ───────────────────────
    /** Returns the cached "static layers" image for `size`: body + offset
     *  shadow + OLED bezel + arc track. Lazy-initialised on first call.
     *  Drawn via g.drawImageAt() on every paint() — replaces the per-frame
     *  re-render of those layers (which was the source of drag stutter). */
    static const juce::Image& getCachedStaticLayers(Size size);

    /** Invalidate the static-layer cache (call if Theme tokens change at
     *  runtime — currently never happens, but kept as a hook). */
    static void clearStaticLayersCache();

    // ── State ──────────────────────────────────────────────────────────────
    Size        sizeVariant;
    juce::String labelText;

    juce::Slider slider;
    juce::AudioProcessorValueTreeState* apvtsRef { nullptr };   // for retargeting attachment
    juce::String enginePrefix;                                  // "a_" / "b_" / "" (non-per-engine)
    juce::String leafName;                                      // e.g. "ghost"
    juce::String mirrorPrefix;                                  // "" when no LINK
    juce::RangedAudioParameter* param { nullptr };              // non-owning; for getText() formatting
    std::unique_ptr<juce::SliderParameterAttachment> attachment;

    // Slider::Listener: when LINK is on, mirror normalized value to the
    // other engine's param. The attachment writes the active param; we
    // write the mirror.
    void sliderValueChanged(juce::Slider*) override;
    bool isMirroring { false };   // recursion guard

    bool  isDragging    { false };
    float dragStartNorm { 0.0f };
    int   dragStartY    { 0 };

    float defaultNorm   { 0.0f };   // normalized default value for double-click reset

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomKnob)
};

} // namespace kaigen::phantom
