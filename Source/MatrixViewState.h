// Source/MatrixViewState.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** Bottom modulation panel view mode. Underlying integer values are stable:
 *  serialized through the WebView bridge and persisted in `<MatrixView>`
 *  properties. Don't reorder or renumber without versioning the persistence. */
enum class MatrixMode : int { Slots = 0, Matrix = 1 };

/** Matrix view UI state. Persists mode + per-engine expanded category list.
 *  Routing data itself lives in `<ModulationConfig>` (PR3a); this struct only
 *  holds layout/UI concerns. The expanded-categories strings are CSV lists of
 *  category IDs (e.g. "GHOST,RECIPE,FILTER") matching the DEST_GROUPS table
 *  in matrix.js. The default ("GHOST,RECIPE") mirrors the JS-side initial
 *  state — both Sets default to those two categories. */
struct MatrixViewState
{
    MatrixMode  mode       { MatrixMode::Slots };
    juce::String expandedA { "GHOST,RECIPE" };  // comma-separated category IDs
    juce::String expandedB { "GHOST,RECIPE" };
};

/** Append `<MatrixView>` child to `parent` capturing the matrix view state. */
inline void writeMatrixViewToTree(juce::ValueTree& parent, const MatrixViewState& s)
{
    juce::ValueTree node("MatrixView");
    node.setProperty("mode", s.mode == MatrixMode::Matrix ? "Matrix" : "Slots", nullptr);
    node.setProperty("expandedA", s.expandedA, nullptr);
    node.setProperty("expandedB", s.expandedB, nullptr);
    parent.appendChild(node, nullptr);
}

/** Read MatrixViewState from the `<MatrixView>` child of `parent`.
 *  Returns defaults (Slots mode, GHOST+RECIPE expanded on both engines) if
 *  the child is absent. */
inline MatrixViewState readMatrixViewFromTree(const juce::ValueTree& parent)
{
    MatrixViewState s;
    auto node = parent.getChildWithName("MatrixView");
    if (! node.isValid()) return s;
    s.mode = (node.getProperty("mode").toString() == "Matrix")
             ? MatrixMode::Matrix : MatrixMode::Slots;
    if (node.hasProperty("expandedA")) s.expandedA = node.getProperty("expandedA").toString();
    if (node.hasProperty("expandedB")) s.expandedB = node.getProperty("expandedB").toString();
    return s;
}

} // namespace kaigen::phantom
