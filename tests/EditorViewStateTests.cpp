#include <catch2/catch_test_macros.hpp>
#include "../Source/EditorViewState.h"

using namespace kaigen::phantom;

TEST_CASE("EditorViewState defaults to useNativeEditor=false", "[editor-view]")
{
    EditorViewState s;
    REQUIRE_FALSE(s.useNativeEditor);
}

TEST_CASE("writeEditorViewToTree + readEditorViewFromTree round-trip true",
          "[editor-view][persistence]")
{
    juce::ValueTree wrapper("PluginState");
    EditorViewState in;
    in.useNativeEditor = true;
    writeEditorViewToTree(wrapper, in);
    auto out = readEditorViewFromTree(wrapper);
    REQUIRE(out.useNativeEditor);
}

TEST_CASE("writeEditorViewToTree + readEditorViewFromTree round-trip false",
          "[editor-view][persistence]")
{
    juce::ValueTree wrapper("PluginState");
    writeEditorViewToTree(wrapper, EditorViewState{ /*useNativeEditor=*/false });
    auto out = readEditorViewFromTree(wrapper);
    REQUIRE_FALSE(out.useNativeEditor);
}

TEST_CASE("readEditorViewFromTree returns default when child absent",
          "[editor-view][persistence]")
{
    juce::ValueTree wrapper("PluginState");
    auto out = readEditorViewFromTree(wrapper);
    REQUIRE_FALSE(out.useNativeEditor);
}
