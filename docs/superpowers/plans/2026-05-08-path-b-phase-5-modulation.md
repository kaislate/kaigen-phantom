# Path B — Phase 5: Modulation System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the WebView2 modulation panel + matrix-view (`modulation-panel.js` + `matrix.js` + `live-modulation.js` ≈ 1,000 lines of JS) with a native ModulationPanel containing a SLOTS/MATRIX mode bar, an 11-slot row with macro knobs, and a MatrixView overlay supporting all the routing interactions: click empty → AddRouting, drag → SetRoutingDepth, right-click popover, inline macro name editor, live cell pulse on modulation, slot-click handoff to matrix mode with row highlight.

**Architecture:** A `ModulationPanel` Component takes the place of the bottom wireframe in `NativePluginEditor`. Its mode bar's "MATRIX" button toggles a `MatrixView` overlay that fills the editor area when active (similar pattern to Phase 4's PresetBrowser overlay). All routing CRUD calls into the existing C++ `ModulationEngine` API directly via `processor.getModulationEngineA()` / `getModulationEngineB()` — no JSON, no IPC, no JS. Live cell pulse uses a 10 Hz `juce::Timer` reading macro values via the existing `Modulator::getCurrentValue()` cached atomic.

**Tech Stack:** JUCE 8.0.4 (`juce_graphics`, `juce_gui_basics`), C++20, MSVC 17.14. Reuses existing `ModulationEngine` (lock-free routing snapshots, RT-safe `getModulatedValue`), `Macro` (cached `std::atomic<float>*`), and `MatrixViewState` (mode + per-engine expanded categories) infrastructure.

**Branch:** continue on `feature/pr3b-macro-editor` (additive; default behavior unchanged for users without native editor enabled).

**Spec:** `docs/superpowers/specs/2026-05-07-path-b-native-ui-design.md`. Phases 0-4 complete.

**Visual fidelity note:** Phase 5 ships functional modulation UI — knob slots, matrix grid, cells, popovers. Visual polish (animations, gradient quality, spacing, typography) deferred to the polish pass before Phase 6 cutover.

**Scope cuts vs. WebView2 implementation:**
- LFO and Random slots stay as static greyed placeholders (PR4/PR5 territory — not part of Path B).
- Engine A/B switching of matrix view is via existing engine focus tab; no separate per-engine matrix toggle.
- Slide/fade animation when matrix opens/closes deferred to polish pass.
- Cell glow gradient quality deferred to polish.

---

## Existing C++ infrastructure (DO NOT modify)

From `Source/Modulation/ModulationEngine.h`:

```cpp
class ModulationEngine
{
public:
    bool addRouting(const Routing& r);  // false if param wrong prefix or unknown source
    void removeRouting(const juce::String& sourceId, const juce::String& paramId);
    bool setRoutingDepth(const juce::String& sourceId, const juce::String& paramId, float newDepth);
    std::vector<Routing> getRoutings() const;
    Modulator* findModulator(const juce::String& sourceId) const;
    // ...
};
```

`Routing` struct (`Source/Modulation/Routing.h`): `sourceId`, `paramId`, `depth` (-1..+1), `polarityInverted`.

`Macro` (`Source/Modulation/Macro.h`): inherits from `Modulator`, has `getId()`, `getName()`, `setName(juce::String)`, `getCurrentValue()` (cached atomic load).

Processor accessors at `Source/PluginProcessor.h`:
- `getModulationEngineA()` / `getModulationEngineB()` — for routing CRUD
- `getMatrixView()` / `setMatrixView(state)` — persisted view state (mode + expandedA + expandedB)

Existing macro APVTS params: `macro1`, `macro2`, `macro3`, `macro4` (global, not per-engine).

---

## File Structure

| File | Status | Responsibility |
|------|--------|----------------|
| `Source/UI/widgets/ModSlot.h` | **Create** | Single slot widget for the slot row — variants: macro (PhantomKnob bound to macroN + ring overlay), morph (PhantomKnob bound to morph_amount), lfo-placeholder, random-placeholder. |
| `Source/UI/widgets/ModSlot.cpp` | **Create** | Implementation. Macro variant wraps a `PhantomKnob` member. Click handler emits `onSlotClicked(slotId)` callback (used in Task 7 for matrix handoff). |
| `Source/UI/panels/ModulationPanel.h` | **Create** | Bottom panel container. Houses ModeBar (SLOTS/MATRIX toggle), SlotRow (11 ModSlots), engine labels. |
| `Source/UI/panels/ModulationPanel.cpp` | **Create** | Implementation. |
| `Source/UI/panels/MatrixView.h` | **Create** | Overlay component (similar pattern to PresetBrowser). Houses ModulatorStrip + DestinationGrid. Toggled visible/hidden by ModulationPanel's MATRIX button. |
| `Source/UI/panels/MatrixView.cpp` | **Create** | Implementation. |
| `Source/UI/widgets/MatrixCell.h` | **Create** | Single grid cell. Reads routing depth, paints bipolar fill + numeric label. Mouse handlers for click/drag/right-click. |
| `Source/UI/widgets/MatrixCell.cpp` | **Create** | Implementation. |
| `Source/UI/widgets/MatrixModRow.h` | **Create** | Single modulator row (left strip): icon + name + value readout. Macro rows have inline name editor. |
| `Source/UI/widgets/MatrixModRow.cpp` | **Create** | Implementation. |
| `Source/UI/NativePluginEditor.h` | **Modify** | Add `ModulationPanel modulationPanel;` and `MatrixView matrixView;` members. |
| `Source/UI/NativePluginEditor.cpp` | **Modify** | Construct modulationPanel + matrixView. Wire ModulationPanel's `onMatrixToggle` callback to show/hide matrixView. Update `paint()` to remove the ModulationPanel wireframe rectangle. Update `resized()` to position both. |
| `CMakeLists.txt` | **Modify** | Add new .cpp files to `target_sources(KaigenPhantom)`. |

---

## Task 1: ModulationPanel scaffold + ModeBar

**Files:**
- Create: `Source/UI/panels/ModulationPanel.h`
- Create: `Source/UI/panels/ModulationPanel.cpp`
- Modify: `Source/UI/NativePluginEditor.h`
- Modify: `Source/UI/NativePluginEditor.cpp`
- Modify: `CMakeLists.txt`

The ModulationPanel is a `juce::Component` that lives at the bottom of the editor (replacing the wireframe placeholder). For Task 1 it has just the mode bar (SLOTS/MATRIX toggle buttons + a routing counter label) — slot row + matrix view come in Tasks 2-3. The MATRIX button emits an `onMatrixToggle(bool active)` callback that NativePluginEditor wires to show/hide the MatrixView overlay (Task 3 creates the overlay; Task 1 just emits the callback).

After Task 1: native editor's bottom panel shows two buttons (SLOTS, MATRIX) with a counter "0 ROUTINGS" — but slot row and matrix view are not yet implemented, so clicking MATRIX has no visible effect yet.

- [ ] **Step 1: Create Source/UI/panels/ModulationPanel.h**

```bash
mkdir -p "Source/UI/panels"
```

```cpp
// Source/UI/panels/ModulationPanel.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Bottom panel housing the modulation system UI:
 *  - Mode bar: SLOTS / MATRIX toggle buttons + counter
 *  - Slot row: 11 modulator slots (Task 2 wires)
 *  - Matrix view: overlay component (Task 3 wires)
 *  
 *  Replaces the WebView2 modulation-panel.js + matrix.js. */
class ModulationPanel : public juce::Component
{
public:
    ModulationPanel(PhantomProcessor& processor,
                    juce::AudioProcessorValueTreeState& apvts);
    ~ModulationPanel() override;

    /** Fires when the user clicks MATRIX. Boolean argument: new active state. */
    std::function<void(bool)> onMatrixToggle;

    /** Update the routing counter (called by NativePluginEditor on state change). */
    void setRoutingCount(int count);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton slotsButton  { "SLOTS"  };
    juce::TextButton matrixButton { "MATRIX" };
    juce::Label      counterLabel;
    bool matrixActive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationPanel)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/panels/ModulationPanel.cpp**

```cpp
// Source/UI/panels/ModulationPanel.cpp
#include "ModulationPanel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

