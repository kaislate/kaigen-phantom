# Path B — Phase 6: WebView2 Cutover

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete the WebView2-based editor entirely. The native editor (Phases 0-5) becomes the only editor. Remove all WebView2 source code, assets, IPC bridge code, build-system entries, and the toggle infrastructure.

**Architecture:** Phases 0-5 introduced a parallel `NativePluginEditor` (under `Source/UI/`) gated by `EditorViewState::useNativeEditor`. Phase 6 removes the gate, the WebView2 editor (`Source/PluginEditor.{h,cpp}`), the WebUI assets (`Source/WebUI/`), the WebView2 NuGet fetch + JUCE_USE_WIN_WEBVIEW2 compile defs, and the now-redundant `EditorViewState` struct. Each task is one commit so the cutover is bisectable.

**Tech Stack:** JUCE 8.0.4, C++20, MSVC 17.14, CMake.

**Branch:** continue on `feature/pr3b-macro-editor`.

**Irreversibility:** This phase deletes ~70 KB of source code + 17 WebUI files + the WebView2 SDK. Once tasks 3-7 land, returning to WebView2 means resurrecting from git history. The `feature/pr3b-macro-editor` branch will retain the full history; nothing on `master` changes until merge.

---

## File-by-file footprint

| Path | Action | Note |
|------|--------|------|
| `Source/PluginEditor.{h,cpp}` | **Delete** | WebView2 editor + inlined IPC bridge (`.withNativeFunction()` calls) |
| `Source/WebUI/` | **Delete (folder)** | 17 JS/CSS/HTML files; self-contained |
| `Source/EditorViewState.h` | **Delete** | Only field was `useNativeEditor`; obsolete after cutover |
| `Source/UI/NativePluginEditor.{h,cpp}` | **Edit** | Remove `backToWebViewButton`, listener setup, buttonClicked handler |
| `Source/PluginProcessor.{h,cpp}` | **Edit** | Remove `editorView` member, getEditorView/setEditorView, simplify createEditor() |
| `CMakeLists.txt` | **Edit** | Remove PhantomWebUI binary-data block, WebView2 SDK fetch block, JUCE_USE_WIN_WEBVIEW2 defines, WebView2LoaderStatic.lib link |
| `tests/EditorViewStateTests.cpp` | **Delete** | Only tests the gate flag |
| `tests/CMakeLists.txt` | **Edit** | Remove EditorViewStateTests entry |
| `docs/` | **Edit (light)** | Mark obsolete plans/specs as superseded; do NOT delete history |

**No external dependencies to remove besides WebView2 itself.** Native editor uses `juce::juce_gui_extra` for `juce::Drawable`, `juce::PopupMenu`, etc. — that module stays.

---

## Task 1: Flip the default to native; remove the toggle infrastructure

**Files:**
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/UI/NativePluginEditor.h`
- Modify: `Source/UI/NativePluginEditor.cpp`

Drop the `useNativeEditor` branch in `createEditor()` to always return the native editor; remove the `backToWebViewButton` from the native editor (since there's no WebView2 to go back to). Leave `EditorViewState` and `Source/PluginEditor.{h,cpp}` alone for now — they get deleted in Tasks 3-4 once we've confirmed the new createEditor() is wired correctly. After Task 1: native editor is the only editor visible; the WebView2 code is dead but still present.

- [ ] **Step 1: Simplify createEditor()**

In `Source/PluginProcessor.cpp`, find the existing `createEditor()` method (around line 481-486). It currently branches on `editorView.useNativeEditor`. Replace the entire body with a single line that returns the native editor:

```cpp
juce::AudioProcessorEditor* PhantomProcessor::createEditor()
{
    return new kaigen::phantom::NativePluginEditor(*this, *apvts);
}
```

(The exact APVTS member name may be different — match what the existing code passes. Read the existing method first before editing.)

- [ ] **Step 2: Remove backToWebViewButton from NativePluginEditor.h**

Delete the `juce::TextButton backToWebViewButton{"Back to WebView"};` member (or whatever the exact label is). Also delete any `Listener` inheritance + override declarations if NativePluginEditor only listened on this button.

- [ ] **Step 3: Remove the listener setup and handler in NativePluginEditor.cpp**

Find and delete:
- `backToWebViewButton.addListener(this);`
- `addAndMakeVisible(backToWebViewButton);`
- The `backToWebViewButton.setBounds(...)` line in `resized()`
- `backToWebViewButton.removeListener(this);` in the destructor
- The entire `void NativePluginEditor::buttonClicked(juce::Button* b)` method
- The `juce::Button::Listener` base class from the header (if no other buttons listen via the Listener protocol — verify)

- [ ] **Step 4: Build both targets to verify**

```
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: clean build. The WebView2 editor still compiles since we haven't deleted it yet — it's just unreachable.

- [ ] **Step 5: Manual smoke**

