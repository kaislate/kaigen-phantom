// Source/UI/widgets/EtchedToggle.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Single-word boolean toggle painted in the WordSelector's etched-text
 *  style. Off = etched-dark-on-light. On = backlit-glow effect (matches
 *  WordSelector's "active" word style).
 *
 *  Bound to an APVTS bool param via juce::ButtonParameterAttachment. */
class EtchedToggle : public juce::Button
{
public:
    EtchedToggle(juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& boolParamId,
                 juce::String label);
    ~EtchedToggle() override = default;

    void paintButton(juce::Graphics& g,
                      bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

private:
    juce::String labelText;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EtchedToggle)
};

} // namespace kaigen::phantom
