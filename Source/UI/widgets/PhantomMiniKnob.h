// Source/UI/widgets/PhantomMiniKnob.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Compact rotary widget used in the Advanced row. Visually identical paint
 *  stack to PhantomKnob (neumorphic body + OLED + arc indicator + value text)
 *  but at a smaller size (~36 px) with an external label rendered below the
 *  body. */
class PhantomMiniKnob : public juce::Component,
                         public juce::Slider::Listener
{
public:
    PhantomMiniKnob(juce::AudioProcessorValueTreeState& apvts,
                    juce::StringRef paramID,
                    const juce::String& label = {},
                    bool darkBackground = false);
    ~PhantomMiniKnob() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool hitTest(int x, int y) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    bool isPerEngine() const noexcept { return enginePrefix.isNotEmpty(); }

    /** See PhantomKnob::setEnginePrefix. Mirror prefix triggers LINK mode
     *  (writes flow to both engines' matching param). */
    void setEnginePrefix(const juce::String& activePrefix,
                          const juce::String& mirrorPrefix = {});

private:
    juce::Slider slider;
    juce::AudioProcessorValueTreeState* apvtsRef { nullptr };
    juce::String enginePrefix;
    juce::String leafName;
    juce::String mirrorPrefix;
    juce::RangedAudioParameter* param { nullptr };  // non-owning
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    juce::String label;

    void sliderValueChanged(juce::Slider*) override;
    bool isMirroring { false };

    bool  isDragging    { false };
    float dragStartNorm { 0.0f };
    int   dragStartY    { 0 };
    float defaultNorm   { 0.0f };

    juce::String formatValue();

    /** Cached static layers (body + shadow + OLED bezel + arc track), shared
     *  across all PhantomMiniKnob instances with the same style. The dark
     *  variant drops the white shadow halo + uses a dark body gradient so
     *  the knob blends with a dark section background (e.g. SamplerStrip)
     *  instead of standing out with phantom-white highlights. */
    static const juce::Image& getCachedStaticLayers(bool darkBackground);

    bool darkStyle { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomMiniKnob)
};

} // namespace kaigen::phantom
