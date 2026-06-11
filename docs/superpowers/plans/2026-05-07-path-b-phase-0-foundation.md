# Path B — Phase 0: Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the scaffolding for the native UI rewrite — `NativePluginEditor` skeleton class, runtime toggle persisted in plugin state, theme tokens header, and a dev-mode toggle UI in the existing WebView2 editor for dogfooding. After this phase the toggle works end-to-end, but the native editor renders only a labeled wireframe.

**Architecture:** A new `kaigen::phantom::NativePluginEditor` class subclasses `juce::AudioProcessorEditor` and lives alongside the existing WebView2 `PhantomEditor`. `PhantomProcessor::createEditor()` checks a persisted `useNativeEditor` flag (in a new `<EditorView>` ValueTree node, mirroring the existing `<EngineFocus>`/`<SpectrumView>`/`<MatrixView>` pattern) and instantiates one or the other. WebView2 stays the default. A shift+click on the PHANTOM logo in the WebView2 editor toggles the flag via a new `setUseNativeEditor` native binding; the change takes effect on next plugin window reopen.

**Tech Stack:** JUCE 8.0.4, C++20 (MSVC 17.14), Catch2 v3.5.2 for tests.

**Branch:** continue on `feature/pr3b-macro-editor` for now (the current branch); a fresh branch like `path-b` could be created later if desired but isn't required for Phase 0 since changes are additive and don't affect default behavior.

**Spec:** `docs/superpowers/specs/2026-05-07-path-b-native-ui-design.md`.

---

## File Structure

| File | Status | Responsibility |
|------|--------|----------------|
| `Source/UI/Theme.h` | **Create** | Header-only `juce::Colour` constants for the steel-blue accent, panel surfaces, modulator type colors. Single source of truth for the color palette. |
| `Source/EditorViewState.h` | **Create** | Header-only `EditorViewState` struct (just `useNativeEditor` bool for now) + `writeEditorViewToTree` / `readEditorViewFromTree` helpers. Mirrors the existing `EngineFocus.h` / `SpectrumViewMode.h` / `MatrixViewState.h` pattern. |
| `Source/UI/NativePluginEditor.h` | **Create** | Editor class header. Subclasses `juce::AudioProcessorEditor`. Fixed 1300×970 size. |
| `Source/UI/NativePluginEditor.cpp` | **Create** | Editor implementation. `paint()` draws labeled wireframe rectangles using `Theme.h` colors. Includes a small "← Back to WebView2" button in the corner so the dev can toggle off without editing plugin state. |
| `Source/PluginProcessor.h` | **Modify** | Add `kaigen::phantom::EditorViewState editorView` member and `getEditorView()` / `setEditorView()` accessors. |
| `Source/PluginProcessor.cpp` | **Modify** | In `getStateInformation`/`setStateInformation`, write/read the `<EditorView>` ValueTree node alongside `<EngineFocus>`/`<SpectrumView>`/`<MatrixView>`/`<ModulationConfig>`. In `createEditor()`, branch on `editorView.useNativeEditor` to instantiate either the native or the WebView2 editor. |
| `Source/PluginEditor.cpp` | **Modify** | Add the `setUseNativeEditor` native binding inside the existing `withNativeFunction` chain. |
| `Source/WebUI/phantom.js` | **Modify** | Add a shift+click handler on the `.phantom-logo` element that calls `setUseNativeEditor(true)` and shows a confirmation alert telling the user to reopen the plugin window. |
| `CMakeLists.txt` | **Modify** | Add `Source/UI/NativePluginEditor.cpp` to `target_sources(KaigenPhantom)`. |
| `tests/EditorViewStateTests.cpp` | **Create** | Catch2 round-trip tests for the persistence helpers. |
| `tests/CMakeLists.txt` | **Modify** | Add the new test file to the test target's source list. |

After Phase 0:
- `Source/UI/` directory exists, ready for Phase 1+ widgets.
- The native editor opens to a wireframe when toggled on.
- WebView2 stays default and untouched. Existing functionality is unchanged.

---

## Task 1: Create Source/UI/Theme.h

