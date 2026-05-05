// Source/EngineFocus.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** Which engine the editor is currently displaying / editing.
 *  Underlying integer values (A=0, B=1) are stable: they're serialized
 *  through the WebView bridge and persisted in `<EditorFocus>` properties.
 *  Don't reorder or renumber without also versioning the persistence. */
enum class ActiveTab : int { A = 0, B = 1 };

/** Editor-state focus: which tab is active, plus whether LINK mode mirrors
 *  knob-value writes to both engines. NOT preset state — preset switching
 *  doesn't move the user's tab. */
struct EngineFocus
{
    ActiveTab activeTab { ActiveTab::A };
    bool      linkOn    { false };
};

/** Append <EditorFocus> child to `parent` capturing the focus state. */
inline void writeEngineFocusToTree(juce::ValueTree& parent, EngineFocus focus)
{
    juce::ValueTree focusTree("EditorFocus");
    focusTree.setProperty("activeTab",
                          focus.activeTab == ActiveTab::B ? "B" : "A", nullptr);
    focusTree.setProperty("linkOn", focus.linkOn, nullptr);
    parent.appendChild(focusTree, nullptr);
}

/** Read EngineFocus from the <EditorFocus> child of `parent`. Returns
 *  defaults (A, link off) if the child is absent. */
inline EngineFocus readEngineFocusFromTree(const juce::ValueTree& parent)
{
    EngineFocus focus;
    auto focusTree = parent.getChildWithName("EditorFocus");
    if (!focusTree.isValid()) return focus;
    focus.activeTab = (focusTree.getProperty("activeTab").toString() == "B")
                      ? ActiveTab::B : ActiveTab::A;
    focus.linkOn    = (bool) focusTree.getProperty("linkOn");
    return focus;
}

} // namespace kaigen::phantom
