#pragma once
#include <JuceHeader.h>

class PhantomKnob : public juce::Slider
{
public:
    enum class Size { Large, Medium };

    PhantomKnob(const juce::String& label, Size size, float defaultValue);

    Size  getKnobSize() const noexcept { return knobSize; }
    float getDefaultValue() const noexcept { return defaultVal; }
    int   getDiameter() const noexcept { return knobSize == Size::Large ? 100 : 72; }

    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    Size  knobSize;
    float defaultVal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomKnob)
};