ModulationPanel::ModulationPanel(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    slotsButton .setClickingTogglesState(true);
    slotsButton .setToggleState(true, juce::dontSendNotification);
    matrixButton.setClickingTogglesState(true);

    slotsButton .onClick = [this] {
        slotsButton.setToggleState(true, juce::dontSendNotification);
        // Slot view is the default; MATRIX toggle only deactivates matrix.
        if (matrixActive) {
            matrixActive = false;
            matrixButton.setToggleState(false, juce::dontSendNotification);
            if (onMatrixToggle) onMatrixToggle(false);
        }
    };
    matrixButton.onClick = [this] {
        matrixActive = ! matrixActive;
        matrixButton.setToggleState(matrixActive, juce::dontSendNotification);
        slotsButton .setToggleState(! matrixActive, juce::dontSendNotification);
        if (onMatrixToggle) onMatrixToggle(matrixActive);
    };

    addAndMakeVisible(slotsButton);
    addAndMakeVisible(matrixButton);

    counterLabel.setColour(juce::Label::textColourId, Theme::textSecondary);
    counterLabel.setFont(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold));
    counterLabel.setText("0 ROUTINGS", juce::dontSendNotification);
    addAndMakeVisible(counterLabel);
}

ModulationPanel::~ModulationPanel() = default;

void ModulationPanel::setRoutingCount(int count)
{
    counterLabel.setText(juce::String(count) + (count == 1 ? " ROUTING" : " ROUTINGS"),
                          juce::dontSendNotification);
}

void ModulationPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);
    g.setColour(Theme::panelBorder);
    g.drawHorizontalLine(0, 0.0f, (float) getWidth());
}

void ModulationPanel::resized()
{
    auto area = getLocalBounds();

    // Mode bar at top of panel.
    auto modeBar = area.removeFromTop(30).reduced(12, 4);
    slotsButton .setBounds(modeBar.removeFromLeft(64));
    modeBar.removeFromLeft(4);
    matrixButton.setBounds(modeBar.removeFromLeft(64));

    counterLabel.setBounds(modeBar.removeFromRight(140));
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/panels/ModulationPanel.cpp` after `Source/UI/panels/TopBar.cpp`.

- [ ] **Step 4: Update NativePluginEditor.h**

Add include + member:

```cpp
#include "panels/ModulationPanel.h"
```

In private section, alongside other panels:

```cpp
ModulationPanel modulationPanel;
```

- [ ] **Step 5: Update NativePluginEditor.cpp**

Add to init list (in declaration order — match where you placed it in the header):

```cpp
modulationPanel(p, a)
```

Add to constructor body:

```cpp
addAndMakeVisible(modulationPanel);
```

In `paint()`, REMOVE the line that draws the labeled "ModulationPanel" wireframe rectangle. The full updated `paint()`:

```cpp
void NativePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::deepBg);
    auto area = getLocalBounds();
    area.removeFromTop(topBarHeight);
    area.removeFromBottom(modPanelHeight);
    // LeftPanel and RightPanel are real Components — no wireframes.
    // ModulationPanel is now real too.
}
```

In `resized()`, position the modulation panel where the wireframe used to be (bottom `modPanelHeight` strip):

```cpp
void NativePluginEditor::resized()
{
    backToWebViewButton.setBounds(getWidth() - 110, 10, 100, 26);

    auto area = getLocalBounds();

    auto topBarArea = area.removeFromTop(topBarHeight);
    topBar.setBounds(topBarArea);

    auto modPanelArea = area.removeFromBottom(modPanelHeight);
    modulationPanel.setBounds(modPanelArea);

    auto leftBounds = area.removeFromLeft(leftPanelWidth);
    leftPanel.setBounds(leftBounds);
    rightPanel.setBounds(area);

    presetBrowser.setBounds(getLocalBounds());
}
```

- [ ] **Step 6: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Native editor's bottom panel shows two buttons (SLOTS active by default, MATRIX inactive) and a "0 ROUTINGS" label. No wireframe.

- [ ] **Step 7: Commit**

```bash
git add Source/UI/panels/ModulationPanel.h Source/UI/panels/ModulationPanel.cpp Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp CMakeLists.txt
git commit -m "feat(path-b): ModulationPanel scaffold + ModeBar (SLOTS/MATRIX toggle)"
```

(Exact commit message — no Claude trailer.)

---

## Task 2: SlotRow with 11 slots (4 macros + morph + 6 placeholders)

**Files:**
- Create: `Source/UI/widgets/ModSlot.h`
- Create: `Source/UI/widgets/ModSlot.cpp`
- Modify: `Source/UI/panels/ModulationPanel.h`
- Modify: `Source/UI/panels/ModulationPanel.cpp`
- Modify: `CMakeLists.txt`

Add the slot row below the mode bar. 11 slots in the order the WebView2 uses:
`LFO1, LFO2, RAND_A, MAC1, MAC2, MORPH, MAC3, MAC4, RAND_B, LFO3, LFO4`

Macro slots (MAC1-4) each contain a `PhantomKnob` bound to `macro1`-`macro4` APVTS params. Morph slot contains a PhantomKnob bound to `morph_amount`. LFO/Random slots are static placeholders (greyed dot + "PR4" / "PR5" text label) — not interactive in this PR.

Each slot has an `onSlotClicked(slotId)` callback. Task 7 wires this to the slot-click → matrix handoff (clicking a macro slot in SLOTS mode jumps to MATRIX mode with that row highlighted).

For Phase 5: macro slots show their value via the bound PhantomKnob (already animated via APVTS attachment from Phase 1). No separate live ring overlay needed.

- [ ] **Step 1: Create Source/UI/widgets/ModSlot.h**

```cpp
// Source/UI/widgets/ModSlot.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PhantomKnob.h"

namespace kaigen::phantom
{

/** Single slot in the modulation panel's slot row.
 *  Variants:
 *    - macro: contains a PhantomKnob bound to macroN APVTS param
 *    - morph: PhantomKnob bound to morph_amount
 *    - lfo / random: static placeholder (greyed text label)
 *  
 *  Click anywhere on the slot fires onSlotClicked(slotId) — Task 7 wires
 *  this to the slot→matrix handoff. */
class ModSlot : public juce::Component
{
public:
    enum class Type { Macro, Morph, Lfo, Random };

    /** Constructor.
     *  @param apvts        APVTS for parameter binding (macro/morph types)
     *  @param type         Slot type
     *  @param slotId       e.g., "macro1", "morph", "lfo1", "randomA"
     *  @param paramID      APVTS param to bind (macro/morph types only; "" for placeholders)
     *  @param label        Display label, e.g., "MAC 1", "MORPH", "LFO 1"
     *  @param placeholder  Optional placeholder text shown for non-functional slots */
    ModSlot(juce::AudioProcessorValueTreeState& apvts,
            Type type,
            const juce::String& slotId,
            const juce::String& paramID,
            const juce::String& label,
            const juce::String& placeholder = {});

    ~ModSlot() override;

    /** Fires when the slot is clicked. Argument: slotId. */
    std::function<void(juce::String)> onSlotClicked;

