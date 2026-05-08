# Path B — Phase 4: Preset System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the WebView2 `preset-system.js` (967 lines) with a native preset selector and browser. Users can navigate the current preset name with prev/next arrows, open a browser modal to load any preset by pack, and save the current state via a simple in-editor text-input dialog.

**Architecture:** A compact `PresetSelector` widget lives in the TopBar (currently a wireframe placeholder). It shows the current preset name, has prev/next arrows that iterate through the flattened preset list, a "Browse" button that opens a `PresetBrowser` overlay, and a "Save" button that pops up a `juce::AlertWindow` text-input dialog. The browser is a child component of `NativePluginEditor` that becomes visible on demand and fills most of the editor area; clicks on a preset row call `processor.getPresetManager().loadPreset(...)`. The PresetSelector tracks the currently-loaded preset name + pack as its own state (the existing C++ PresetManager doesn't track this).

**Tech Stack:** JUCE 8.0.4 (`juce_gui_basics`, `juce::AlertWindow`, `juce::TextEditor`), C++20, MSVC 17.14. Reuses the existing C++ `PresetManager` API in `Source/PresetManager.h` directly — no native bindings, no JSON, no IPC.

**Branch:** continue on `feature/pr3b-macro-editor` (additive; default behavior unchanged for users without native editor enabled).

**Spec:** `docs/superpowers/specs/2026-05-07-path-b-native-ui-design.md`. Phase 0/1/2/3 already complete.

**Visual fidelity note:** Phase 4 ships functional preset UI. Visual polish (animation, custom fonts, neumorphic styling) is part of the broader pre-cutover refinement pass. Save dialog uses JUCE's stock `AlertWindow` for simplicity — replacing with a custom dialog component is a deferred polish task.

**Scope cuts vs. WebView2 implementation:**
- Save form: name field only. Designer/description/type fields deferred — saving still works via PresetManager (those fields default to "Experimental"/"User"/empty).
- Search box in browser: deferred.
- Favorites toggle: deferred (factory presets are still marked but no UI to toggle).
- Delete preset: deferred (users can delete files manually for now).
- Tag filtering: deferred.
- Preset preview thumbnails (the 7-harmonic mini-graph): deferred — just text rows for now.

These deferrals keep Phase 4 to ~5 tasks. Polish round before Phase 6 cutover can add them.

---

## Existing C++ infrastructure (DO NOT modify)

From `Source/PresetManager.h`:

```cpp
class PresetManager
{
public:
    void initialize();
    std::map<juce::String, std::vector<PresetInfo>> getAllPresets() const;
    bool loadPreset(juce::AudioProcessorValueTreeState& apvts,
                    const juce::String& presetName,
                    const juce::String& packName);
    juce::String savePreset(juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& presetName,
                            const juce::String& type,
                            const juce::String& designer,
                            const juce::String& description,
                            bool overwrite);
    void rescan();
    // ...
};
```

`PresetInfo` contains:
- `metadata.name`, `metadata.type`, `metadata.designer`, `metadata.description`
- `metadata.isFavorite`, `metadata.isFactory`
- `preview.h[7]`, `preview.crossover`, `preview.skip`

Native Phase 4 code calls these directly via `processor.getPresetManager()`.

---

## File Structure

| File | Status | Responsibility |
|------|--------|----------------|
| `Source/UI/widgets/PresetSelector.h` | **Create** | TopBar-mounted preset display + control widget. Shows current preset name, has 4 small buttons (prev / next / Browse / Save). Owns `currentPresetName` + `currentPresetPack` state. Emits `onBrowseRequested` callback when Browse clicked. |
| `Source/UI/widgets/PresetSelector.cpp` | **Create** | Implementation. Prev/Next iterate through the flattened preset list (concatenation of all packs in alphabetical order). Save calls `juce::AlertWindow::showAsync` to prompt for a name, then `processor.getPresetManager().savePreset(...)`. |
| `Source/UI/panels/PresetBrowser.h` | **Create** | Overlay Component: full-area list of presets grouped by pack. Visible/hidden on demand. Has a close button. Each row shows name + pack; click row → `processor.getPresetManager().loadPreset(...)`. |
| `Source/UI/panels/PresetBrowser.cpp` | **Create** | Implementation. Builds a `std::vector` of (pack, name) entries on `setVisible(true)`. Paint draws a backdrop scrim + a centered card with rows. Custom mouse handling (no `juce::ListBox` for now — flat row list keeps it simple). |
| `Source/UI/panels/TopBar.h` | **Create** | TopBar Component that hosts PresetSelector. Future content: advanced toggle (currently in viz-section as a seam latch — could move here in a polish pass), about/help button, etc. |
| `Source/UI/panels/TopBar.cpp` | **Create** | Implementation. |
| `Source/UI/NativePluginEditor.h` | **Modify** | Add `TopBar topBar;` and `PresetBrowser presetBrowser;` members. The browser is a child of NativePluginEditor (overlays the whole editor when visible), not a child of TopBar. |
| `Source/UI/NativePluginEditor.cpp` | **Modify** | Construct topBar + presetBrowser. Wire PresetSelector's `onBrowseRequested` to `presetBrowser.setVisible(true)`. Update `paint()` to remove the TopBar wireframe rectangle. Update `resized()` to position TopBar at the top + presetBrowser to fill the editor when visible. |
| `CMakeLists.txt` | **Modify** | Add `Source/UI/widgets/PresetSelector.cpp`, `Source/UI/panels/PresetBrowser.cpp`, `Source/UI/panels/TopBar.cpp` to `target_sources(KaigenPhantom)`. |

---

## Task 1: PresetSelector widget

**Files:**
- Create: `Source/UI/widgets/PresetSelector.h`
- Create: `Source/UI/widgets/PresetSelector.cpp`
- Modify: `CMakeLists.txt`

A compact widget for the TopBar — shows the current preset name in the middle, with prev / next arrow buttons flanking it, plus "Browse" and "Save" buttons. Owns the `currentPresetName` + `currentPresetPack` state (the existing C++ PresetManager doesn't track current selection).

Behavior:
- **Prev / Next**: build a flattened list of all (pack, name) entries from `getAllPresets()`. Find the current entry's index, advance/retreat by 1 (wrap at edges), call `loadPreset(name, pack)`, update `currentPresetName/Pack` state, repaint.
- **Browse**: emit `onBrowseRequested` callback (NativePluginEditor wires this to show the PresetBrowser overlay).
- **Save**: pop a `juce::AlertWindow::showAsync` with a text input for the preset name. On OK: call `savePreset(name, "Experimental", "User", "", false)` (using stock metadata defaults). On cancel: do nothing.

After Task 1: class compiles + links but isn't yet referenced. Task 4 wires it into TopBar.

- [ ] **Step 1: Create Source/UI/widgets/PresetSelector.h**

```cpp
// Source/UI/widgets/PresetSelector.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Compact preset selector for the TopBar.
 *  - Center: current preset name (text label)
 *  - Left: prev / next buttons (iterate flattened preset list)
 *  - Right: Browse button (opens PresetBrowser overlay) + Save button
 *           (opens juce::AlertWindow text-input dialog)
 *  
 *  Owns the `currentPresetName` and `currentPresetPack` state — the C++
 *  PresetManager doesn't track which preset is currently loaded. */
class PresetSelector : public juce::Component
{
public:
    PresetSelector(PhantomProcessor& processor,
                   juce::AudioProcessorValueTreeState& apvts);
    ~PresetSelector() override;

    /** Called when the user clicks "Browse". NativePluginEditor wires this
     *  to show its PresetBrowser overlay. */
    std::function<void()> onBrowseRequested;

    /** Updates the displayed preset name. Called by PresetBrowser when the
     *  user picks a preset there. */
    void setCurrentPreset(const juce::String& name, const juce::String& pack);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void prevPreset();
    void nextPreset();
    void saveDialog();

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::String currentPresetName;
    juce::String currentPresetPack;

    juce::TextButton prevButton  { "<"     };
    juce::TextButton nextButton  { ">"     };
    juce::TextButton browseButton{ "Browse" };
    juce::TextButton saveButton  { "Save"   };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetSelector)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/PresetSelector.cpp**

```cpp
// Source/UI/widgets/PresetSelector.cpp
#include "PresetSelector.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../PresetManager.h"

namespace kaigen::phantom
{

namespace
{
    /** Flatten all presets into an ordered list of (pack, name) pairs.
     *  Packs alphabetical; presets within each pack in PresetManager's order. */
    std::vector<std::pair<juce::String, juce::String>> flattenPresets(PhantomProcessor& processor)
    {
        std::vector<std::pair<juce::String, juce::String>> out;
        const auto all = processor.getPresetManager().getAllPresets();
        for (const auto& [packName, presets] : all)
            for (const auto& p : presets)
                out.emplace_back(packName, p.metadata.name);
        return out;
    }

    /** Index of (pack, name) in the flat list, or -1 if not found. */
    int findIndex(const std::vector<std::pair<juce::String, juce::String>>& flat,
                  const juce::String& pack, const juce::String& name)
    {
        for (size_t i = 0; i < flat.size(); ++i)
            if (flat[i].first == pack && flat[i].second == name)
                return (int) i;
        return -1;
    }
}

PresetSelector::PresetSelector(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    prevButton .onClick = [this] { prevPreset(); };
    nextButton .onClick = [this] { nextPreset(); };
    browseButton.onClick = [this] { if (onBrowseRequested) onBrowseRequested(); };
    saveButton .onClick = [this] { saveDialog(); };
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);
    addAndMakeVisible(browseButton);
    addAndMakeVisible(saveButton);
}

PresetSelector::~PresetSelector() = default;

void PresetSelector::setCurrentPreset(const juce::String& name, const juce::String& pack)
{
    currentPresetName = name;
    currentPresetPack = pack;
    repaint();
}

void PresetSelector::prevPreset()
{
    auto flat = flattenPresets(processor);
    if (flat.empty()) return;
    int idx = findIndex(flat, currentPresetPack, currentPresetName);
    if (idx < 0) idx = 0;
    else idx = (idx == 0) ? (int) flat.size() - 1 : idx - 1;
    const auto& [pack, name] = flat[(size_t) idx];
    if (processor.getPresetManager().loadPreset(apvts, name, pack))
        setCurrentPreset(name, pack);
}

void PresetSelector::nextPreset()
{
    auto flat = flattenPresets(processor);
    if (flat.empty()) return;
    int idx = findIndex(flat, currentPresetPack, currentPresetName);
    if (idx < 0) idx = 0;
    else idx = (idx + 1) % (int) flat.size();
    const auto& [pack, name] = flat[(size_t) idx];
    if (processor.getPresetManager().loadPreset(apvts, name, pack))
        setCurrentPreset(name, pack);
}

void PresetSelector::saveDialog()
{
    auto* aw = new juce::AlertWindow("Save Preset",
                                      "Enter a name for the new preset:",
                                      juce::MessageBoxIconType::QuestionIcon);
    aw->addTextEditor("name", currentPresetName.isEmpty() ? "New Preset" : currentPresetName);
    aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true,
        juce::ModalCallbackFunction::create([this, aw](int result)
        {
            std::unique_ptr<juce::AlertWindow> owned(aw);
            if (result != 1) return;
            const auto name = owned->getTextEditorContents("name").trim();
            if (name.isEmpty()) return;
            const auto saved = processor.getPresetManager().savePreset(
                apvts, name, "Experimental", "User", "", /*overwrite=*/false);
            if (saved.isNotEmpty())
                setCurrentPreset(saved, "User");
        }),
        true);
}

void PresetSelector::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    // Current preset name in center.
    g.setColour(Theme::textPrimary);
    g.setFont(juce::FontOptions("Space Grotesk", 13.0f, juce::Font::bold));
    const auto display = currentPresetName.isEmpty() ? juce::String("Default") : currentPresetName;
    auto labelArea = getLocalBounds().reduced(120, 0);
    g.drawText(display, labelArea.toFloat(), juce::Justification::centred, true);
}

