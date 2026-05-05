// Source/SpectrumViewMode.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** Spectrum view layout. Underlying integer values are stable: serialized
 *  through the WebView bridge and persisted in `<SpectrumView>` properties.
 *  Don't reorder or renumber without versioning the persistence. */
enum class SpectrumViewMode : int { Split = 0, Combined = 1 };

/** Append `<SpectrumView>` child to `parent` capturing the view mode. */
inline void writeSpectrumViewModeToTree(juce::ValueTree& parent, SpectrumViewMode mode)
{
    juce::ValueTree node("SpectrumView");
    node.setProperty("mode", mode == SpectrumViewMode::Combined ? "Combined" : "Split", nullptr);
    parent.appendChild(node, nullptr);
}

/** Read SpectrumViewMode from the `<SpectrumView>` child of `parent`.
 *  Returns the default (Split) if the child is absent or malformed. */
inline SpectrumViewMode readSpectrumViewModeFromTree(const juce::ValueTree& parent)
{
    auto node = parent.getChildWithName("SpectrumView");
    if (!node.isValid()) return SpectrumViewMode::Split;
    return (node.getProperty("mode").toString() == "Combined")
           ? SpectrumViewMode::Combined : SpectrumViewMode::Split;
}

} // namespace kaigen::phantom
