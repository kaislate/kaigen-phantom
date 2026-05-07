// Source/UI/widgets/PhantomKnob.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Native rotary knob widget for the Phantom plugin. Wraps a juce::Slider
 *  (hidden) and a SliderParameterAttachment for APVTS binding. Paints a
 *  baked-SVG body plus a dynamic indicator line. Mouse events forward to
 *  the internal slider. */
class PhantomKnob : public juce::Component, private juce::Slider::Listener
{
public:
    enum class Size { Large, Medium, Small };

    PhantomKnob(juce::AudioProcessorValueTreeState& apvts,
                juce::StringRef paramID,
                Size size,
                const juce::String& label = {});
    ~PhantomKnob() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void sliderValueChanged(juce::Slider* s) override;

    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    Size size;
    juce::String label;
    std::unique_ptr<juce::Drawable> bodyDrawable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomKnob)
};

} // namespace kaigen::phantom
