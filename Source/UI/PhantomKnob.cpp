#include "PhantomKnob.h"

PhantomKnob::PhantomKnob(const juce::String& label, Size size, float defaultValue)
    : knobSize(size), defaultVal(defaultValue)
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setName(label);
}

void PhantomKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    setValue(defaultVal, juce::sendNotificationSync);
}