**Files:**
- Create: `Source/UI/Theme.h`

This is a header-only file. No tests yet — it's just constants, exercised when later tasks reference the values.

- [ ] **Step 1: Create the Source/UI/ directory and Theme.h**

```bash
mkdir -p "Source/UI"
```

Create `Source/UI/Theme.h`:

```cpp
// Source/UI/Theme.h
#pragma once
#include <juce_graphics/juce_graphics.h>

namespace kaigen::phantom::Theme
{
    // ── Accent + modulator type colors (matches existing CSS values) ─────
    inline const juce::Colour steelBlue       { 0xff4A90E2 };  // active toggles, primary accent
    inline const juce::Colour macroTeal       { 0xff5DD3E0 };
    inline const juce::Colour lfoBlue         { 0xff4A90E2 };
    inline const juce::Colour randomPurple    { 0xff9990E0 };
    inline const juce::Colour morphWhite      { 0xffEFEFF2 };

    // ── Surfaces ─────────────────────────────────────────────────────────
    inline const juce::Colour panelBg         { 0xff0E1116 };  // standard panel surface
    inline const juce::Colour matrixBg        { 0xff0A0C10 };  // matrix view background
    inline const juce::Colour panelBorder     { 0x14ffffff };  // 8% white panel borders
    inline const juce::Colour deepBg          { 0xff05070A };  // editor outermost background

    // ── Text ─────────────────────────────────────────────────────────────
    inline const juce::Colour textPrimary     { 0xffc8d8ea };  // primary readable text
    inline const juce::Colour textSecondary   { 0x73ffffff };  // 45% white — secondary labels
    inline const juce::Colour textDim         { 0x40ffffff };  // 25% white — placeholder labels

    // ── Highlights / glow ────────────────────────────────────────────────
    inline const juce::Colour activeGlow      { 0x335DD3E0 };  // 20% teal — active state shadow
    inline const juce::Colour clipRed         { 0xffe85050 };  // meter clip indicator
}
```

- [ ] **Step 2: Commit**

```bash
git add Source/UI/Theme.h
git commit -m "feat(path-b): Theme.h color tokens for native UI"
```

---

## Task 2: Create EditorViewState.h with TDD persistence test

**Files:**
- Create: `Source/EditorViewState.h`
- Create: `tests/EditorViewStateTests.cpp`
- Modify: `tests/CMakeLists.txt`

The persistence pattern mirrors `Source/EngineFocus.h` and `Source/MatrixViewState.h` — header-only struct + free `writeXxxToTree`/`readXxxFromTree` helpers in the `kaigen::phantom` namespace.

- [ ] **Step 1: Write the failing test**

Create `tests/EditorViewStateTests.cpp`:

```cpp
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
```

Add to `tests/CMakeLists.txt`. Find the `add_executable(KaigenPhantomTests` block (around line 9 — same place `MatrixViewStatePersistenceTest.cpp` is listed). Add `EditorViewStateTests.cpp` alphabetically near it:

```cmake
add_executable(KaigenPhantomTests
    WaveletSynthTests.cpp
    BinauralStageTests.cpp
    BassExtractorTests.cpp
    WaveshaperTests.cpp
    EnvelopeFollowerTests.cpp
    PhantomEngineTests.cpp
    PresetMigrationTests.cpp
    MorphCrossfaderTests.cpp
    DualEngineHostTests.cpp
    EditorViewStateTests.cpp
    EngineFocusTests.cpp
    SpectrumViewModeTests.cpp
    MatrixViewStatePersistenceTest.cpp
    RoutingTests.cpp
    ModulatorTests.cpp
    ModulationEngineTests.cpp
    RoutingMutationTests.cpp
    ../Source/Engines/BinauralStage.cpp
    ../Source/Engines/BassExtractor.cpp
    ../Source/Engines/Waveshaper.cpp
    ../Source/Engines/EnvelopeFollower.cpp
    ../Source/Engines/PhantomEngine.cpp
    ../Source/Engines/ZeroCrossingSynth.cpp
    ../Source/Engines/WaveletSynth.cpp
    ../Source/PresetManager.cpp
    ../Source/PresetMigration.cpp
    ../Source/MorphCrossfader.cpp
    ../Source/DualEngineHost.cpp
    ../Source/Modulation/Routing.cpp
    ../Source/Modulation/Macro.cpp
    ../Source/Modulation/ModulationEngine.cpp
)
```