    /** Slot id (e.g., "macro1") — used for matrix-row highlight in Task 7. */
    const juce::String& getSlotId() const noexcept { return slotId; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    Type type;
    juce::String slotId;
    juce::String label;
    juce::String placeholder;

    std::unique_ptr<PhantomKnob> knob;  // only for macro/morph

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModSlot)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/ModSlot.cpp**

```cpp
// Source/UI/widgets/ModSlot.cpp
#include "ModSlot.h"
#include "../Theme.h"

namespace kaigen::phantom
{

ModSlot::ModSlot(juce::AudioProcessorValueTreeState& apvts,
                 Type t,
                 const juce::String& sId,
                 const juce::String& paramID,
                 const juce::String& lbl,
                 const juce::String& ph)
    : type(t), slotId(sId), label(lbl), placeholder(ph)
{
    if (type == Type::Macro || type == Type::Morph)
    {
        knob = std::make_unique<PhantomKnob>(apvts, paramID, PhantomKnob::Size::Small,
                                              juce::String{});  // no label on knob; we draw our own
        addAndMakeVisible(*knob);
    }
}

ModSlot::~ModSlot() = default;

void ModSlot::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Color-coded border based on type.
    juce::Colour accent;
    switch (type)
    {
        case Type::Macro:  accent = Theme::macroTeal;    break;
        case Type::Morph:  accent = Theme::morphWhite;   break;
        case Type::Lfo:    accent = Theme::lfoBlue;      break;
        case Type::Random: accent = Theme::randomPurple; break;
    }

    // Label below the slot's interactive area.
    auto labelArea = bounds.removeFromBottom(14);
    g.setColour(accent.withAlpha(type == Type::Lfo || type == Type::Random ? 0.5f : 0.85f));
    g.setFont(juce::FontOptions("Space Grotesk", 9.0f, juce::Font::bold));
    g.drawText(label, labelArea.toFloat(), juce::Justification::centred, false);

    // For LFO/Random placeholders: draw a greyed "PR4" / "PR5" stub.
    if (type == Type::Lfo || type == Type::Random)
    {
        g.setColour(Theme::textDim);
        g.setFont(juce::FontOptions("Space Grotesk", 8.0f, juce::Font::plain));
        const auto stub = placeholder.isEmpty() ? juce::String("--") : placeholder;
        g.drawText(stub, bounds.toFloat(), juce::Justification::centred, false);

        // Type-color dot indicator.
        g.setColour(accent.withAlpha(0.4f));
        const float dotR = 4.0f;
        const auto centre = bounds.getCentre().toFloat();
        g.fillEllipse(centre.x - dotR, centre.y + 8.0f, dotR * 2.0f, dotR * 2.0f);
    }
}

void ModSlot::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(14);  // reserve label area

    if (knob != nullptr)
        knob->setBounds(bounds);
}

void ModSlot::mouseDown(const juce::MouseEvent&)
{
    if (onSlotClicked) onSlotClicked(slotId);
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Update Source/UI/panels/ModulationPanel.h**

Add include for ModSlot:

```cpp
#include "../widgets/ModSlot.h"
```

Add slot row members in private section:

```cpp
juce::OwnedArray<ModSlot> slots;
```

Add a public accessor for the slot row (used by NativePluginEditor in Task 7 for handoff):

```cpp
public:
    /** Allows external code (NativePluginEditor) to install onSlotClicked
     *  handlers on each slot. */
    juce::OwnedArray<ModSlot>& getSlots() noexcept { return slots; }
```

- [ ] **Step 4: Update Source/UI/panels/ModulationPanel.cpp**

In the constructor (after the existing addAndMakeVisible calls), build the slot row:

```cpp
// Slot row: 11 slots in WebView2 order.
struct SlotDef { ModSlot::Type type; const char* slotId; const char* paramID; const char* label; const char* placeholder; };
const SlotDef defs[] = {
    { ModSlot::Type::Lfo,    "lfo1",    "",          "LFO 1", "PR4" },
    { ModSlot::Type::Lfo,    "lfo2",    "",          "LFO 2", "PR4" },
    { ModSlot::Type::Random, "randomA", "",          "RAND",  "PR5" },
    { ModSlot::Type::Macro,  "macro1",  "macro1",    "MAC 1", "" },
    { ModSlot::Type::Macro,  "macro2",  "macro2",    "MAC 2", "" },
    { ModSlot::Type::Morph,  "morph",   "morph_amount", "MORPH", "" },
    { ModSlot::Type::Macro,  "macro3",  "macro3",    "MAC 3", "" },
    { ModSlot::Type::Macro,  "macro4",  "macro4",    "MAC 4", "" },
    { ModSlot::Type::Random, "randomB", "",          "RAND",  "PR5" },
    { ModSlot::Type::Lfo,    "lfo3",    "",          "LFO 3", "PR4" },
    { ModSlot::Type::Lfo,    "lfo4",    "",          "LFO 4", "PR4" },
};
for (const auto& def : defs)
{
    auto* slot = new ModSlot(apvts, def.type, def.slotId, def.paramID, def.label, def.placeholder);
    addAndMakeVisible(*slot);
    slots.add(slot);
}
```

In `resized()`, lay out the slot row below the mode bar:

```cpp
void ModulationPanel::resized()
{
    auto area = getLocalBounds();

    // Mode bar at top of panel.
    auto modeBar = area.removeFromTop(30).reduced(12, 4);
    slotsButton .setBounds(modeBar.removeFromLeft(64));
    modeBar.removeFromLeft(4);
    matrixButton.setBounds(modeBar.removeFromLeft(64));
    counterLabel.setBounds(modeBar.removeFromRight(140));

    // Slot row below mode bar — 11 slots evenly distributed.
    auto slotRow = area.reduced(12, 4);
    if (slots.isEmpty()) return;
    const int slotW = slotRow.getWidth() / slots.size();
    for (auto* slot : slots)
        slot->setBounds(slotRow.removeFromLeft(slotW));
}
```

- [ ] **Step 5: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/widgets/ModSlot.cpp` after `Source/UI/widgets/PresetSelector.cpp`.

- [ ] **Step 6: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Native editor's bottom panel shows the mode bar at top, then 11 slots in a row. The 4 macro slots and morph slot have working knobs; the 6 placeholder slots show "PR4"/"PR5" text and a greyed type-colored dot.

- [ ] **Step 7: Commit**

```bash
git add Source/UI/widgets/ModSlot.h Source/UI/widgets/ModSlot.cpp Source/UI/panels/ModulationPanel.h Source/UI/panels/ModulationPanel.cpp CMakeLists.txt
git commit -m "feat(path-b): SlotRow with 11 mod slots (4 macros + morph + 6 placeholders)"
```

(Exact commit message — no Claude trailer.)

---

## Task 3: MatrixView overlay scaffold

**Files:**
- Create: `Source/UI/panels/MatrixView.h`
- Create: `Source/UI/panels/MatrixView.cpp`
- Modify: `Source/UI/NativePluginEditor.h`
- Modify: `Source/UI/NativePluginEditor.cpp`
- Modify: `CMakeLists.txt`

The MatrixView is an overlay component (similar pattern to PresetBrowser from Phase 4) that becomes visible when the user clicks MATRIX in the ModulationPanel's mode bar. For Task 3 it's a scaffold — fills the editor area with a backdrop scrim and a centered card with placeholder text. Task 4 adds the modulator strip on the left; Task 5 adds the destination grid on the right.

After Task 3: clicking MATRIX shows a card overlay over the editor. Clicking outside the card (or pressing the backdrop) dismisses it.

- [ ] **Step 1: Create Source/UI/panels/MatrixView.h**

```cpp
// Source/UI/panels/MatrixView.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Modulation matrix view — overlay component that fills the editor area
 *  when active. Houses ModulatorStrip (left) + DestinationGrid (right).
 *  
 *  Toggled visible/hidden by ModulationPanel's MATRIX button. */
class MatrixView : public juce::Component
{
public:
    MatrixView(PhantomProcessor& processor,
               juce::AudioProcessorValueTreeState& apvts);
    ~MatrixView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void visibilityChanged() override;

private:
    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    static constexpr int kCardMargin = 30;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixView)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/panels/MatrixView.cpp**

```cpp
// Source/UI/panels/MatrixView.cpp
#include "MatrixView.h"
#include "../Theme.h"

namespace kaigen::phantom
{

MatrixView::MatrixView(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
}

MatrixView::~MatrixView() = default;

void MatrixView::visibilityChanged()
{
    if (isVisible()) repaint();
}

void MatrixView::paint(juce::Graphics& g)
{
    // Backdrop scrim.
    g.fillAll(juce::Colour(0xc0000000));

    // Centered card.
    const auto bounds = getLocalBounds();
    auto card = bounds.reduced(kCardMargin);

    g.setColour(Theme::matrixBg);
    g.fillRoundedRectangle(card.toFloat(), 6.0f);
    g.setColour(Theme::panelBorder);
    g.drawRoundedRectangle(card.toFloat(), 6.0f, 1.0f);

    // Placeholder title bar — Tasks 4-5 replace this with real content.
    auto titleBar = card.removeFromTop(40).reduced(16, 8);
    g.setColour(Theme::textPrimary);
    g.setFont(juce::FontOptions("Space Grotesk", 14.0f, juce::Font::bold));
    g.drawText("Modulation Matrix", titleBar.toFloat(), juce::Justification::centredLeft, false);

    g.setColour(Theme::textDim);
    g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::plain));
    g.drawText("Modulator strip + destination grid (Tasks 4-5)",
               card.toFloat(), juce::Justification::centred, false);
}

void MatrixView::resized()
{
    // Tasks 4-5 add child components.
}

void MatrixView::mouseDown(const juce::MouseEvent& e)
{
    // Click outside the card → dismiss.
    const auto bounds = getLocalBounds();
    const auto card = bounds.reduced(kCardMargin);
    if (! card.contains(e.x, e.y))
        setVisible(false);
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/panels/MatrixView.cpp` after `Source/UI/panels/ModulationPanel.cpp`.

- [ ] **Step 4: Update NativePluginEditor.h**

Add include + member:

```cpp
#include "panels/MatrixView.h"
```

In private section (after modulationPanel):

```cpp
MatrixView matrixView;
```

- [ ] **Step 5: Update NativePluginEditor.cpp**

Add to init list (in declaration order):

```cpp
matrixView(p, a)
```

Add to constructor body (after the existing `addAndMakeVisible(modulationPanel)`):

```cpp
addAndMakeVisible(matrixView);
matrixView.setVisible(false);
matrixView.toFront(false);

// Wire ModulationPanel's MATRIX toggle to show/hide matrixView.
modulationPanel.onMatrixToggle = [this](bool active) {
    matrixView.setVisible(active);
    if (active) matrixView.toFront(false);
};
```

In `resized()`, position the matrixView (it overlays the entire editor, similar to PresetBrowser):

```cpp
matrixView.setBounds(getLocalBounds());
```

(Add this line at the end of resized(), alongside the existing `presetBrowser.setBounds(getLocalBounds());`.)

- [ ] **Step 6: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Native editor: click MATRIX in mode bar → matrix overlay appears with title "Modulation Matrix" and placeholder text. Click outside the card → dismisses. Click MATRIX again to re-toggle.

- [ ] **Step 7: Commit**

```bash
git add Source/UI/panels/MatrixView.h Source/UI/panels/MatrixView.cpp Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp CMakeLists.txt
git commit -m "feat(path-b): MatrixView overlay scaffold"
```

(Exact commit message — no Claude trailer.)

---

## Task 4: ModulatorStrip with 10 modulator rows

**Files:**
- Create: `Source/UI/widgets/MatrixModRow.h`
- Create: `Source/UI/widgets/MatrixModRow.cpp`
- Modify: `Source/UI/panels/MatrixView.h`
- Modify: `Source/UI/panels/MatrixView.cpp`
- Modify: `CMakeLists.txt`

Add the modulator strip on the left side of the matrix card. 10 rows: macro1-2 + lfo1-2 + randomA (engine A side, top 5) + macro3-4 + lfo3-4 + randomB (engine B, bottom 5). Each row shows: type-color icon dot, modulator name (e.g. "MAC 1"), and a small value readout.

For Phase 5, only macros are interactive (name editor in Task 7). LFO/Random rows are greyed.

After Task 4: matrix view's left strip shows 10 rows with names + value readouts. The right side (destination grid) is still placeholder.

- [ ] **Step 1: Create Source/UI/widgets/MatrixModRow.h**

```cpp
// Source/UI/widgets/MatrixModRow.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ModSlot.h"  // reuses ModSlot::Type enum

class PhantomProcessor;

namespace kaigen::phantom
{

/** Single modulator row in the MatrixView's left strip.
 *  Shows: type-color icon dot + modulator name + value readout.
 *  Macro rows have an inline name editor (Task 7 wires).
 *  LFO/Random rows are greyed placeholders (PR4/PR5). */
class MatrixModRow : public juce::Component
{
public:
    MatrixModRow(PhantomProcessor& processor,
                 ModSlot::Type type,
                 const juce::String& modId,    // e.g., "macro1", "lfo1"
                 const juce::String& label);   // e.g., "MAC 1"
    ~MatrixModRow() override;

    /** Modulator id — used by Task 5 to compute cell positions. */
    const juce::String& getModId() const noexcept { return modId; }

    /** Update the value readout (Task 7 wires this to the live tick). */
    void setValueText(const juce::String& text);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PhantomProcessor& processor;
    ModSlot::Type type;
    juce::String modId;
    juce::String label;
    juce::String valueText { "0.00" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixModRow)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/MatrixModRow.cpp**

```cpp
// Source/UI/widgets/MatrixModRow.cpp
#include "MatrixModRow.h"
#include "../Theme.h"

namespace kaigen::phantom
{

MatrixModRow::MatrixModRow(PhantomProcessor& p, ModSlot::Type t,
                            const juce::String& id, const juce::String& lbl)
    : processor(p), type(t), modId(id), label(lbl)
{
}

MatrixModRow::~MatrixModRow() = default;

void MatrixModRow::setValueText(const juce::String& text)
{
    if (text != valueText)
    {
        valueText = text;
        repaint();
    }
}

void MatrixModRow::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().reduced(4, 2);

    // Type-color border.
    juce::Colour accent;
    switch (type)
    {
        case ModSlot::Type::Macro:  accent = Theme::macroTeal;    break;
        case ModSlot::Type::Lfo:    accent = Theme::lfoBlue;      break;
        case ModSlot::Type::Random: accent = Theme::randomPurple; break;
        case ModSlot::Type::Morph:  accent = Theme::morphWhite;   break;
    }

    const float alpha = (type == ModSlot::Type::Macro) ? 1.0f : 0.45f;

    // Left dot.
    g.setColour(accent.withAlpha(alpha));
    const float dotR = 4.0f;
    const auto centreY = bounds.getCentreY();
    g.fillEllipse(bounds.getX() + 4.0f, (float) centreY - dotR, dotR * 2.0f, dotR * 2.0f);

    // Name.
    g.setFont(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::bold));
    g.drawText(label, bounds.withTrimmedLeft(20).withWidth(60).toFloat(),
               juce::Justification::centredLeft, false);

    // Value readout (right-aligned).
    g.setColour(Theme::textSecondary);
    g.setFont(juce::FontOptions("Space Grotesk", 9.0f, juce::Font::plain));
    g.drawText(valueText, bounds.withTrimmedRight(4).toFloat(),
               juce::Justification::centredRight, false);
}

void MatrixModRow::resized()
{
    // No children for now.
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Update MatrixView.h**

Add include + member:

```cpp
#include "../widgets/MatrixModRow.h"
```

In private section:

```cpp
juce::OwnedArray<MatrixModRow> modRows;
```

- [ ] **Step 4: Update MatrixView.cpp**

In the constructor, build the 10 modulator rows:

```cpp
MatrixView::MatrixView(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a)
{
    struct ModDef { ModSlot::Type type; const char* modId; const char* label; };
    const ModDef defs[] = {
        // Engine A modulators
        { ModSlot::Type::Macro,  "macro1",  "MAC 1" },
        { ModSlot::Type::Macro,  "macro2",  "MAC 2" },
        { ModSlot::Type::Lfo,    "lfo1",    "LFO 1" },
        { ModSlot::Type::Lfo,    "lfo2",    "LFO 2" },
        { ModSlot::Type::Random, "randomA", "RAND"  },
        // Engine B modulators
        { ModSlot::Type::Macro,  "macro3",  "MAC 3" },
        { ModSlot::Type::Macro,  "macro4",  "MAC 4" },
        { ModSlot::Type::Lfo,    "lfo3",    "LFO 3" },
        { ModSlot::Type::Lfo,    "lfo4",    "LFO 4" },
        { ModSlot::Type::Random, "randomB", "RAND"  },
    };
    for (const auto& def : defs)
    {
        auto* row = new MatrixModRow(processor, def.type, def.modId, def.label);
        addAndMakeVisible(*row);
        modRows.add(row);
    }
}
```

Update `paint()` to remove the placeholder text — the modulator strip on the left now occupies space, and the destination grid placeholder is on the right (Task 5 fills it):

```cpp
void MatrixView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xc0000000));
    const auto bounds = getLocalBounds();
    auto card = bounds.reduced(kCardMargin);

    g.setColour(Theme::matrixBg);
    g.fillRoundedRectangle(card.toFloat(), 6.0f);
    g.setColour(Theme::panelBorder);
    g.drawRoundedRectangle(card.toFloat(), 6.0f, 1.0f);

    auto titleBar = card.removeFromTop(40).reduced(16, 8);
    g.setColour(Theme::textPrimary);
    g.setFont(juce::FontOptions("Space Grotesk", 14.0f, juce::Font::bold));
    g.drawText("Modulation Matrix", titleBar.toFloat(), juce::Justification::centredLeft, false);

    // Right side placeholder until Task 5.
    auto bodyArea = card.withTrimmedLeft(160);
    g.setColour(Theme::textDim);
    g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::plain));
    g.drawText("Destination grid (Task 5)", bodyArea.toFloat(),
               juce::Justification::centred, false);
}
```

Update `resized()` to position the modulator rows in the left strip:

```cpp
void MatrixView::resized()
{
    auto card = getLocalBounds().reduced(kCardMargin);
    card.removeFromTop(40);  // title bar

    // Left strip: 160px wide for modulator rows.
    auto strip = card.removeFromLeft(160).reduced(8);
    if (modRows.isEmpty()) return;
    const int rowH = strip.getHeight() / modRows.size();
    for (auto* row : modRows)
        row->setBounds(strip.removeFromTop(rowH));
}
```

- [ ] **Step 5: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/widgets/MatrixModRow.cpp` after `Source/UI/widgets/ModSlot.cpp`.