void PresetSelector::resized()
{
    auto area = getLocalBounds().reduced(8, 4);

    // Left: prev / next
    prevButton.setBounds(area.removeFromLeft(28));
    area.removeFromLeft(2);
    nextButton.setBounds(area.removeFromLeft(28));

    // Right: save / browse (right-to-left layout)
    saveButton  .setBounds(area.removeFromRight(60));
    area.removeFromRight(4);
    browseButton.setBounds(area.removeFromRight(60));
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/widgets/PresetSelector.cpp` after `Source/UI/widgets/RecipeWheel.cpp`.

- [ ] **Step 4: Build to verify compile**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Class is unreferenced — Task 4 wires it in.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/widgets/PresetSelector.h Source/UI/widgets/PresetSelector.cpp CMakeLists.txt
git commit -m "feat(path-b): PresetSelector widget (prev/next/browse/save)"
```

(Exact commit message — no Claude trailer.)

---

## Task 2: PresetBrowser overlay component

**Files:**
- Create: `Source/UI/panels/PresetBrowser.h`
- Create: `Source/UI/panels/PresetBrowser.cpp`
- Modify: `CMakeLists.txt`

A child Component of NativePluginEditor that, when visible, fills the editor's MainArea (or whole editor) with a scrim + a card listing all presets grouped by pack. Click a preset row → load it + close. Click outside the card or the close button → close without loading.

For Phase 4 simplicity: NO scrolling (preset list is short enough to fit), NO search field, NO favorites toggle. Just a flat list of rows, scrollable internally if needed via `juce::Viewport`.

After Task 2: class compiles + links but isn't yet referenced. Task 4 wires it into NativePluginEditor.

- [ ] **Step 1: Create Source/UI/panels/PresetBrowser.h**

```cpp
// Source/UI/panels/PresetBrowser.h
#pragma once
#include <functional>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Overlay component: fills the editor when visible. Lists all presets
 *  grouped by pack; click a row to load that preset. Close button
 *  dismisses without loading.
 *  
 *  When the user picks a preset, fires `onPresetSelected(name, pack)`
 *  callback (NativePluginEditor wires this to update the PresetSelector's
 *  current-preset display). */
class PresetBrowser : public juce::Component
{
public:
    PresetBrowser(PhantomProcessor& processor,
                  juce::AudioProcessorValueTreeState& apvts);
    ~PresetBrowser() override;

    /** Fires when a preset row is clicked: (name, pack). */
    std::function<void(juce::String, juce::String)> onPresetSelected;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void visibilityChanged() override;

private:
    void loadPresetAt(int rowIndex);

    struct Row { juce::String name; juce::String pack; bool isHeader { false }; };
    std::vector<Row> rows;
    int hoverRow { -1 };

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton closeButton { "X" };

    static constexpr int kRowHeight     = 24;
    static constexpr int kHeaderHeight  = 28;
    static constexpr int kCardWidthPx   = 600;
    static constexpr int kCardMargin    = 40;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/panels/PresetBrowser.cpp**

```cpp
// Source/UI/panels/PresetBrowser.cpp
#include "PresetBrowser.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../PresetManager.h"

namespace kaigen::phantom
{

PresetBrowser::PresetBrowser(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    closeButton.onClick = [this] { setVisible(false); };
    addAndMakeVisible(closeButton);
}

PresetBrowser::~PresetBrowser() = default;

void PresetBrowser::visibilityChanged()
{
    if (! isVisible())
    {
        rows.clear();
        return;
    }

    // Rebuild row list each time the browser opens.
    rows.clear();
    const auto all = processor.getPresetManager().getAllPresets();
    for (const auto& [packName, presets] : all)
    {
        Row header;
        header.pack = packName;
        header.isHeader = true;
        rows.push_back(header);
        for (const auto& p : presets)
            rows.push_back({ p.metadata.name, packName, false });
    }
    repaint();
}

void PresetBrowser::loadPresetAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= (int) rows.size()) return;
    const auto& r = rows[(size_t) rowIndex];
    if (r.isHeader) return;
    if (processor.getPresetManager().loadPreset(apvts, r.name, r.pack))
    {
        if (onPresetSelected) onPresetSelected(r.name, r.pack);
        setVisible(false);
    }
}