- [ ] **Step 2: Run the test to verify it fails**

Run:
```
cmake --build build --config Debug --target KaigenPhantomTests
```
Expected: build FAILS — `Source/EditorViewState.h` doesn't exist; the test's `#include` cannot resolve.

- [ ] **Step 3: Create Source/EditorViewState.h**

```cpp
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
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```
cmake --build build --config Debug --target KaigenPhantomTests
ctest --test-dir build/tests -C Debug -R "editor-view"
```
Expected: all 4 EditorViewState tests PASS.

- [ ] **Step 5: Commit**

```bash
git add Source/EditorViewState.h tests/EditorViewStateTests.cpp tests/CMakeLists.txt
git commit -m "feat(path-b): EditorViewState persistence header + Catch2 tests"
```

---

## Task 3: Create NativePluginEditor skeleton

**Files:**
- Create: `Source/UI/NativePluginEditor.h`
- Create: `Source/UI/NativePluginEditor.cpp`
- Modify: `CMakeLists.txt`

A minimal `juce::AudioProcessorEditor` subclass. `paint()` fills the entire editor with `Theme::deepBg` (a dark background). Doesn't include the wireframe yet — Task 7 adds the labeled rectangles. Doesn't include the back-to-WebView2 button yet — Task 7 adds that too. This task is pure scaffolding; we just want the class to compile and link.

- [ ] **Step 1: Create Source/UI/NativePluginEditor.h**

```cpp
// Source/UI/NativePluginEditor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PhantomProcessor;  // forward declaration

