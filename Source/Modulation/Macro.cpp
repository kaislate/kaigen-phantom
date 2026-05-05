// Source/Modulation/Macro.cpp
#include "Macro.h"

namespace kaigen::phantom
{

Macro::Macro(juce::String idStr,
             juce::AudioProcessorValueTreeState& apvtsRef,
             juce::String apvtsParamIdStr)
    : Modulator(std::move(idStr))
    , apvts(apvtsRef)
    , apvtsParamId(std::move(apvtsParamIdStr))
    , name(getId())
{
    cachedValuePtr = apvts.getRawParameterValue(apvtsParamId);
    jassert(cachedValuePtr != nullptr);
}

float Macro::getCurrentValue() const noexcept
{
    return cachedValuePtr ? cachedValuePtr->load() : 0.0f;
}

void Macro::writeToTree(juce::ValueTree& parent) const
{
    juce::ValueTree node("Macro");
    node.setProperty("id",   getId(), nullptr);
    node.setProperty("name", name,    nullptr);
    parent.appendChild(node, nullptr);
}

void Macro::readFromTree(const juce::ValueTree& parent)
{
    for (int i = 0; i < parent.getNumChildren(); ++i)
    {
        auto child = parent.getChild(i);
        if (child.getType() == juce::Identifier("Macro")
            && child.getProperty("id").toString() == getId())
        {
            name = child.getProperty("name", getId()).toString();
            return;
        }
    }
}

} // namespace kaigen::phantom
