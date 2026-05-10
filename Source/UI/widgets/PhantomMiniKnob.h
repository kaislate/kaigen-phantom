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
class PhantomMiniKnob : public juce::Component
{
public:
    PhantomMiniKnob(juce::AudioProcessorValueTreeState& apvts,
                    juce::StringRef paramID,
                    const juce::String& label = {});
    ~PhantomMiniKnob() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool hitTest(int x, int y) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    juce::Slider slider;
    juce::RangedAudioParameter* param { nullptr };  // non-owning
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    juce::String label;

    bool  isDragging    { false };
    float dragStartNorm { 0.0f };
    int   dragStartY    { 0 };
    float defaultNorm   { 0.0f };

    juce::String formatValue();

    /** Cached static layers (body + shadow + OLED bezel + arc track), shared
     *  across all PhantomMiniKnob instances. Built lazily on first paint. */
    static const juce::Image& getCachedStaticLayers();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomMiniKnob)
};

} // namespace kaigen::phantom