void PresetBrowser::paint(juce::Graphics& g)
{
    // Backdrop scrim.
    g.fillAll(juce::Colour(0xc0000000));

    // Centered card.
    const auto bounds = getLocalBounds();
    const int cardW = juce::jmin(kCardWidthPx, bounds.getWidth() - 2 * kCardMargin);
    const int cardH = bounds.getHeight() - 2 * kCardMargin;
    const int cardX = (bounds.getWidth() - cardW) / 2;
    const int cardY = kCardMargin;
    auto cardBounds = juce::Rectangle<int>(cardX, cardY, cardW, cardH);

    g.setColour(Theme::matrixBg);
    g.fillRoundedRectangle(cardBounds.toFloat(), 6.0f);
    g.setColour(Theme::panelBorder);
    g.drawRoundedRectangle(cardBounds.toFloat(), 6.0f, 1.0f);

    // Title bar inside card.
    auto titleBar = cardBounds.removeFromTop(36).reduced(12, 8);
    g.setColour(Theme::textPrimary);
    g.setFont(juce::FontOptions("Space Grotesk", 14.0f, juce::Font::bold));
    g.drawText("Presets", titleBar.toFloat(), juce::Justification::centredLeft, false);

    // Row list inside card.
    auto rowArea = cardBounds.reduced(0, 8);
    int y = rowArea.getY();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (y + h > rowArea.getBottom()) break;  // simple clip; no scroll for v1

        auto rowBounds = juce::Rectangle<int>(rowArea.getX() + 8, y, rowArea.getWidth() - 16, h);

        if (r.isHeader)
        {
            g.setColour(Theme::textSecondary);
            g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::bold));
            g.drawText(r.pack.toUpperCase(), rowBounds.toFloat(), juce::Justification::centredLeft, false);
        }
        else
        {
            const bool hovered = ((int) i == hoverRow);
            if (hovered)
            {
                g.setColour(Theme::activeGlow);
                g.fillRoundedRectangle(rowBounds.toFloat(), 3.0f);
            }
            g.setColour(Theme::textPrimary);
            g.setFont(juce::FontOptions("Space Grotesk", 12.0f, juce::Font::plain));
            g.drawText(r.name, rowBounds.reduced(8, 0).toFloat(), juce::Justification::centredLeft, false);
        }
        y += h;
    }
}