Open the plugin in Live (reload if needed). Confirm: native editor opens directly; no "Back to WebView" button in the corner; PHANTOM logo no longer toggles to WebView2. The shift+click toggle on the WebView2 side stops mattering because nothing constructs the WebView2 editor anymore.

- [ ] **Step 6: Commit**

```bash
git add Source/PluginProcessor.cpp Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp
git commit -m "feat(path-b): default to native editor; remove WebView2 toggle UI"
```

---

## Task 2: Delete `Source/PluginEditor.{h,cpp}` (the WebView2 editor)

**Files:**
- Delete: `Source/PluginEditor.h`
- Delete: `Source/PluginEditor.cpp`
- Modify: `CMakeLists.txt` (remove from `target_sources`)

- [ ] **Step 1: Remove from CMakeLists.txt target_sources**

Find `target_sources(KaigenPhantom PRIVATE` and remove the lines that reference `Source/PluginEditor.cpp` and `Source/PluginEditor.h`. (Header may not be in target_sources — only the .cpp typically. Check.)

- [ ] **Step 2: Delete the files**

```bash
git rm Source/PluginEditor.h Source/PluginEditor.cpp
```

- [ ] **Step 3: Build both targets**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: clean build. (We need to re-run cmake configure since target_sources changed.)

If the build fails with "undeclared identifier" for any WebView2-related symbol that NativePluginEditor accidentally referenced, STOP and report — that's a bug in Phases 0-5 that needs investigation.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(path-b): delete WebView2 editor (PluginEditor.{h,cpp})"
```

---

## Task 3: Delete `Source/WebUI/` (assets) and the PhantomWebUI binary data registration

**Files:**
- Delete: `Source/WebUI/` (whole folder)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Remove the PhantomWebUI binary data block in CMakeLists.txt**

Find `juce_add_binary_data(PhantomWebUI ...)` (around lines 79-97 per the audit). Delete the entire block including all `SOURCES` lines listing each WebUI file. Confirm the `juce_add_binary_data(PhantomNativeAssets ...)` block at lines 99-108 (per audit) STAYS — that's the native SVG assets.

Find any `target_link_libraries` line that links `PhantomWebUI` and remove just that token (leave the other libs).

- [ ] **Step 2: Delete the WebUI folder**

```bash
git rm -r Source/WebUI
```

- [ ] **Step 3: Build both targets**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: clean build. If the build fails on any unresolved `BinaryData::*` symbol from the PhantomWebUI namespace, that means something still references the WebView2 assets — STOP and report.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(path-b): delete Source/WebUI/ and PhantomWebUI binary data"
```

---

## Task 4: Remove the WebView2 SDK fetch and JUCE_USE_WIN_WEBVIEW2 defines

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Remove the WebView2 NuGet fetch block**

Find the block (around lines 117-128 per the audit) that downloads & extracts the WebView2 NuGet package. Delete the entire block.

- [ ] **Step 2: Remove the compile defines**

Find `target_compile_definitions(KaigenPhantom PUBLIC ...)` containing `JUCE_WEB_BROWSER=1`, `JUCE_USE_WIN_WEBVIEW2=1`, `WIN32_LEAN_AND_MEAN`, etc. (around lines 130-136). Delete the WebView2-specific defines:
- `JUCE_WEB_BROWSER=1`
- `JUCE_USE_WIN_WEBVIEW2=1`

Keep any non-WebView2 defines that block contains. If the entire block is empty after removal, delete the block.

- [ ] **Step 3: Remove WebView2LoaderStatic.lib link**

Find and delete the line that links `WebView2LoaderStatic.lib` (around line 145 per audit). Keep any other libs in the same `target_link_libraries` block.

- [ ] **Step 4: Build both targets**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: clean build. JUCE's `juce::WebBrowserComponent` will no longer be functional, but nothing references it anymore (we deleted PluginEditor in Task 2).

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt
git commit -m "feat(path-b): remove WebView2 SDK fetch + JUCE_USE_WIN_WEBVIEW2 defines"
```

---

## Task 5: Delete `EditorViewState`

**Files:**
- Delete: `Source/EditorViewState.h`
- Delete: `tests/EditorViewStateTests.cpp` (or whatever the test file is named)
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Remove the EditorViewState member and accessors from PluginProcessor**

In `Source/PluginProcessor.h`, find and remove:
- `#include "EditorViewState.h"`
- `EditorViewState getEditorView() const;`
- `void setEditorView(const EditorViewState& s);`
- `EditorViewState editorView;` (private member)

In `Source/PluginProcessor.cpp`, find and remove:
- The implementations of `getEditorView()` and `setEditorView()`
- Any code that round-trips `editorView` to/from the value tree state in `getStateInformation()` / `setStateInformation()` (the `editorView.writeToTree(...)` / `readFromTree(...)` calls). Remove just those round-trip calls; don't touch the rest of the state I/O.

