// Source/UI/panels/RightPanel.h
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/PhantomMiniKnob.h"

namespace kaigen::phantom
{

class RightPanel : public juce::Component
{
public:
    explicit RightPanel(juce::AudioProcessorValueTreeState& apvts);
    ~RightPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Harmonic Engine section
    PhantomKnob saturationKnob;
    PhantomKnob shapeKnob;
    PhantomKnob skipKnob;

    // Stereo section
    PhantomKnob widthKnob;

    // Levels section
    PhantomKnob inGainKnob;
    PhantomKnob outGainKnob;

    // Levels section auto-gain toggle
    juce::TextButton autoGainButton { "Auto" };
    std::unique_ptr<juce::ButtonParameterAttachment> autoGainAttachment;

    // Advanced panel — 14 mini knobs, all per-engine 'a_' prefix.
    std::array<std::unique_ptr<PhantomMiniKnob>, 14> miniKnobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RightPanel)
};

} // namespace kaigen::phantom