void PresetBrowser::resized()
{
    const auto bounds = getLocalBounds();
    const int cardW = juce::jmin(kCardWidthPx, bounds.getWidth() - 2 * kCardMargin);
    const int cardX = (bounds.getWidth() - cardW) / 2;
    const int cardY = kCardMargin;

    closeButton.setBounds(cardX + cardW - 36, cardY + 6, 28, 24);
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e)
{
    const auto bounds = getLocalBounds();
    const int cardW = juce::jmin(kCardWidthPx, bounds.getWidth() - 2 * kCardMargin);
    const int cardH = bounds.getHeight() - 2 * kCardMargin;
    const int cardX = (bounds.getWidth() - cardW) / 2;
    const int cardY = kCardMargin;

    // Click outside the card → dismiss.
    if (e.x < cardX || e.x > cardX + cardW || e.y < cardY || e.y > cardY + cardH)
    {
        setVisible(false);
        return;
    }

    // Hit-test rows. Title bar at top 36px is non-interactive.
    const int rowAreaY = cardY + 36 + 8;
    const int rowAreaH = cardH - 36 - 16;
    if (e.y < rowAreaY || e.y > rowAreaY + rowAreaH) return;

    int y = rowAreaY;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const int h = r.isHeader ? kHeaderHeight : kRowHeight;
        if (e.y >= y && e.y < y + h)
        {
            loadPresetAt((int) i);
            return;
        }
        y += h;
    }
}

} // namespace kaigen::phantom
```

(Note: this implementation has no scroll support — the preset list is assumed to fit in the card. If preset count grows large, wrap the row area in a `juce::Viewport` with a child Component for the rows. Deferred to a polish pass.)

- [ ] **Step 3: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/panels/PresetBrowser.cpp` after `Source/UI/panels/LeftPanel.cpp`.