namespace kaigen::phantom
{

/** Native (non-WebView2) plugin editor for the Phantom plugin.
 *  Phase 0: blank wireframe placeholder. Phase 1+ progressively populates
 *  with native widgets (knobs, visualizers, matrix view, etc.). */
class NativePluginEditor : public juce::AudioProcessorEditor
{
public:
    NativePluginEditor(PhantomProcessor& processor, juce::AudioProcessorValueTreeState& apvts);
    ~NativePluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativePluginEditor)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/NativePluginEditor.cpp**

```cpp
// Source/UI/NativePluginEditor.cpp
#include "NativePluginEditor.h"
#include "../PluginProcessor.h"
#include "Theme.h"

namespace kaigen::phantom
{

NativePluginEditor::NativePluginEditor(PhantomProcessor& p,
                                       juce::AudioProcessorValueTreeState& a)
    : juce::AudioProcessorEditor(&p), processor(p), apvts(a)
{
    setSize(1300, 970);
}

NativePluginEditor::~NativePluginEditor() = default;

void NativePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::deepBg);
}

void NativePluginEditor::resized()
{
    // Phase 7+ will lay out child components here.
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, find the `target_sources(KaigenPhantom PRIVATE` block (~line 42). Add `Source/UI/NativePluginEditor.cpp`:

```cmake
target_sources(KaigenPhantom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/NativePluginEditor.cpp
    Source/Engines/BinauralStage.cpp
    Source/Engines/BassExtractor.cpp
    Source/Engines/ZeroCrossingSynth.cpp
    Source/Engines/WaveletSynth.cpp
    Source/Engines/EnvelopeFollower.cpp
    Source/Engines/PhantomEngine.cpp
    Source/PresetManager.cpp
    Source/PresetMigration.cpp
    Source/MorphCrossfader.cpp
    Source/DualEngineHost.cpp
    Source/Modulation/Routing.cpp
    Source/Modulation/Macro.cpp
    Source/Modulation/ModulationEngine.cpp
)
```

- [ ] **Step 4: Build to verify compile**

Run:
```
cmake --build build --config Debug --target KaigenPhantom_Standalone
```
Expected: build succeeds. The new NativePluginEditor class is compiled and linked but never instantiated yet.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp CMakeLists.txt
git commit -m "feat(path-b): NativePluginEditor skeleton class (paints background)"
```

---

## Task 4: Wire EditorViewState into PluginProcessor

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

Add the `EditorViewState editorView` member and persist it through `getStateInformation`/`setStateInformation`. Mirror the exact pattern used for `MatrixViewState`.

- [ ] **Step 1: Modify Source/PluginProcessor.h**

Find the `#include "MatrixViewState.h"` line (or wherever the existing view-state includes are). Add:

```cpp
#include "EditorViewState.h"
```

Inside the `PhantomProcessor` class, find the existing view-state members (likely `engineFocus`, `spectrumViewMode`, `matrixView`). Add:

```cpp
public:
    using EditorViewState = kaigen::phantom::EditorViewState;
    EditorViewState getEditorView() const               { return editorView; }
    void setEditorView(const EditorViewState& s)        { editorView = s; }

private:
    kaigen::phantom::EditorViewState editorView;
```

(Place these alongside the existing `getMatrixView()` / `setMatrixView()` and `MatrixViewState matrixView` to keep the header organized.)

- [ ] **Step 2: Modify Source/PluginProcessor.cpp — write side**

In `getStateInformation`, find the existing `kaigen::phantom::writeMatrixViewToTree(wrapper, matrixView);` line. Add immediately after it:

```cpp
kaigen::phantom::writeEditorViewToTree(wrapper, editorView);
```

- [ ] **Step 3: Modify Source/PluginProcessor.cpp — read side**

In `setStateInformation`, find the existing block that reads `<MatrixView>`:

```cpp
if (wrapper.getChildWithName("MatrixView").isValid())
    matrixView = kaigen::phantom::readMatrixViewFromTree(wrapper);
```

Add immediately after it:

```cpp
if (wrapper.getChildWithName("EditorView").isValid())
    editorView = kaigen::phantom::readEditorViewFromTree(wrapper);
```

- [ ] **Step 4: Build and run all tests**

Run:
```
cmake --build build --config Debug --target KaigenPhantomTests
ctest --test-dir build/tests -C Debug
```
Expected: all tests pass, including the EditorViewState tests from Task 2.

- [ ] **Step 5: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat(path-b): persist EditorViewState in plugin state"
```

---

## Task 5: Branch createEditor() on the flag

**Files:**
- Modify: `Source/PluginProcessor.cpp`

When `editorView.useNativeEditor` is true, instantiate `NativePluginEditor`; otherwise the existing `PhantomEditor`. WebView2 stays the default.

- [ ] **Step 1: Add include for NativePluginEditor**

In `Source/PluginProcessor.cpp`, near the top alongside the other includes (probably near `#include "PluginEditor.h"`), add:

```cpp
#include "UI/NativePluginEditor.h"
```

- [ ] **Step 2: Modify the createEditor() implementation**

Find `juce::AudioProcessorEditor* PhantomProcessor::createEditor()`. The current body returns a `new PhantomEditor(...)` directly. Replace with:

```cpp
juce::AudioProcessorEditor* PhantomProcessor::createEditor()
{
    if (editorView.useNativeEditor)
        return new kaigen::phantom::NativePluginEditor(*this, apvts);
    return new PhantomEditor(*this);
}
```

(If the existing `PhantomEditor` constructor takes different arguments — e.g., just `*this` rather than `(*this, apvts)` — match its current signature exactly. The native one takes both because we'll need APVTS for parameter attachments in Phase 1+.)

- [ ] **Step 3: Build to verify**

Run:
```
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```
Expected: both targets build successfully. Default behavior unchanged (flag is false; WebView2 still loads).

- [ ] **Step 4: Commit**

```bash
git add Source/PluginProcessor.cpp
git commit -m "feat(path-b): branch createEditor() on useNativeEditor flag"
```

---

## Task 6: Add `setUseNativeEditor` native binding + JS shift+click toggle

**Files:**
- Modify: `Source/PluginEditor.cpp`
- Modify: `Source/WebUI/phantom.js`

Adds a way for the dev (us) to toggle the flag from inside the running plugin. Shift+click the PHANTOM logo in the WebView2 editor calls a new native binding `setUseNativeEditor(true)`. The change persists immediately and takes effect on the next plugin window reopen.

- [ ] **Step 1: Add the native binding in PluginEditor.cpp**

Find the existing `withNativeFunction(...)` chain (where `matrixGetState` and other bindings are registered). Add a new binding to the chain:

```cpp
.withNativeFunction("setUseNativeEditor",
    [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
    {
        if (args.size() < 1) { complete({}); return; }
        const bool useNative = (bool) args[0];
        auto state = self.processor.getEditorView();
        state.useNativeEditor = useNative;
        self.processor.setEditorView(state);
        // Mark plugin state dirty so the host saves the new flag.
        self.processor.updateHostDisplay();
        complete(juce::var(true));
    })
```

(Place this near the existing `matrixSetState` binding to keep editor-view bindings grouped.)

- [ ] **Step 2: Add the shift+click handler in phantom.js**

Find an appropriate place in `Source/WebUI/phantom.js` to add the handler — preferably near other top-level click handlers. The PHANTOM logo element is identified by its location at the top of the editor; we'll target it via a data attribute or class. First, verify in `Source/WebUI/index.html` what the logo element's selector is. Look near the top of the body (around the preset bar). It's likely a `<div class="phantom-logo">` or similar.

If the logo doesn't have a stable selector, add `id="phantom-logo"` to the HTML element first:

In `Source/WebUI/index.html`, find the element that displays "PHANTOM" branding text or logo (typically near the top, in a header-style div). Add `id="phantom-logo"` to it. If you can't find a clear logo element, use the body itself as the click target with a coordinate-based shift+click filter — but a dedicated element is cleaner.

Then in `Source/WebUI/phantom.js`, add at the bottom of the file (after the existing initialization):

```js
// ── Dev toggle: shift+click the PHANTOM logo to switch to native UI ────
// Path B development aid. Sends a setUseNativeEditor binding call; the
// change takes effect on next plugin window reopen. Removed when Path B
// cuts over (Phase 6).
(function setupNativeUIToggle() {
    const logo = document.getElementById('phantom-logo');
    if (!logo) return;
    if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') return;

    let setUseNativeEditor = null;
    try { setUseNativeEditor = window.Juce.getNativeFunction('setUseNativeEditor'); }
    catch (e) { console.warn('[phantom] setUseNativeEditor binding unavailable', e); return; }
    if (!setUseNativeEditor) return;

    logo.addEventListener('click', (ev) => {
        if (!ev.shiftKey) return;
        ev.preventDefault();
        ev.stopPropagation();
        try {
            setUseNativeEditor(true);
            alert('Native UI enabled. Close and reopen the plugin window to see it.');
        } catch (e) { console.warn('[phantom] setUseNativeEditor failed', e); }
    });
})();
```

- [ ] **Step 3: Build VST3 + Standalone**

Run:
```
cmake --build build --config Debug --target KaigenPhantom_VST3
cmake --build build --config Debug --target KaigenPhantom_Standalone
```
Expected: both succeed. VST3 auto-installs to `%LOCALAPPDATA%\Programs\Common\VST3\`.

- [ ] **Step 4: Commit**

```bash
git add Source/PluginEditor.cpp Source/WebUI/phantom.js Source/WebUI/index.html
git commit -m "feat(path-b): dev toggle — shift+click PHANTOM logo enables native UI"
```

---

## Task 7: Render labeled wireframe + back-to-WebView2 button in NativePluginEditor

**Files:**
- Modify: `Source/UI/NativePluginEditor.h`
- Modify: `Source/UI/NativePluginEditor.cpp`

The native editor's `paint()` draws labeled rectangles at the positions of the future major panels (TopBar, LeftPanel, RightPanel, ModulationPanel) so we can visually verify the layout intent. Also adds a small `juce::TextButton` in the corner labeled "← WebView2" that toggles the flag back off so the dev can switch without editing plugin state externally.

The wireframe is purely placeholder — Phase 1+ will replace `paint()` with real children + their own paint methods. The corner button stays through development as a dev-mode escape hatch; removed in Phase 6 cutover.

- [ ] **Step 1: Update NativePluginEditor.h**

Add the button member and click handler:

```cpp
// Source/UI/NativePluginEditor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

class NativePluginEditor : public juce::AudioProcessorEditor,
                           private juce::Button::Listener
{
public:
    NativePluginEditor(PhantomProcessor& processor, juce::AudioProcessorValueTreeState& apvts);
    ~NativePluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* b) override;

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton backToWebViewButton { "<- WebView2" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativePluginEditor)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Update NativePluginEditor.cpp**

Replace the file's contents with:

```cpp
// Source/UI/NativePluginEditor.cpp
#include "NativePluginEditor.h"
#include "../PluginProcessor.h"
#include "Theme.h"

namespace kaigen::phantom
{

namespace
{
    // Wireframe panel positions. Phase 1+ replaces these with real child Components.
    constexpr int editorWidth         = 1300;
    constexpr int editorHeight        = 970;
    constexpr int topBarHeight        = 50;
    constexpr int modPanelHeight      = 150;
    constexpr int leftPanelWidth      = 420;

    void drawLabeledPanel(juce::Graphics& g,
                          juce::Rectangle<int> bounds,
                          const juce::String& label)
    {
        g.setColour(Theme::panelBg);
        g.fillRect(bounds);
        g.setColour(Theme::panelBorder);
        g.drawRect(bounds, 1);
        g.setColour(Theme::textSecondary);
        g.setFont(juce::Font("Space Grotesk", 14.0f, juce::Font::bold));
        g.drawText(label, bounds, juce::Justification::centred, false);
    }
}

NativePluginEditor::NativePluginEditor(PhantomProcessor& p,
                                       juce::AudioProcessorValueTreeState& a)
    : juce::AudioProcessorEditor(&p), processor(p), apvts(a)
{
    setSize(editorWidth, editorHeight);

    backToWebViewButton.addListener(this);
    addAndMakeVisible(backToWebViewButton);
}

NativePluginEditor::~NativePluginEditor()
{
    backToWebViewButton.removeListener(this);
}

void NativePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::deepBg);

    auto area = getLocalBounds();

    // TopBar across the very top.
    auto topBar = area.removeFromTop(topBarHeight);
    drawLabeledPanel(g, topBar, "TopBar (preset selector + advanced toggle)");

    // ModulationPanel across the very bottom.
    auto modPanel = area.removeFromBottom(modPanelHeight);
    drawLabeledPanel(g, modPanel, "ModulationPanel (mode bar + slot row + engine labels)");

    // MainArea split into LeftPanel + RightPanel.
    auto leftPanel = area.removeFromLeft(leftPanelWidth);
    drawLabeledPanel(g, leftPanel, "LeftPanel (recipe wheel + ghost + filter)");
    drawLabeledPanel(g, area, "RightPanel (harmonic engine + stereo + levels + visualizers)");
}

void NativePluginEditor::resized()
{
    // Corner escape-hatch button — dev-only, removed in Phase 6.
    backToWebViewButton.setBounds(getWidth() - 110, 10, 100, 26);
}

void NativePluginEditor::buttonClicked(juce::Button* b)
{
    if (b == &backToWebViewButton)
    {
        auto state = processor.getEditorView();
        state.useNativeEditor = false;
        processor.setEditorView(state);
        processor.updateHostDisplay();
        // Note: change takes effect on next editor reopen. The current
        // editor stays as-is until the host destroys + recreates it.
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Switched to WebView2",
            "Close and reopen the plugin window to load the WebView2 UI.");
    }
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Build VST3 + Standalone**

Run:
```
cmake --build build --config Debug --target KaigenPhantom_VST3
cmake --build build --config Debug --target KaigenPhantom_Standalone
```
Expected: both build clean. VST3 auto-reinstalls.

- [ ] **Step 4: Commit**

```bash
git add Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp
git commit -m "feat(path-b): wireframe panels + back-to-WebView2 dev button in native editor"
```

---

## Task 8: Manual smoke test

**Files:** none modified — this is verification only.

Verify the toggle works end-to-end. No commit unless something is broken.

- [ ] **Step 1: Open the Standalone app**

Launch:
```
"build/KaigenPhantom_artefacts/Debug/Standalone/Kaigen Phantom.exe"
```

Expected: WebView2 editor opens normally (default behavior).

- [ ] **Step 2: Shift+click the PHANTOM logo**

Hold Shift and click the PHANTOM branding text/logo at the top of the editor. Expected: a JS `alert()` popup says "Native UI enabled. Close and reopen the plugin window to see it." Click OK to dismiss.

- [ ] **Step 3: Close and reopen the plugin window**

In Standalone, close the app and reopen. (In a DAW VST3 test, close the plugin window and reopen it from the device.)

Expected: the editor opens to a wireframe with four labeled rectangles:
- "TopBar (preset selector + advanced toggle)" at the top
- "LeftPanel (recipe wheel + ghost + filter)" on the left
- "RightPanel (harmonic engine + stereo + levels + visualizers)" on the right
- "ModulationPanel (mode bar + slot row + engine labels)" at the bottom
- A small "← WebView2" button in the top-right corner.

- [ ] **Step 4: Click the back-to-WebView2 button**

Click the corner button. Expected: a JUCE alert dialog says "Switched to WebView2. Close and reopen the plugin window to load the WebView2 UI."

- [ ] **Step 5: Reopen the plugin again**

Expected: the original WebView2 editor returns. The toggle state persisted correctly through both transitions.

- [ ] **Step 6: Verify in a DAW (Live)**

Load Phantom in Ableton Live, do steps 2-5 inside Live. Verify:
- Plugin state persists across Live's project save/load (set native, save Live project, close, reopen, expect native still active).
- Both editors load without crashing.
- No DAW UI thread issues (the WebView2 saturation problem is unrelated to this Phase 0 toggle).

- [ ] **Step 7: If everything works, no commit needed**

This task is verification only. If you hit issues, file them as Phase 0 follow-ups before continuing to Phase 1.

---

## Self-review notes

### Spec coverage

Phase 0 covers, per the spec's "Phase 0 — Foundation" line item:
- ✓ `NativePluginEditor` skeleton (Task 3, Task 7)
- ✓ Runtime toggle wired (Task 4, Task 5, Task 6)
- ✓ `Theme.h` color tokens (Task 1)
- ✓ Asset CMake plumbing — **deferred to Phase 1.** Phase 0 has no actual SVG/PNG assets; bundling infrastructure isn't useful until Phase 1 adds the first knob body. CMake addition is a one-line change that fits naturally with Phase 1 Task 1.
- ✓ Blank panels at correct positions/sizes (Task 7)

### Placeholder scan

No "TBD" / "TODO" / vague-language. Each step has runnable code or commands.

### Type / signature consistency

- `EditorViewState` struct shape consistent across header (Task 2), tests (Task 2), processor accessor (Task 4), and binding (Task 6).
- `getEditorView()` / `setEditorView()` referenced with the same names in Tasks 4, 6, 7.
- `useNativeEditor` field name consistent everywhere.
- `NativePluginEditor` namespace `kaigen::phantom` and constructor signature `(PhantomProcessor&, juce::AudioProcessorValueTreeState&)` consistent across Task 3 (skeleton), Task 5 (createEditor call site), and Task 7 (paint + button updates).
- `setUseNativeEditor` binding name consistent between C++ (Task 6 step 1) and JS (Task 6 step 2).

### What ships in Phase 0

After this plan completes, the plugin behaves identically by default — WebView2 still loads, all existing features work. The native editor is reachable only via the dev toggle (shift+click). No user-facing change yet. The `useNativeEditor` flag persists in plugin state alongside the existing view-state blocks.

Phase 1 picks up by populating `Source/Assets/svg/` with the first knob body, wiring it into `juce_add_binary_data`, creating the `PhantomKnob` widget class, and replacing the wireframe `RightPanel` rectangle with a real harmonic-engine knob row.
