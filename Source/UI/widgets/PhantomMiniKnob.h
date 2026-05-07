// Source/UI/widgets/PhantomMiniKnob.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

class PhantomMiniKnob : public juce::Component, private juce::Slider::Listener
{
public:
    PhantomMiniKnob(juce::AudioProcessorValueTreeState& apvts,
                    juce::StringRef paramID,
                    const juce::String& label = {});
    ~PhantomMiniKnob() override;

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
    juce::String label;
    std::unique_ptr<juce::Drawable> bodyDrawable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomMiniKnob)
};

} // namespace kaigen::phantom