- [ ] **Step 4: Build to verify compile**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/panels/PresetBrowser.h Source/UI/panels/PresetBrowser.cpp CMakeLists.txt
git commit -m "feat(path-b): PresetBrowser overlay (preset list grouped by pack)"
```

(Exact commit message — no Claude trailer.)

---

## Task 3: TopBar component

**Files:**
- Create: `Source/UI/panels/TopBar.h`
- Create: `Source/UI/panels/TopBar.cpp`
- Modify: `CMakeLists.txt`

A simple Component that hosts the PresetSelector and (later, in a polish pass) any other top-bar widgets like advanced toggles, about/help buttons, etc. For Phase 4, it's basically a wrapper around PresetSelector.

After Task 3: class compiles + links but isn't yet referenced. Task 4 wires it into NativePluginEditor.

- [ ] **Step 1: Create Source/UI/panels/TopBar.h**

```cpp
// Source/UI/panels/TopBar.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PresetSelector.h"

class PhantomProcessor;

namespace kaigen::phantom
{

class TopBar : public juce::Component
{
public:
    TopBar(PhantomProcessor& processor, juce::AudioProcessorValueTreeState& apvts);
    ~TopBar() override;

    PresetSelector& getPresetSelector() noexcept { return presetSelector; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PresetSelector presetSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/panels/TopBar.cpp**

```cpp
// Source/UI/panels/TopBar.cpp
#include "TopBar.h"
#include "../Theme.h"

namespace kaigen::phantom
{

TopBar::TopBar(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : presetSelector(p, a)
{
    addAndMakeVisible(presetSelector);
}

TopBar::~TopBar() = default;

void TopBar::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);
    g.setColour(Theme::panelBorder);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float) getWidth());
}

void TopBar::resized()
{
    auto area = getLocalBounds();

    // Center the preset selector horizontally; ~600px wide.
    const int selectorW = juce::jmin(600, area.getWidth() - 200);
    const int selectorX = (area.getWidth() - selectorW) / 2;
    presetSelector.setBounds(selectorX, 0, selectorW, area.getHeight());
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/panels/TopBar.cpp` after `Source/UI/panels/PresetBrowser.cpp`.

- [ ] **Step 4: Build to verify compile**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/panels/TopBar.h Source/UI/panels/TopBar.cpp CMakeLists.txt
git commit -m "feat(path-b): TopBar component hosting PresetSelector"
```

(Exact commit message — no Claude trailer.)

---

## Task 4: Wire TopBar + PresetBrowser into NativePluginEditor

**Files:**
- Modify: `Source/UI/NativePluginEditor.h`
- Modify: `Source/UI/NativePluginEditor.cpp`

Add `TopBar` and `PresetBrowser` as members of `NativePluginEditor`. Wire the PresetSelector's `onBrowseRequested` callback to show the PresetBrowser. Wire the PresetBrowser's `onPresetSelected` callback to update the PresetSelector's current-preset display. Update `paint()` to remove the TopBar wireframe rectangle. Update `resized()` to position TopBar at the top + PresetBrowser to overlay the entire editor when visible.

- [ ] **Step 1: Update NativePluginEditor.h**

Add includes for the new panels:

```cpp
#include "panels/TopBar.h"
#include "panels/PresetBrowser.h"
```

Add members in the private section (after the existing `LeftPanel`/`RightPanel` members):

```cpp
TopBar topBar;
PresetBrowser presetBrowser;
```

- [ ] **Step 2: Update NativePluginEditor.cpp constructor**

The constructor already takes `(PhantomProcessor&, juce::AudioProcessorValueTreeState&)`. Update the init list to construct topBar and presetBrowser:

```cpp
NativePluginEditor::NativePluginEditor(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : juce::AudioProcessorEditor(&p),
      processor(p),
      apvts(a),
      rightPanel(a, p),
      leftPanel(a),
      topBar(p, a),
      presetBrowser(p, a)
{
    setSize(editorWidth, editorHeight);

    backToWebViewButton.addListener(this);
    addAndMakeVisible(backToWebViewButton);

    addAndMakeVisible(rightPanel);
    addAndMakeVisible(leftPanel);
    addAndMakeVisible(topBar);

    // Browser is added but starts hidden; clicked-to-show by Browse button.
    addAndMakeVisible(presetBrowser);
    presetBrowser.setVisible(false);
    presetBrowser.toFront(false);  // ensure it's painted on top of other panels

    // Wire PresetSelector callbacks via TopBar.
    topBar.getPresetSelector().onBrowseRequested = [this] {
        presetBrowser.setVisible(true);
        presetBrowser.toFront(false);
    };
    presetBrowser.onPresetSelected = [this](juce::String name, juce::String pack) {
        topBar.getPresetSelector().setCurrentPreset(name, pack);
    };
}
```

(Keep the existing constructor body, just add the new init-list entries and the new `addAndMakeVisible` calls + callback wiring.)

- [ ] **Step 3: Update NativePluginEditor.cpp paint() to remove TopBar wireframe**

Find the existing `paint()`. Remove the line that draws the labeled "TopBar" rectangle. Keep the ModulationPanel wireframe (Phase 5 will replace that).

```cpp
void NativePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::deepBg);

    auto area = getLocalBounds();

    // TopBar is a real Component now; skip the wireframe rect for it.
    area.removeFromTop(topBarHeight);

    // ModulationPanel still wireframe (Phase 5).
    auto modPanel = area.removeFromBottom(modPanelHeight);
    drawLabeledPanel(g, modPanel, "ModulationPanel (mode bar + slot row + engine labels)");

    // LeftPanel and RightPanel are real Components; no wireframes for them.
}
```

- [ ] **Step 4: Update NativePluginEditor.cpp resized() to position TopBar + PresetBrowser**

The TopBar fills the top `topBarHeight` strip. The PresetBrowser fills the entire editor (so its scrim covers everything when visible).

```cpp
void NativePluginEditor::resized()
{
    backToWebViewButton.setBounds(getWidth() - 110, 10, 100, 26);

    auto area = getLocalBounds();

    auto topBarArea = area.removeFromTop(topBarHeight);
    topBar.setBounds(topBarArea);

    area.removeFromBottom(modPanelHeight);

    auto leftBounds = area.removeFromLeft(leftPanelWidth);
    leftPanel.setBounds(leftBounds);
    rightPanel.setBounds(area);

    // PresetBrowser overlays the entire editor when visible.
    presetBrowser.setBounds(getLocalBounds());
}
```

- [ ] **Step 5: Build VST3 + Standalone**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: builds clean. VST3 reinstalls.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp
git commit -m "feat(path-b): wire TopBar + PresetBrowser into NativePluginEditor"
```

(Exact commit message — no Claude trailer.)

---

## Task 5: Manual smoke + final review

**Files:** none modified — verification only.

End-to-end smoke for Phase 4.

- [ ] **Step 1: Build VST3 + Standalone**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_VST3
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expect: both build clean. VST3 auto-installs.

- [ ] **Step 2: Toggle to native editor in Live**

Open WebView2 editor in Live. Shift+click PHANTOM logo. Reopen plugin window. Expect native editor with Phase 1+2+3+4 widgets.

- [ ] **Step 3: Verify TopBar shows preset selector**

- The TopBar (top of editor) is no longer a wireframe rectangle
- Centered: a text label showing "Default" (or whatever the last preset name was)
- Left of label: prev (`<`) and next (`>`) buttons
- Right of label: "Browse" and "Save" buttons

- [ ] **Step 4: Verify prev/next**

- Click `>` (next) → another preset loads, label updates to its name, the editor knobs update to that preset's values
- Click `<` (prev) → previous preset loads, label updates
- At list edges, wrapping should occur

- [ ] **Step 5: Verify Browse**

- Click "Browse" → a dark scrim overlays the editor, a centered card appears listing presets grouped by pack header
- Click a preset row → it loads, browser closes, label updates
- Click outside the card OR the X button → browser closes without loading

- [ ] **Step 6: Verify Save**

- Modify some knob values
- Click "Save" → an AlertWindow dialog appears with a text input prefilled with the current preset name
- Type a new name and click Save → preset is saved to User pack
- Open the Browse list → the new preset appears under "User" pack header
- Click it → it loads, knobs return to the saved values

- [ ] **Step 7: No commit needed if smoke passes**

Phase 4 done. Phase 5 (modulation system — slot row + matrix view) is next.

---

## Self-review notes

### Spec coverage

Phase 4 covers per the spec's "Phase 4 — Preset system" line item:
- ✓ Native preset browser/save/load UI replacing `preset-system.js`

Scope cuts (deferred to polish pass):
- Search box in browser
- Favorites toggle (no UI; data still tracked by PresetManager)
- Delete preset button
- Tag filtering
- Designer/description/type fields in save dialog
- Preset preview thumbnails (the 7-harmonic mini-graph visible in WebView2 browser)

### Placeholder scan

No "TBD"/"TODO"/vague language. Each step has runnable code or commands.

### Type / signature consistency

- `PresetSelector(PhantomProcessor&, juce::AudioProcessorValueTreeState&)` constructor consistent across Tasks 1, 3, 4.
- `PresetBrowser(PhantomProcessor&, juce::AudioProcessorValueTreeState&)` constructor consistent across Tasks 2, 4.
- `TopBar(PhantomProcessor&, juce::AudioProcessorValueTreeState&)` constructor consistent across Tasks 3, 4.
- Callback signatures: `PresetSelector::onBrowseRequested = std::function<void()>;` and `PresetBrowser::onPresetSelected = std::function<void(juce::String, juce::String)>;` — used consistently in Task 4 wiring.
- `processor.getPresetManager()` accessor: verified in `Source/PluginProcessor.h` (used by existing `getAllPresets` binding).
- PresetManager methods (`getAllPresets`, `loadPreset`, `savePreset`) match the existing API in `Source/PresetManager.h:86-110`.

### What ships in Phase 4

After this plan completes:
- Native editor's TopBar shows the current preset name + prev/next/browse/save controls
- Browse opens a card-style modal listing presets grouped by pack
- Save opens a stock JUCE AlertWindow asking for a name
- All operations call into the existing C++ PresetManager directly (no IPC, no JSON)

Phase 5 (modulation system: slot row + matrix view) is next.