- [ ] **Step 6: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Open native editor → click MATRIX → matrix overlay shows. Left strip displays 10 rows: MAC 1, MAC 2, LFO 1, LFO 2, RAND (engine A), then MAC 3, MAC 4, LFO 3, LFO 4, RAND (engine B). Macro rows have full-color dots; LFO/Random rows are greyed.

- [ ] **Step 7: Commit**

```bash
git add Source/UI/widgets/MatrixModRow.h Source/UI/widgets/MatrixModRow.cpp Source/UI/panels/MatrixView.h Source/UI/panels/MatrixView.cpp CMakeLists.txt
git commit -m "feat(path-b): MatrixView modulator strip (10 rows: 4 macros + LFO/Random placeholders)"
```

(Exact commit message — no Claude trailer.)

---

## Task 5: DestinationGrid + MatrixCell (read-only display)

**Files:**
- Create: `Source/UI/widgets/MatrixCell.h`
- Create: `Source/UI/widgets/MatrixCell.cpp`
- Modify: `Source/UI/panels/MatrixView.h`
- Modify: `Source/UI/panels/MatrixView.cpp`
- Modify: `CMakeLists.txt`

Add the destination grid to the right of the modulator strip. Categorized destination columns (8 categories × 31 leaves total, per the matrix.js implementation we already shipped). For each row × column intersection, a `MatrixCell` shows whether a routing exists. Read-only display only — Task 6 adds interactions.

