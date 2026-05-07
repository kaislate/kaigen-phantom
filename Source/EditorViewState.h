// Source/EditorViewState.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** Editor-level view state. Persisted in <EditorView> as a sibling of
 *  <EngineFocus>/<SpectrumView>/<MatrixView> inside the <PluginState> wrapper.
 *  NOT preset state — preset switching doesn't change which editor opens. */
struct EditorViewState
{
    bool useNativeEditor { false };
};

inline void writeEditorViewToTree(juce::ValueTree& parent, const EditorViewState& s)
{
    juce::ValueTree node("EditorView");
    node.setProperty("useNativeEditor", s.useNativeEditor, nullptr);
    parent.appendChild(node, nullptr);
}

inline EditorViewState readEditorViewFromTree(const juce::ValueTree& parent)
{
    EditorViewState s;
    auto node = parent.getChildWithName("EditorView");
    if (! node.isValid()) return s;
    if (node.hasProperty("useNativeEditor"))
        s.useNativeEditor = (bool) node.getProperty("useNativeEditor");
    return s;
}

} // namespace kaigen::phantom