- [ ] **Step 2: Delete the EditorViewState header**

```bash
git rm Source/EditorViewState.h
```

- [ ] **Step 3: Delete the test file and remove its CMakeLists entry**

```bash
git rm tests/EditorViewStateTests.cpp
```

In `tests/CMakeLists.txt`, find and remove the line(s) that reference `EditorViewStateTests.cpp`.

- [ ] **Step 4: Build all targets including tests**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
ctest --test-dir build --build-config Debug
```

Expected: clean build, all tests pass. If a test references `editorView` indirectly, STOP and report.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat(path-b): delete EditorViewState (toggle no longer exists)"
```

---

## Task 6: Update docs to mark WebView2-era plans/specs as superseded

**Files:**
- Modify (don't delete) docs that reference WebView2

Don't delete history. Add a single-line "**Superseded by Phase 6 cutover (2026-05-08).**" at the top of WebView2-era plans/specs so future readers know the implementation no longer matches the doc.

- [ ] **Step 1: Audit which docs need a banner**

Run a grep for "WebView", "web UI", "WebView2", "PluginEditor.cpp" in `docs/` and list the files. Skim the matched ones to decide which describe shipping behavior (need the banner) vs which describe the migration itself (no banner needed — they're correct in describing their moment).

- [ ] **Step 2: Add the superseded banner**

For each shipping-behavior doc, add a single line at the top (after the title):

```markdown
> **Superseded by Phase 6 cutover (2026-05-08) — WebView2 editor is no longer present in the codebase. This doc is retained for historical context.**
```

- [ ] **Step 3: Commit**

```bash
git add docs/
git commit -m "docs: mark WebView2-era plans as superseded after Phase 6 cutover"
```

---

## Task 7: Final smoke + verification

**Files:** none modified — verification only.

- [ ] **Step 1: Full clean rebuild**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
ctest --test-dir build --build-config Debug
```

Expected: clean. All tests pass.

- [ ] **Step 2: Manual smoke in Live**

Reload the plugin in Live. Walk through the same smoke that Phase 5 used:
- Slot row visible by default; macro/morph knobs draggable
- MATRIX → matrix overlay, slot row collapses
- Cell click/drag/right-click works
- Macro name editor in matrix view works
- Live cell pulse visible when macro values change (use Live automation/MIDI mapping)
- Persistence across project save/reopen
- Preset prev/next/save/browse all work
- A/B engine compare works
- All other Phase 0-4 features (visualizers, recipe wheel, filter coupling, advanced collapse, ghost section) work

If any feature is broken, STOP — Phase 6 cutover should be invisible from a UX standpoint.

- [ ] **Step 3: Source tree audit**

Confirm zero references remain:

```bash
grep -r "WebView" Source/ CMakeLists.txt
grep -r "PhantomWebUI" Source/ CMakeLists.txt
grep -r "PluginEditor.h" Source/
grep -r "useNativeEditor" Source/
grep -r "EditorViewState" Source/
grep -r "JUCE_USE_WIN_WEBVIEW2" .
```

Expect each grep to return zero matches (or only matches inside `docs/` history — those are fine).

- [ ] **Step 4: Branch summary**

Run `git log --oneline master..HEAD` to see the full Phase 5+6 commit list. Should be ~25-30 commits.

- [ ] **Step 5: Push (deferred — only if user requests)**

DO NOT push without user confirmation. The user reviews the branch before deciding when to merge to `master` and what to do about the existing PRs (#13, #14, #15 etc.).

---

## Self-review notes

### Spec coverage

Phase 6 covers per the original spec line item: "Phase 6 — Cutover (delete WebView2 entirely)". Done across 7 tasks.

### Risk areas

- **Task 1**: low risk; reversible by reverting the createEditor() simplification.
- **Tasks 2-4**: irreversible deletion. Should be done one task at a time with build verification between each so a regression can be bisected.
- **Task 5**: removing EditorViewState has a state-tree compat consideration. Old presets/projects that wrote `editorView` to ValueTree state will have an orphan node on load. JUCE's ValueTree iteration silently ignores unknown children — confirm by reading `setStateInformation` after the change. Worst case: a one-time warning log line; data is preserved.
- **Task 6**: doc edits are cosmetic.
- **Task 7**: no code change.

### Placeholder scan

No "TBD"/"TODO"/vague language. Each step has runnable code or commands. Line-number references are approximate (per audit) and may need to be looked up at execution time.

### What ships after Phase 6

A standalone, single-editor plugin with no WebView2 dependency:
- Smaller binary (no embedded HTML/CSS/JS)
- No WebView2 SDK NuGet fetch in CI
- No `JUCE_USE_WIN_WEBVIEW2` runtime requirement
- Native editor as the sole UI

After Phase 6, Path B is complete. The remaining polish items (visual fidelity to match WebView2 CSS, animation, dirty-state indicator, etc.) are tracked separately.