For simplicity in Phase 5: NO category collapse (all categories always visible — scrollable horizontally if too wide). Polish pass can add collapse later. Each cell is a `juce::Component` with paint code that reads the routing depth and draws a bipolar fill.

8 categories from matrix.js DEST_GROUPS (mirror exactly):
- GHOST: ghost, phantom_threshold, phantom_strength, output_gain
- RECIPE: recipe_h2..h8, harmonic_saturation
- SHAPE: synth_step, synth_duty, synth_skip
- ENVELOPE: env_attack_ms, env_release_ms
- FILTER: synth_lpf_hz, synth_hpf_hz
- RESYN: synth_wavelet_length, synth_gate_threshold, synth_h1, synth_sub
- PITCH: synth_min_samples, synth_max_samples, tracking_speed, punch_amount, synth_boost_threshold, synth_boost_amount
- STEREO: binaural_width, stereo_width

Total: 31 destinations per engine. Cell grid: 10 modulator rows × 31 columns (with engine A modulators getting `a_` prefixed param IDs and engine B modulators getting `b_` prefixed) — but actually, the routings are scoped per-engine: macro1 (engine A) only routes to a_* params, macro3 (engine B) only routes to b_*. So we draw cells with:
- Top 5 rows (engine A modulators) × 31 a_* destinations
- Bottom 5 rows (engine B modulators) × 31 b_* destinations

The grid rendering: each row has 31 cells; the cells reflect routings to that engine's prefix.

After Task 5: matrix shows the full grid with read-only cell display reflecting current routings.

- [ ] **Step 1: Create Source/UI/widgets/MatrixCell.h**

```cpp
// Source/UI/widgets/MatrixCell.h
#pragma once
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ModSlot.h"  // for Type enum (color coding)

class PhantomProcessor;

namespace kaigen::phantom
{

/** Single cell in the modulation matrix grid.
 *  Reads routing depth from ModulationEngine via paint-time lookups.
 *  Click → AddRouting at +0.5 (Task 6). Drag → SetRoutingDepth (Task 6).
 *  Right-click → quick-set + remove popover (Task 6). */
class MatrixCell : public juce::Component
{
public:
    MatrixCell(PhantomProcessor& processor,
               const juce::String& sourceId,    // e.g., "macro1"
               const juce::String& paramId,     // e.g., "a_ghost"
               ModSlot::Type type);
    ~MatrixCell() override;

    /** Modulator id — used by Task 7 for live cell pulse. */
    const juce::String& getSourceId() const noexcept { return sourceId; }

    /** Param id — used by Task 7 for cell lookup. */
    const juce::String& getParamId() const noexcept { return paramId; }

    /** Live modulation contribution — used by Task 7 to drive cell glow.
     *  Setting this triggers a repaint if the value changed meaningfully. */
    void setLiveContribution(float value);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    /** Read current routing depth from ModulationEngine. Returns 0 if no
     *  routing exists. */
    float currentDepth() const;

    PhantomProcessor& processor;
    juce::String sourceId;
    juce::String paramId;
    ModSlot::Type type;
    float liveContribution { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixCell)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/MatrixCell.cpp**

```cpp
// Source/UI/widgets/MatrixCell.cpp
#include "MatrixCell.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../Modulation/ModulationEngine.h"
#include "../../Modulation/Routing.h"

namespace kaigen::phantom
{

MatrixCell::MatrixCell(PhantomProcessor& p, const juce::String& src,
                        const juce::String& param, ModSlot::Type t)
    : processor(p), sourceId(src), paramId(param), type(t)
{
}

MatrixCell::~MatrixCell() = default;

void MatrixCell::setLiveContribution(float v)
{
    if (std::abs(v - liveContribution) > 0.01f)
    {
        liveContribution = v;
        repaint();
    }
}

float MatrixCell::currentDepth() const
{
    // Determine engine from param prefix.
    auto& engine = paramId.startsWith("a_")
        ? processor.getModulationEngineA()
        : processor.getModulationEngineB();
    for (const auto& r : engine.getRoutings())
        if (r.sourceId == sourceId && r.paramId == paramId)
            return r.depth;
    return 0.0f;
}

void MatrixCell::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float depth = currentDepth();
    const bool routed = (std::abs(depth) > 0.001f);

    // Background.
    g.setColour(routed ? Theme::matrixBg.brighter(0.10f) : Theme::matrixBg);
    g.fillRect(bounds);

    if (routed)
    {
        // Bipolar fill: positive = right-leaning gradient; negative = left-leaning.
        juce::Colour accent;
        switch (type)
        {
            case ModSlot::Type::Macro:  accent = Theme::macroTeal;    break;
            case ModSlot::Type::Lfo:    accent = Theme::lfoBlue;      break;
            case ModSlot::Type::Random: accent = Theme::randomPurple; break;
            default:                    accent = Theme::macroTeal;    break;
        }
        const float alpha = juce::jmin(1.0f, std::abs(depth));

        if (depth >= 0.0f)
        {
            auto grad = juce::ColourGradient(accent.withAlpha(alpha),
                                              bounds.getX(), bounds.getCentreY(),
                                              accent.withAlpha(0.0f),
                                              bounds.getRight(), bounds.getCentreY(),
                                              false);
            g.setGradientFill(grad);
        }
        else
        {
            auto grad = juce::ColourGradient(accent.withAlpha(0.0f),
                                              bounds.getX(), bounds.getCentreY(),
                                              accent.withAlpha(alpha),
                                              bounds.getRight(), bounds.getCentreY(),
                                              false);
            g.setGradientFill(grad);
        }
        g.fillRect(bounds.reduced(1.0f));

        // Numeric depth label.
        g.setColour(Theme::textPrimary);
        g.setFont(juce::FontOptions("Space Grotesk", 7.0f, juce::Font::bold));
        const int pct = juce::roundToInt(depth * 100.0f);
        const auto txt = (pct > 0 ? juce::String("+") + juce::String(pct) : juce::String(pct));
        g.drawText(txt, bounds, juce::Justification::centred, false);

        // Live pulse glow (Task 7).
        if (liveContribution > 0.05f)
        {
            g.setColour(accent.withAlpha(0.4f * juce::jmin(1.0f, liveContribution)));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 2.0f, 1.5f);
        }
    }

    // Border.
    g.setColour(Theme::panelBorder);
    g.drawRect(bounds, 0.5f);
}

