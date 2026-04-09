#pragma once
#include <JuceHeader.h>

namespace ParamID
{
    inline constexpr auto MODE  = "mode";
    inline constexpr auto GHOST = "ghost";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParamID::MODE, "Mode", StringArray{ "Effect", "Instrument" }, 0));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParamID::GHOST, "Ghost",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    return { params.begin(), params.end() };
}
