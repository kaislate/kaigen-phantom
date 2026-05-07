// Source/UI/widgets/ToggleGroup.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

class ToggleGroup : public juce::Component, private juce::ComboBox::Listener
{
public:
    ToggleGroup(juce::AudioProcessorValueTreeState& apvts,
                juce::StringRef paramID,
                const juce::StringArray& labels);
    ~ToggleGroup() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void comboBoxChanged(juce::ComboBox* c) override;
    void buttonClicked(int index);

    juce::ComboBox combo;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;
    juce::OwnedArray<juce::TextButton> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToggleGroup)
};

} // namespace kaigen::phantom