void MatrixCell::resized()
{
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Update MatrixView.h**

Add include + members:

```cpp
#include "../widgets/MatrixCell.h"
```

In private section:

```cpp
juce::OwnedArray<MatrixCell> cells;

struct CategoryGroup { juce::String label; std::vector<std::pair<juce::String, juce::String>> leaves; };
std::vector<CategoryGroup> categories;
```

(`leaves` is a vector of (paramLeaf, columnAbbrev) pairs, e.g., `{{"ghost", "GHOST"}, {"phantom_threshold", "PHTHR"}, ...}`.)

- [ ] **Step 4: Update MatrixView.cpp**

In an anonymous namespace at the top of the .cpp, define the categories:

```cpp
namespace
{
    constexpr int kStripWidthPx = 160;
    constexpr int kCellHeightPx = 24;

    std::vector<MatrixView::CategoryGroup> buildCategories()
    {
        return {
            { "GHOST",    {{"ghost","GHOST"},{"phantom_threshold","PHTHR"},{"phantom_strength","PSTR"},{"output_gain","OUT"}} },
            { "RECIPE",   {{"recipe_h2","H2"},{"recipe_h3","H3"},{"recipe_h4","H4"},{"recipe_h5","H5"},{"recipe_h6","H6"},{"recipe_h7","H7"},{"recipe_h8","H8"},{"harmonic_saturation","HSAT"}} },
            { "SHAPE",    {{"synth_step","STEP"},{"synth_duty","DUTY"},{"synth_skip","SKIP"}} },
            { "ENV",      {{"env_attack_ms","ATK"},{"env_release_ms","REL"}} },
            { "FILTER",   {{"synth_lpf_hz","LPF"},{"synth_hpf_hz","HPF"}} },
            { "RESYN",    {{"synth_wavelet_length","WVLEN"},{"synth_gate_threshold","GATE"},{"synth_h1","H1"},{"synth_sub","SUB"}} },
            { "PITCH",    {{"synth_min_samples","MINSP"},{"synth_max_samples","MAXSP"},{"tracking_speed","TRACK"},{"punch_amount","PUNCH"},{"synth_boost_threshold","BTHR"},{"synth_boost_amount","BAMT"}} },
            { "STEREO",   {{"binaural_width","BIN"},{"stereo_width","WIDTH"}} },
        };
    }
}
```

(The `MatrixView::CategoryGroup` struct should be moved out of the class header into this anonymous namespace, OR remain in the header — your choice. The simpler path is to keep it nested in MatrixView and use `MatrixView::CategoryGroup` here.)

In the constructor, after creating modRows, build the cell grid:

```cpp
categories = buildCategories();

// Build cells: 10 modulator rows × N destination columns.
struct ModInfo { const char* sourceId; ModSlot::Type type; bool isEngineA; };
const ModInfo mods[] = {
    {"macro1",  ModSlot::Type::Macro,  true},
    {"macro2",  ModSlot::Type::Macro,  true},
    {"lfo1",    ModSlot::Type::Lfo,    true},
    {"lfo2",    ModSlot::Type::Lfo,    true},
    {"randomA", ModSlot::Type::Random, true},
    {"macro3",  ModSlot::Type::Macro,  false},
    {"macro4",  ModSlot::Type::Macro,  false},
    {"lfo3",    ModSlot::Type::Lfo,    false},
    {"lfo4",    ModSlot::Type::Lfo,    false},
    {"randomB", ModSlot::Type::Random, false},
};

for (const auto& mod : mods)
{
    const juce::String prefix = mod.isEngineA ? "a_" : "b_";
    for (const auto& cat : categories)
        for (const auto& [leaf, abbrev] : cat.leaves)
        {
            const juce::String paramId = prefix + leaf;
            auto* cell = new MatrixCell(processor, mod.sourceId, paramId, mod.type);
            addAndMakeVisible(*cell);
            cells.add(cell);
        }
}
```

Update `resized()` to lay out the cells. The grid: 10 rows top-to-bottom, each row has N cells (N = total destination columns).

```cpp
void MatrixView::resized()
{
    auto card = getLocalBounds().reduced(kCardMargin);
    card.removeFromTop(40);  // title bar

    // Left strip: 160px wide for modulator rows.
    auto strip = card.removeFromLeft(kStripWidthPx).reduced(8);
    if (modRows.isEmpty()) return;
    const int rowH = strip.getHeight() / modRows.size();
    for (auto* row : modRows)
        row->setBounds(strip.removeFromTop(rowH));

    // Right area: cell grid.
    auto gridArea = card.reduced(8);
    int totalCols = 0;
    for (const auto& cat : categories) totalCols += (int) cat.leaves.size();
    if (totalCols == 0 || cells.isEmpty()) return;

    const int cellW = gridArea.getWidth() / totalCols;
    const int cellH = gridArea.getHeight() / 10;  // 10 modulator rows
    int cellIdx = 0;
    for (int row = 0; row < 10; ++row)
    {
        const int y = gridArea.getY() + row * cellH;
        int col = 0;
        for (const auto& cat : categories)
            for (size_t i = 0; i < cat.leaves.size(); ++i)
            {
                if (cellIdx >= cells.size()) break;
                const int x = gridArea.getX() + col * cellW;
                cells[cellIdx]->setBounds(x, y, cellW, cellH);
                ++cellIdx;
                ++col;
            }
    }
}
```

Update `paint()` to remove the "Destination grid (Task 5)" placeholder text and instead draw category headers above the cell grid:

```cpp
void MatrixView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xc0000000));
    const auto bounds = getLocalBounds();
    auto card = bounds.reduced(kCardMargin);

    g.setColour(Theme::matrixBg);
    g.fillRoundedRectangle(card.toFloat(), 6.0f);
    g.setColour(Theme::panelBorder);
    g.drawRoundedRectangle(card.toFloat(), 6.0f, 1.0f);

    auto titleBar = card.removeFromTop(40).reduced(16, 8);
    g.setColour(Theme::textPrimary);
    g.setFont(juce::FontOptions("Space Grotesk", 14.0f, juce::Font::bold));
    g.drawText("Modulation Matrix", titleBar.toFloat(), juce::Justification::centredLeft, false);

    // Category headers above the grid.
    auto stripAndGrid = card.reduced(8);
    stripAndGrid.removeFromLeft(kStripWidthPx);
    if (categories.empty()) return;

    int totalCols = 0;
    for (const auto& cat : categories) totalCols += (int) cat.leaves.size();
    if (totalCols == 0) return;
    const int cellW = stripAndGrid.getWidth() / totalCols;

    g.setColour(Theme::textSecondary);
    g.setFont(juce::FontOptions("Space Grotesk", 8.0f, juce::Font::bold));
    int col = 0;
    for (const auto& cat : categories)
    {
        const int span = (int) cat.leaves.size();
        const int x = stripAndGrid.getX() + col * cellW;
        const int w = span * cellW;
        const auto headerBounds = juce::Rectangle<int>(x, stripAndGrid.getY() - 14, w, 12);
        g.drawText(cat.label, headerBounds.toFloat(), juce::Justification::centred, false);
        col += span;
    }
}
```

(Note: the headers are above the grid area which means they technically poke into the title bar's row. For Phase 5 simplicity, this is OK — polish pass can fix vertical layout. Adjust the `card.removeFromTop(40)` to `card.removeFromTop(40 + 14)` if you want explicit space for the headers.)

- [ ] **Step 5: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/widgets/MatrixCell.cpp` after `Source/UI/widgets/MatrixModRow.cpp`.

- [ ] **Step 6: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Open native editor → MATRIX → matrix shows: left strip with 10 modulator rows, right side with category headers (GHOST, RECIPE, SHAPE, ENV, FILTER, RESYN, PITCH, STEREO) and a 10-row × 31-column grid of empty cells. (Cells are empty until Task 6 lets the user add routings.)

- [ ] **Step 7: Commit**

```bash
git add Source/UI/widgets/MatrixCell.h Source/UI/widgets/MatrixCell.cpp Source/UI/panels/MatrixView.h Source/UI/panels/MatrixView.cpp CMakeLists.txt
git commit -m "feat(path-b): MatrixView destination grid (read-only cell display)"
```

(Exact commit message — no Claude trailer.)

---

## Task 6: Cell interactions — click, drag, right-click popover

**Files:**
- Modify: `Source/UI/widgets/MatrixCell.h`
- Modify: `Source/UI/widgets/MatrixCell.cpp`

Add mouse interactions to MatrixCell:
- **Click empty cell** → `engine.addRouting({sourceId, paramId, +0.5, false})`. Creates a routing at +50% depth.
- **Drag active cell vertically** → `engine.setRoutingDepth(...)` based on Y delta. -100..+100 maps to ~100 px of vertical drag.
- **Right-click any cell** → small popover with "Set −100 / 0 / +50 / +100" + "Remove" buttons. Click an option → calls the appropriate engine method.

After Task 6: full routing CRUD via cell interactions.

- [ ] **Step 1: Update MatrixCell.h**

Add mouse-handler overrides + a popover state:

```cpp
public:
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    /** Show right-click popover with quick-set + remove options. */
    void showPopover(juce::Point<int> mousePosScreen);

    float dragStartDepth { 0.0f };
    int   dragStartY     { 0 };
```

- [ ] **Step 2: Update MatrixCell.cpp**

Add mouse handlers:

```cpp
void MatrixCell::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showPopover(e.getScreenPosition());
        return;
    }

    const float existing = currentDepth();
    auto& engine = paramId.startsWith("a_")
        ? processor.getModulationEngineA()
        : processor.getModulationEngineB();

    if (std::abs(existing) < 0.001f)
    {
        // Empty cell — add routing at +0.5.
        engine.addRouting({sourceId, paramId, 0.5f, false});
        repaint();
    }
    else
    {
        // Existing cell — start drag.
        dragStartDepth = existing;
        dragStartY = e.y;
    }
}

