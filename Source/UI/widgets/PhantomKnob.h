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
class PhantomKnob : public juce::Component
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

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    // ── Geometry helpers ───────────────────────────────────────────────────
    int   diameter() const;
    int   inset()    const;
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

    // ── State ──────────────────────────────────────────────────────────────
    Size        sizeVariant;
    juce::String labelText;

    juce::Slider slider;
    juce::RangedAudioParameter* param { nullptr };  // non-owning; for getText() formatting
    std::unique_ptr<juce::SliderParameterAttachment> attachment;

    bool  isDragging    { false };
    float dragStartNorm { 0.0f };
    int   dragStartY    { 0 };

    float defaultNorm   { 0.0f };   // normalized default value for double-click reset

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomKnob)
};

} // namespace kaigen::phantom