void MatrixCell::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;

    const float existing = currentDepth();
    if (std::abs(existing) < 0.001f) return;  // not actively routed

    // Vertical drag: 100 px = full -1..+1 range.
    const int dy = dragStartY - e.y;  // up = positive
    const float newDepth = juce::jlimit(-1.0f, 1.0f, dragStartDepth + (float) dy / 100.0f);

    auto& engine = paramId.startsWith("a_")
        ? processor.getModulationEngineA()
        : processor.getModulationEngineB();
    engine.setRoutingDepth(sourceId, paramId, newDepth);
    repaint();
}

void MatrixCell::showPopover(juce::Point<int>)
{
    juce::PopupMenu menu;

    auto& engine = paramId.startsWith("a_")
        ? processor.getModulationEngineA()
        : processor.getModulationEngineB();
    const float existing = currentDepth();
    const bool routed = (std::abs(existing) > 0.001f);

    if (routed)
    {
        menu.addItem(1, "Set -100%");
        menu.addItem(2, "Set 0%");
        menu.addItem(3, "Set +50%");
        menu.addItem(4, "Set +100%");
        menu.addSeparator();
        menu.addItem(5, "Remove");
    }
    else
    {
        menu.addItem(10, "Add at +50%");
        menu.addItem(11, "Add at -50%");
    }

    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this),
        [this, &engine](int result) {
            switch (result)
            {
                case 1:  engine.setRoutingDepth(sourceId, paramId, -1.0f); break;
                case 2:  engine.setRoutingDepth(sourceId, paramId, 0.0f);  break;
                case 3:  engine.setRoutingDepth(sourceId, paramId, 0.5f);  break;
                case 4:  engine.setRoutingDepth(sourceId, paramId, 1.0f);  break;
                case 5:  engine.removeRouting(sourceId, paramId);          break;
                case 10: engine.addRouting({sourceId, paramId, 0.5f, false});  break;
                case 11: engine.addRouting({sourceId, paramId, -0.5f, false}); break;
                default: break;
            }
            repaint();
        });
}
```

(`PopupMenu` is JUCE's stock async menu — drives a small native menu. Simpler than a custom dialog component.)

- [ ] **Step 3: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: builds clean. Open MATRIX → click an empty cell → it lights up at +50% (you'll see a teal/blue gradient + "+50" label). Drag the cell vertically to adjust. Right-click → popover with quickset + remove options.

- [ ] **Step 4: Commit**

```bash
git add Source/UI/widgets/MatrixCell.h Source/UI/widgets/MatrixCell.cpp
git commit -m "feat(path-b): MatrixCell interactions (click/drag/right-click popover)"
```

(Exact commit message — no Claude trailer.)

---

## Task 7: Polish — name editor, live pulse, slot handoff, persistence

**Files:**
- Modify: `Source/UI/widgets/MatrixModRow.h`
- Modify: `Source/UI/widgets/MatrixModRow.cpp`
- Modify: `Source/UI/panels/MatrixView.h`
- Modify: `Source/UI/panels/MatrixView.cpp`
- Modify: `Source/UI/panels/ModulationPanel.h`
- Modify: `Source/UI/panels/ModulationPanel.cpp`
- Modify: `Source/UI/NativePluginEditor.cpp`

Four polish items in one task:

1. **Inline macro name editor**: clicking a macro row's name in MatrixModRow opens a small `juce::TextEditor` overlay that, on Enter/blur, calls `processor.getModulationEngineX().findModulator("macroN")->setName(...)`.

2. **Live cell pulse**: 10 Hz Timer in MatrixView reads each macro's current value via `findModulator(modId)->getCurrentValue()`, computes contribution = `value * depth` per active cell, calls `cell.setLiveContribution(...)`.

3. **Slot-click → matrix handoff**: clicking a macro slot in SLOTS mode calls `onMatrixToggle(true)` and tells the matrix to highlight that mod row briefly.

4. **MatrixViewState persistence**: on construction, read `processor.getMatrixView()` to set the initial matrix mode + expanded categories. On any mode change, save back. (For Phase 5 we only persist mode; expandedA/B is unused since we don't have category collapse.)

After Task 7: all matrix interactions polished; matrix state persists across sessions.

- [ ] **Step 1: Inline name editor in MatrixModRow**

Update MatrixModRow.h:

```cpp
public:
    void mouseDown(const juce::MouseEvent& e) override;

private:
    std::unique_ptr<juce::TextEditor> nameEditor;

    void commitNameEdit();
    void cancelNameEdit();
```

Update MatrixModRow.cpp:

```cpp
void MatrixModRow::mouseDown(const juce::MouseEvent& e)
{
    if (type != ModSlot::Type::Macro) return;  // only macros are editable
    if (nameEditor != nullptr) return;          // already editing

    nameEditor = std::make_unique<juce::TextEditor>();
    nameEditor->setText(label);
    nameEditor->setBounds(getLocalBounds().withTrimmedLeft(20).withWidth(80));
    nameEditor->onReturnKey = [this] { commitNameEdit(); };
    nameEditor->onEscapeKey = [this] { cancelNameEdit(); };
    nameEditor->onFocusLost = [this] { commitNameEdit(); };
    addAndMakeVisible(*nameEditor);
    nameEditor->grabKeyboardFocus();
    nameEditor->selectAll();
}

void MatrixModRow::commitNameEdit()
{
    if (nameEditor == nullptr) return;
    const auto newName = nameEditor->getText().trim();
    if (newName.isNotEmpty())
    {
        // Update via ModulationEngine. macros 1-2 are engine A; 3-4 are engine B.
        auto& engine = (modId == "macro1" || modId == "macro2")
            ? processor.getModulationEngineA()
            : processor.getModulationEngineB();
        if (auto* mod = engine.findModulator(modId))
            mod->setName(newName);
        label = newName;
    }
    nameEditor.reset();
    repaint();
}

void MatrixModRow::cancelNameEdit()
{
    nameEditor.reset();
}
```

- [ ] **Step 2: Live pulse via Timer in MatrixView**

Update MatrixView.h:

```cpp
private juce::Timer
{
    void timerCallback() override;
};
```

Wait, that's wrong syntax. Update MatrixView.h to add `private juce::Timer` as a base:

```cpp
class MatrixView : public juce::Component, private juce::Timer
{
    // ... existing members ...

private:
    void timerCallback() override;
};
```

Update MatrixView.cpp's constructor to start the timer:

```cpp
// At end of constructor, after building cells.
startTimerHz(10);
```

In destructor, stop the timer:

```cpp
MatrixView::~MatrixView()
{
    stopTimer();
}
```

Implement timerCallback:

```cpp
void MatrixView::timerCallback()
{
    if (! isVisible()) return;

    // For each cell, compute contribution = modValue * depth and update.
    for (auto* cell : cells)
    {
        const auto sourceId = cell->getSourceId();
        const auto paramId = cell->getParamId();

        auto& engine = paramId.startsWith("a_")
            ? processor.getModulationEngineA()
            : processor.getModulationEngineB();

        float modValue = 0.0f;
        if (auto* mod = engine.findModulator(sourceId))
            modValue = mod->getCurrentValue();

        float depth = 0.0f;
        for (const auto& r : engine.getRoutings())
            if (r.sourceId == sourceId && r.paramId == paramId)
            {
                depth = r.depth;
                break;
            }

        cell->setLiveContribution(modValue * std::abs(depth));
    }

    // Update modRow value readouts (only for macros).
    for (auto* row : modRows)
    {
        const auto modId = row->getModId();
        if (! modId.startsWith("macro")) continue;
        auto& engine = (modId == "macro1" || modId == "macro2")
            ? processor.getModulationEngineA()
            : processor.getModulationEngineB();
        if (auto* mod = engine.findModulator(modId))
            row->setValueText(juce::String(mod->getCurrentValue(), 2));
    }
}
```

- [ ] **Step 3: Slot-click handoff in NativePluginEditor**

Update NativePluginEditor.cpp's constructor (after `addAndMakeVisible(modulationPanel)` and matrixView wiring):

```cpp
// Slot-click → matrix mode + row highlight.
for (auto* slot : modulationPanel.getSlots())
{
    slot->onSlotClicked = [this](juce::String slotId) {
        // Only handle macro slots for now.
        if (! slotId.startsWith("macro")) return;
        // Activate matrix mode.
        if (! matrixView.isVisible())
        {
            matrixView.setVisible(true);
            matrixView.toFront(false);
            // Update mode bar to reflect — this is a small abstraction leak;
            // a polish pass could expose modulationPanel.activateMatrix() to
            // keep the mode-bar buttons in sync.
        }
        // Highlight the row briefly (left as a polish enhancement; no-op for now).
    };
}
```

(Highlighting the matched row briefly with a flash animation is deferred to polish.)

- [ ] **Step 4: MatrixViewState persistence**

In NativePluginEditor.cpp constructor (after the matrixView is constructed and wired):

```cpp
// Restore persisted matrix mode.
const auto persisted = processor.getMatrixView();
if (persisted.mode == kaigen::phantom::MatrixMode::Matrix)
{
    matrixView.setVisible(true);
    matrixView.toFront(false);
}

// Save mode on toggle changes.
modulationPanel.onMatrixToggle = [this](bool active) {
    matrixView.setVisible(active);
    if (active) matrixView.toFront(false);
    auto state = processor.getMatrixView();
    state.mode = active ? kaigen::phantom::MatrixMode::Matrix : kaigen::phantom::MatrixMode::Slots;
    processor.setMatrixView(state);
    processor.updateHostDisplay();
};
```

(This replaces the simpler `onMatrixToggle` lambda from Task 3.)

- [ ] **Step 5: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: builds clean. Verify:
- Click MAC 1 row in matrix → text input → type "BASS DRIVE" → press Enter → label updates
- Move macro 1 knob in SLOTS → matrix cells routed from macro 1 show the live-pulse glow
- Click MAC 1 slot in SLOTS view → matrix opens
- Save the project, close, reopen → matrix mode is preserved

- [ ] **Step 6: Commit**

```bash
git add Source/UI/widgets/MatrixModRow.h Source/UI/widgets/MatrixModRow.cpp Source/UI/panels/MatrixView.h Source/UI/panels/MatrixView.cpp Source/UI/panels/ModulationPanel.h Source/UI/panels/ModulationPanel.cpp Source/UI/NativePluginEditor.cpp
git commit -m "feat(path-b): matrix polish (name editor, live pulse, slot handoff, persistence)"
```

(Exact commit message — no Claude trailer.)

---

## Task 8: Manual smoke + final review

**Files:** none modified — verification only.

End-to-end smoke for Phase 5.

- [ ] **Step 1: Build VST3 + Standalone**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_VST3
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expect: both build clean. VST3 auto-installs.

- [ ] **Step 2: Toggle to native UI in Live**

Open WebView2 editor in Live. Shift+click PHANTOM logo. Reopen plugin window.

- [ ] **Step 3: Verify ModulationPanel + slot row**

- Bottom panel shows mode bar (SLOTS active by default, MATRIX inactive) + counter "0 ROUTINGS"
- 11 slots in a row: LFO 1, LFO 2, RAND, MAC 1-2, MORPH, MAC 3-4, RAND, LFO 3, LFO 4
- Macro slots (MAC 1-4) and morph slot have working knobs that drag to change their values
- LFO/Random slots are greyed with "PR4"/"PR5" placeholder text

- [ ] **Step 4: Verify MatrixView**

- Click MATRIX → overlay appears with backdrop scrim + centered card
- Card shows "Modulation Matrix" title, 10 modulator rows on left (MAC 1-2, LFO 1-2, RAND, MAC 3-4, LFO 3-4, RAND), 8 category headers across top (GHOST, RECIPE, SHAPE, ENV, FILTER, RESYN, PITCH, STEREO), and a 10×~30 grid of empty cells

- [ ] **Step 5: Verify cell interactions**

- Click an empty cell at MAC 1 × GHOST → cell lights up with gradient fill + "+50" label
- Drag the cell up → value goes up to "+100"; drag down → value goes negative
- Right-click an active cell → popover with quickset + remove
- Click "Remove" → cell becomes empty
- Counter updates to reflect routing count

- [ ] **Step 6: Verify name editor + live pulse**

- Click MAC 1 row in matrix → text input, type new name, Enter → label updates
- Add a routing MAC 1 × GHOST. In another window, drag the MAC 1 slot's knob → cell shows live pulse glow

- [ ] **Step 7: Verify slot handoff + persistence**

- Click MAC 2 slot in SLOTS view → matrix opens
- Save Live project. Close Live. Reopen project → matrix mode is restored if it was active

- [ ] **Step 8: No commit needed if smoke passes**

Phase 5 done. Phase 6 (cutover — switch default to native, delete WebView2 code) is the final phase.

---

## Self-review notes

### Spec coverage

Phase 5 covers per the spec's "Phase 5 — Modulation system" line item:
- ✓ ModulationPanel with always-visible mode bar — Task 1
- ✓ Slot row (collapsible deferred — see scope cuts below) — Task 2
- ✓ MatrixView with click empty → AddRouting — Task 6
- ✓ Drag → SetRoutingDepth — Task 6
- ✓ Right-click popover — Task 6
- ✓ Inline macro name editor — Task 7
- ✓ Live cell pulse — Task 7
- ✓ Slot-click handoff — Task 7

Scope cuts:
- **Slot row collapse**: spec mentions "always-visible mode bar + collapsible slot row." Phase 5 ships always-visible slot row. Collapse animation deferred.
- **Mode bar slide/fade animation**: instant toggle in Phase 5.
- **Matrix view slide/fade animation**: instant show/hide in Phase 5.
- **Engine focus integration**: matrix shows all 10 modulators always (5 engine A + 5 engine B). The existing engine-focus tab from earlier WebView2 work is not wired to filter matrix view — for Phase 5 this is fine.
- **Category collapse/expand**: matrix.js had this; Phase 5 has all categories always visible (scrollable horizontally if too wide).
- **Row highlight on slot-click handoff**: brief flash deferred to polish.

### Placeholder scan

No "TBD"/"TODO"/vague language. Each step has runnable code or commands.

### Type / signature consistency

- `ModSlot::Type` enum used consistently across ModSlot, MatrixModRow, MatrixCell.
- `ModulationEngine::addRouting`, `removeRouting`, `setRoutingDepth`, `findModulator`, `getRoutings` — all match `Source/Modulation/ModulationEngine.h`.
- `Routing` struct fields `sourceId`, `paramId`, `depth`, `polarityInverted` — verified by Phase 0 work.
- `processor.getModulationEngineA/B()`, `getMatrixView/setMatrixView()` — verified by `Source/PluginProcessor.h`.
- `MatrixViewState::Mode::Matrix/Slots` enum used in Task 7 persistence.
- `ModSlot::onSlotClicked` callback signature `void(juce::String)` consistent across Tasks 2 and 7.
- `ModulationPanel::onMatrixToggle` callback `void(bool)` consistent across Tasks 1 and 3.

### What ships in Phase 5

After this plan completes:
- Native editor's bottom modulation panel works: SLOTS/MATRIX toggle + 11-slot row with macro knobs
- Matrix overlay with full grid: 10 modulator rows × ~30 destination cells, color-coded by modulator type
- All routing CRUD interactions: click-to-add, drag-to-adjust, right-click popover, name editor
- Live cell pulse driven by 10 Hz Timer reading macro values
- Slot-click handoff to matrix mode
- Matrix mode persists across sessions

After Phase 5, only Phase 6 remains (cutover): flip default to native, delete WebView2 code, ship.
