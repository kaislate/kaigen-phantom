# Phantom A/B Compare — Design Specification

**Date:** 2026-04-22
**Status:** Design Phase
**Scope:** Two-slot A/B snap comparison system for Kaigen Phantom (Standard build), with architectural seam for a future Pro Morph Module

---

## Overview

Phantom gains a standard A/B comparison feature: two independent state slots (A and B) that users can snap between, edit independently, and save as designer-authored A/B presets. The feature ships in the **Standard build** as a binary snap toggle with no continuous interpolation.

A future **Pro Morph Module** (separate spec) will layer continuous parameter morphing and optional dual-engine DSP crossfading on top of the same slot storage. This spec defines the Standard build and the architectural seam the Pro module will attach to; it does **not** design the morph engine itself.

### Product Tiering

| Capability                              | Standard | Pro |
|-----------------------------------------|:--------:|:---:|
| Two slots with A/B snap                 | ✓        | ✓   |
| Copy A↔B                                | ✓        | ✓   |
| Per-slot modified indicator             | ✓        | ✓   |
| Save/load Single presets                | ✓        | ✓   |
| Save/load A/B presets                   | ✓        | ✓   |
| Load A/B+Morph presets (as A/B)         | ✓        | ✓   |
| "Include discrete params" setting       | ✓        | ✓   |
| Continuous morph knob (0 → 1)           | —        | ✓   |
| Save/load A/B+Morph presets (w/ morph)  | —        | ✓   |
| Dual-engine crossfade for discrete      | —        | ✓   |

Pro features are gated by a compile-time flag (`KAIGEN_PRO_BUILD`). Standard and Pro ship as separate binaries. No license key / runtime unlock in scope for this spec.

---

## Architecture

### Class Layout

```
PhantomProcessor
├── apvts (AudioProcessorValueTreeState)   // live state
├── presetManager (PresetManager)           // existing
└── abSlots (ABSlotManager)                 // NEW
    ├── slots[2] (ValueTree)                // slot A + slot B snapshots
    ├── active (Slot enum)                  // A or B
    ├── designerAuthored (bool)             // set when an A/B preset is loaded
    └── includeDiscreteInSnap (bool)        // user setting (per-project)
```

### New Class: `ABSlotManager`

Owned by `PhantomProcessor` (not `PhantomEditor`) so lifetime matches the plugin, not the UI. Follows the same ownership pattern established by `PresetManager`.

```cpp
// Source/ABSlotManager.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace kaigen::phantom
{

class ABSlotManager
{
public:
    enum class Slot { A = 0, B = 1 };

    explicit ABSlotManager(juce::AudioProcessorValueTreeState& apvts);

    // Core operations
    void snapTo(Slot target);             // commit live → active, replace live ← target
    void copy(Slot from, Slot to);        // full-tree copy; clears destination modified flag
    Slot getActive() const noexcept { return active; }

    // Modified-indicator state (per slot)
    bool isModified(Slot s) const noexcept;
    juce::String getPresetRef(Slot s) const;  // "pack/name" of last preset loaded into slot, or empty

    // Plugin state persistence (getStateInformation / setStateInformation)
    void syncActiveSlotFromLive();        // commits live APVTS → slots[active] — call before serializing
    juce::ValueTree toStateTree() const;
    void fromStateTree(const juce::ValueTree& abSlotsTree);

    // Designer-authored A/B preset application
    void loadSinglePresetIntoActive(const juce::ValueTree& presetState,
                                    const juce::String& presetRef);
    void loadABPreset(const juce::ValueTree& presetRootState,
                      const juce::String& presetRef);

    // Called by PresetManager::savePreset when building the tree to write
    juce::ValueTree buildPresetRootTree(Slot sourceSlot) const;   // for Single
    juce::ValueTree buildPresetSlotBChild() const;                 // for A/B: the <SlotB> child

    // Setting accessors
    bool getIncludeDiscreteInSnap() const noexcept { return includeDiscreteInSnap; }
    void setIncludeDiscreteInSnap(bool on) noexcept { includeDiscreteInSnap = on; }

    // Read-only slot access — this is the seam the Pro MorphEngine consumes.
    const juce::ValueTree& getSlot(Slot s) const noexcept { return slots[(int)s]; }

private:
    void commitLiveToSlot(Slot s);          // apvts.copyState() → slots[s]
    void applySlotToLive(Slot s);           // apvts.replaceState(slots[s])
    void preserveDiscreteAcrossSnap();      // re-poke the 4 discrete params post-replace
    void clearModifiedFlag(Slot s);
    void markModifiedIfLive();              // APVTS listener bumps the active slot's dirty bit

    juce::AudioProcessorValueTreeState& apvts;
    juce::ValueTree slots[2];
    Slot active = Slot::A;
    bool designerAuthored = false;
    bool includeDiscreteInSnap = false;

    bool modified[2] { false, false };
    juce::String presetRef[2];

    // Param IDs whose values are preserved across ad-hoc snaps when
    // includeDiscreteInSnap == false AND !designerAuthored.
    // Derived from Parameters.h: Mode, Bypass, Ghost Mode, Binaural Mode.
    static const juce::StringArray& kDiscreteParamIDs();
};

} // namespace kaigen::phantom
```

### Pro Seam

The Pro `MorphEngine` (future spec) reads from `ABSlotManager::getSlot(A)` and `::getSlot(B)` to compute per-block interpolated parameter values. Its instantiation in `PhantomProcessor` is wrapped in:

```cpp
#ifdef KAIGEN_PRO_BUILD
    MorphEngine morph;   // reads abSlots, drives apvts under morph control
#endif
```

Standard builds do not compile `MorphEngine.cpp` into the target and do not reference it from `PhantomProcessor`. The seam is the **read-only slot accessor** on `ABSlotManager`.

---

## Data Model

### Slot Storage (in-memory)

Each slot is a `juce::ValueTree` — the same structure as `apvts.copyState()`. ValueTrees are value-typed with copy-on-write children, so two full snapshots are cheap and correct. No manual parameter-by-parameter copying needed.

### Discrete Parameter Exclusion List

When `includeDiscreteInSnap == false` AND `designerAuthored == false`, a snap preserves the following parameters' live values instead of loading the target slot's values:

- `MODE` (Effect / Resyn)
- `BYPASS`
- `GHOST_MODE` (Replace / Combine / Phantom Only)
- `BINAURAL_MODE`

Implementation: after `apvts.replaceState(target)`, read these four IDs from the pre-snap state and write them back via parameter `beginChangeGesture` / `setValueNotifyingHost` / `endChangeGesture`.

### Plugin State Persistence

Existing `PhantomProcessor::getStateInformation` already serializes `apvts.copyState()` to XML. The updated flow:

1. `abSlots.syncActiveSlotFromLive()` — captures any in-flight edits so `slots[active]` matches live before serializing. Without this, a user who tweaked a knob and immediately saved the project would find their tweak on reload as live state but *not* in `slots[active]`, so snapping to the active slot would silently revert it.
2. Build root state from `apvts.copyState()`.
3. Append `<ABSlots>` child built from `abSlots.toStateTree()`.
4. Serialize to XML / MemoryBlock.

It gains an additional `<ABSlots>` child appended before XML conversion:

```xml
<KaigenPhantomState>
  <PARAM id="..." value="..." />
  ...
  <Metadata> ... </Metadata>    <!-- existing -->
  <ABSlots active="A" includeDiscrete="0">
    <Slot name="A">
      <KaigenPhantomState> ... full APVTS state tree ... </KaigenPhantomState>
    </Slot>
    <Slot name="B">
      <KaigenPhantomState> ... full APVTS state tree ... </KaigenPhantomState>
    </Slot>
  </ABSlots>
</KaigenPhantomState>
```

Attributes:
- `active` = `"A"` or `"B"`
- `includeDiscrete` = `"0"` or `"1"` (per-project persistence of the user setting)

**Backwards compatibility:** Plugin-state blobs written before this feature have no `<ABSlots>` child. In that case `ABSlotManager::fromStateTree` initializes both slots from the restored live APVTS state (effectively `A == B == current`), sets `active = A`, and `includeDiscreteInSnap = false`. Users see a clean baseline on first load of old projects.

### Preset File Format Extension

The existing preset `.fxp` format is a `<KaigenPhantomState>` ValueTree (APVTS state) with a `<Metadata>` child. Two optional additional children define the preset kind:

```xml
<KaigenPhantomState>
  <PARAM id="..." value="..." />   <!-- Slot A / primary state -->
  ...
  <Metadata>
    <PROP name="name" value="Bright Vs Dark" />
    <PROP name="type" value="Synth" />
    <PROP name="designer" value="Kai Slate" />
    <PROP name="description" value="..." />
    <PROP name="presetKind" value="ab" />    <!-- NEW: "single" | "ab" | "ab_morph" -->
  </Metadata>
  <SlotB>                                       <!-- NEW: present only for ab / ab_morph -->
    <KaigenPhantomState>
      <PARAM id="..." value="..." />
      ...
    </KaigenPhantomState>
  </SlotB>
  <MorphConfig defaultPosition="0.5" curve="linear" />   <!-- NEW: present only for ab_morph -->
</KaigenPhantomState>
```

**Standard build behavior:**
- Writes `<MorphConfig>` and `presetKind="ab_morph"`: never (Pro-only save path).
- Reads `<MorphConfig>` and `presetKind="ab_morph"`: ignored. Treated as plain A/B preset.

**Pro build behavior:**
- Writes `<MorphConfig>` when user selects "A/B + Morph" in save modal.
- Reads `<MorphConfig>` on preset load to restore morph position.

**Backwards compatibility:** Existing single-state presets have no `<SlotB>` and no `presetKind` metadata. They are treated as `presetKind="single"`.

### Per-Preset Preview Data (browser thumbnails)

The existing `PreviewData` extraction used for browser spectrum thumbnails continues to operate on the root state tree (slot A data for A/B presets). No change required for the thumbnail — it represents the preset's "A face."

---

## Behavior

### Snap (A ↔ B)

Trigger: user clicks the A or B pill button in the header.

1. If target slot == active slot, return (no-op).
2. `slots[active] = apvts.copyState()` — capture any in-flight edits.
3. Save live values of the four discrete param IDs into locals.
4. `apvts.replaceState(slots[target])` — atomic parameter swap.
5. If `!designerAuthored && !includeDiscreteInSnap`: write the saved discrete values back via parameter gesture API.
6. `active = target`.
7. Fire UI update (refresh button lit state and modified indicators).

No automation gestures are recorded for the individual parameter changes — the snap is a user-initiated action, not automation.

### Copy (Active → Other)

Trigger: user clicks the copy button (dynamic label: `A→B` when active=A, `B→A` when active=B).

1. If `slots[A]` already equals `slots[B]` (deep-compare ValueTrees), return (no-op).
2. `slots[active] = apvts.copyState()` — capture in-flight edits on source.
3. `slots[other] = slots[active].createCopy()`.
4. Clear destination modified flag and preset reference.
5. Fire UI update.

Active slot remains unchanged.

### Preset Load

Determined by `presetKind` metadata:

**Single preset** (`presetKind == "single"` or absent, and no `<SlotB>`):
1. `slots[active] = presetRootState.createCopy()`.
2. `apvts.replaceState(slots[active])`.
3. Clear active slot's modified flag; set `presetRef[active] = "pack/name"`.
4. Inactive slot: untouched.
5. `designerAuthored = false`.

**A/B preset** (`presetKind == "ab"` or has `<SlotB>` child):
1. `slots[A] = presetRootState.createCopy()` (with `<SlotB>` and `<MorphConfig>` children stripped so the stored slot is pure APVTS state).
2. `slots[B] = presetRootState.getChildWithName("SlotB").getChild(0).createCopy()`.
3. `apvts.replaceState(slots[active])` (active stays whatever it was).
4. Clear both slots' modified flags; set both `presetRef[]` to `"pack/name"`.
5. `designerAuthored = true` — snaps will now flip discrete params regardless of the user setting.

**A/B + Morph preset** (`presetKind == "ab_morph"` and has `<MorphConfig>`):
1. Same as A/B preset above.
2. **Pro build only:** additionally read `<MorphConfig defaultPosition=...>` and set morph engine position.
3. **Standard build:** `<MorphConfig>` is ignored; behaves identically to A/B preset.

### Preset Save

The save modal gains a **Preset Kind** radio group below the Type dropdown. Options and their availability:

| Kind             | Standard | Pro  | Disabled when                       |
|------------------|:--------:|:----:|-------------------------------------|
| Single (default) | ✓        | ✓    | —                                   |
| A/B              | ✓        | ✓    | slot A and slot B are identical     |
| A/B + Morph      | hidden   | ✓    | slot A and slot B are identical     |

When disabled due to identical slots, inline helper text appears below the radio row:
> *Slot B is unchanged from Slot A. Snap to B and make edits first, or save as Single.*

Save action (`PresetManager::savePreset` gains a `kind` parameter):

**Single:**
- Write `apvts.copyState()` (current active slot state) as root state + `<Metadata>` with `presetKind="single"`.
- Current behavior unchanged apart from the added metadata prop.

**A/B:**
- Write user's slot A as the root state (regardless of active slot at save time).
- Append `<SlotB>` containing user's slot B state.
- `<Metadata>` with `presetKind="ab"`.

**A/B + Morph** (Pro only):
- Same as A/B.
- Additionally append `<MorphConfig defaultPosition="<current morph position>" curve="linear" />`.
- `<Metadata>` with `presetKind="ab_morph"`.

Post-save: both slots get `presetRef[]` updated and modified flags cleared.

### Modified Indicator (Per-Slot)

Each slot tracks `modified[slot]` and `presetRef[slot]` independently.

- `modified[slot]` is set by an APVTS listener whenever a parameter changes AND that slot is the active slot.
- `modified[slot]` is cleared on: preset load into that slot, copy into that slot as destination, save of that slot.
- `presetRef[slot]` is set on preset load into that slot, copied on copy-to operations, cleared on load of a preset with a different reference.

**Existing top-level asterisk** (next to preset pill name) reflects `modified[active]` — same visual element, now sourced from the active slot. No layout change.

**Inactive slot indicator** (new): a 3px red dot appears at the bottom-center of the inactive slot's pill button when `modified[inactive] == true`. Tooltip: *"Slot X has unsaved changes"*.

### Designer-Authored Flag Lifecycle

`designerAuthored` influences one behavior: whether snaps flip discrete params regardless of `includeDiscreteInSnap`.

- **Set** when an A/B or A/B+Morph preset is loaded.
- **Cleared** when: any parameter edit fires (the comparison has left the designer's territory), a Single preset is loaded, a new preset is saved, or the plugin state is replaced via `setStateInformation`.

### Settings Toggle

New entry in the ⚙ Settings panel under a new **Compare** subsection:

```
COMPARE
[ ] Include discrete parameters when snapping A/B
    Mode, Bypass, Ghost Mode, and Binaural Mode also flip between slots.
```

State persists in the plugin state tree (`<ABSlots includeDiscrete="0|1">`). Per-project, not global.

---

## UI

### Header — A/B Cluster

Placed in `index.html` immediately after the save button and before the mode toggle group (`<div class="mt">`). Structure:

```html
<div class="ab-group" style="display:inline-flex; align-items:center; gap:3px; margin:0 8px;">
  <button class="ab-btn active" id="ab-slot-a" title="Snap to slot A">A</button>
  <button class="cp-btn" id="ab-copy" title="Copy active slot to other slot">A→B</button>
  <button class="ab-btn" id="ab-slot-b" title="Snap to slot B">B</button>
</div>
```

**Sizing and style (neumorphic, matching `.wheel-mount` / `.tog` language):**

| Element  | Size   | Active fill                                           | Inactive fill                              |
|----------|--------|-------------------------------------------------------|--------------------------------------------|
| A / B    | 22×22  | linear-gradient(180deg, #E8EAEC, #D4D6D8) + inner shadow | rgba(0,0,0,0.08) + subtle top highlight |
| Copy     | 26×22  | rgba(0,0,0,0.05)                                      | 0.4 opacity when slots A and B are identical (button becomes unclickable) |

Active A/B button: text color `rgba(0,0,0,0.9)`, inset shadow `inset 0 1px 0 rgba(255,255,255,0.6), 0 1px 2px rgba(0,0,0,0.15)`.
Inactive A/B button: text color `rgba(0,0,0,0.45)`.

**Copy button label** is dynamic in JS:
- `active == A` → label `"A→B"`
- `active == B` → label `"B→A"`

**Inactive-slot modified dot:** a 3px red dot (`background: #c74a4a`) absolutely positioned at the bottom-center of the inactive A/B button when its slot is modified. Hidden when not modified. Hidden on the active button (existing top-level asterisk covers that case).

**No drop-shadows, no halos.** Follows the established Neumorphic UI feedback.

### Browser — A/B Column

In `preset-system.js:renderBrowserList`:

**Header columns** (add `{ id: 'kind', label: 'A/B' }` between `designer` and `shape`):

```js
const headerCols = [
  { id: 'name',     label: 'Name' },
  { id: 'type',     label: 'Type' },
  { id: 'designer', label: 'Designer' },
  { id: 'kind',     label: 'A/B' },
  { id: 'shape',    label: 'Shape' },
  { id: 'skip',     label: 'Skip' },
  { id: 'heart',    label: '♥' },
];
```

**Grid template columns** (existing `1fr 72px 72px 140px 40px 30px` → add 54px for kind):

```css
grid-template-columns: 1fr 72px 72px 54px 140px 40px 30px;
```

**Badge rendering** per row based on `r.meta.presetKind`:

| Kind         | Markup                                                                                                |
|--------------|-------------------------------------------------------------------------------------------------------|
| `single`     | `<span style="color: rgba(0,0,0,0.25);">—</span>`                                                    |
| `ab`         | `<span style="background: rgba(0,0,0,0.06); color: rgba(0,0,0,0.55); padding:2px 6px; border-radius:3px; font-size:9px; font-weight:600; letter-spacing:0.5px;">A\|B</span>` |
| `ab_morph`   | same shape, `background: rgba(0,0,0,0.14); color: rgba(0,0,0,0.75);` and label `A\|B·M`              |

**Sort order** (ascending): `single` → `ab` → `ab_morph`. `browserSort.column === 'kind'` maps to this order.

**Filter dropdown:** new `<select>` sibling to the existing search input in the browser's header area. Options: *All*, *Single*, *A/B*, *Morph*. Default `"All"`. Applied as a row-filter before the existing search-term filter.

### Save Modal — Preset Kind Radio

Below the existing Type dropdown in the save modal, add:

```
Preset Kind:
  (•) Single    ( ) A/B     [Pro only: ( ) A/B + Morph]

  [helper text, shown only when A/B / A/B + Morph disabled:]
  Slot B is unchanged from Slot A. Snap to B and make edits first, or save as Single.
```

Radio disabled state uses 0.4 opacity + `cursor: not-allowed` on the label.

Helper text only rendered when the disabled state is active.

Standard builds do not render the "A/B + Morph" option at all (no locked placeholder).

### Settings Panel — Compare Subsection

Below any existing settings sections in the ⚙ modal:

```
COMPARE
──────────────────────────────
[ ] Include discrete parameters when snapping A/B
    Mode, Bypass, Ghost Mode, and Binaural Mode also flip between slots.
```

Checkbox state syncs to/from `abSlots.includeDiscreteInSnap` via a native function bridge.

---

## Native Function Bridge

New native functions on `PhantomEditor`, exposed via `withNativeFunction` (the correct JUCE bridge pattern — see preset system feedback memory):

| Function                             | Returns / Accepts                                           |
|--------------------------------------|-------------------------------------------------------------|
| `abGetState`                          | `{active: "A"|"B", modifiedA: bool, modifiedB: bool, slotsIdentical: bool, includeDiscrete: bool}` |
| `abSnapTo(slot)`                      | slot in {"A","B"}. Returns new state.                       |
| `abCopy()`                            | copies active → other. Returns new state.                   |
| `abSetIncludeDiscrete(on)`            | bool; persists. Returns new state.                          |

On the JS side (in `preset-system.js:initNativeBridge`), these are resolved once at init via `window.Juce.getNativeFunction("...")` and called with `await`. Every function in the table above MUST be looked up via `getNativeFunction` — direct property access (`window.Juce.abSnapTo(...)`) returns `undefined` and the call silently does nothing. This is the single most important bridge pattern in this codebase; see `docs/superpowers/specs/2026-04-19-preset-system-design.md` for the prior incident that established the rule.

**JS-side state synchronization:** the UI renders from a local `state.abState` object that mirrors `abGetState`'s return value. It is refreshed by:
- Calling `abGetState()` on plugin load and after any native A/B action (`abSnapTo`, `abCopy`, `abSetIncludeDiscrete`).
- Listening for APVTS parameter change events (already wired via `WebSliderRelay`) and flipping the active slot's modified flag locally without a round trip — then reconciling on next native action. An explicit push channel from the processor is not required for this spec; if the UI ever shows stale state in practice, add a lightweight poll or JUCE `WebValueStateRelay`.

The browser's preset-kind column reads `presetKind` from the existing preset metadata already delivered via `getAllPresets` — no new native function required for browsing. But `getAllPresets` returns must include the new `presetKind` field; `PresetManager::PresetMetadata` gains a `juce::String presetKind` member populated during `readMetadataFromFile`.

---

## File Structure

### New Files

```
Source/
  ABSlotManager.h
  ABSlotManager.cpp

(no new JS file — A/B logic is folded into existing `preset-system.js`; see Modified Files below. Rationale: the save modal, browser column, load path, and preset metadata are all already owned there, and the previous session's consolidation lesson favors fewer modules over more.)

tests/
  ABSlotManagerTests.cpp
```

### Modified Files

```
Source/
  PluginProcessor.h     // + ABSlotManager member; exposed via accessor
  PluginProcessor.cpp   // + initialize abSlots; persist <ABSlots> child
  PluginEditor.h/cpp    // + 4 native functions; wire into getResource/withNativeFunction
  PresetManager.h       // + PresetMetadata.presetKind; + kind parameter on savePreset
  PresetManager.cpp     // + read presetKind in readMetadataFromFile
                        //   + emit <SlotB> on ab save
                        //   + populate slot B on ab load
                        //   + strip <SlotB>/<MorphConfig> when reading slot A

Source/WebUI/
  index.html            // + A/B cluster markup in header
  styles.css            // + .ab-btn, .cp-btn, .ab-group styles
                        //   + .kind-badge styles (softer gray palette)
  preset-system.js      // + kind column, filter dropdown, save-modal kind radio
                        // + A/B cluster wiring (snap/copy buttons, active state, modified dot)
                        // + settings toggle wiring
                        // + native-function refs: abGetState, abSnapTo, abCopy, abSetIncludeDiscrete

tests/
  CMakeLists.txt        // + ABSlotManagerTests.cpp
```

### Build System

No CMake changes beyond the test source add. The `KAIGEN_PRO_BUILD` flag is *not* defined in this spec — Pro-build wiring happens when the Morph module spec is written.

---

## Testing

### Unit Tests — `ABSlotManagerTests.cpp`

Following the existing `tests/` Catch2-style pattern (mirroring `PresetManagerTests.cpp` structure):

1. **Construction** — fresh `ABSlotManager` initializes both slots from current APVTS, `active == A`, both modified flags false.
2. **snapTo — same-slot no-op** — `snapTo(A)` while already on A produces no change to slot storage or APVTS.
3. **snapTo — commits live state** — tweak a param, `snapTo(B)`, `snapTo(A)`. Slot A's state reflects the tweak; param currently live is back to tweaked value.
4. **snapTo — includeDiscrete=false preserves discrete params** — load different Ghost Modes into A and B, disable include-discrete, snap. Live Ghost Mode stays at pre-snap value; continuous params flip.
5. **snapTo — includeDiscrete=true flips discrete params** — same setup, enable setting, snap. Live Ghost Mode flips.
6. **snapTo — designerAuthored overrides includeDiscrete=false** — load an A/B preset with different Ghost Modes, leave setting off, snap. Discrete flips (designer intent honored).
7. **copy — full snapshot copy** — edit A, copy A→B. `slots[B]` deep-equals `slots[A]`.
8. **copy — clears destination modified flag** — mark slot B modified, copy A→B. `modified[B] == false`.
9. **copy — identical slots is no-op** — two copies in a row don't flip any flags.
10. **toStateTree / fromStateTree round-trip** — serialize + deserialize preserves slot contents, active, includeDiscrete setting.
11. **fromStateTree — no ABSlots child** — both slots initialize from current APVTS, active=A, includeDiscrete=false.
12. **loadSinglePresetIntoActive — other slot untouched** — preset loads into A; slot B's prior content is identical after load.
13. **loadABPreset — populates both slots + sets designerAuthored** — preset root → slot A, `<SlotB>` → slot B, flag set.
14. **designerAuthored cleared on parameter edit** — after A/B load, tweak any param. Flag clears; subsequent snaps honor user setting.
15. **Modified flag set on APVTS change (active slot only)** — tweak param while on A: `modified[A]=true`, `modified[B]=false`.

### Integration Tests

1. **Save-as-A/B round-trip** — populate A and B distinctly, save as A/B, reload into fresh processor, slots match.
2. **Save-as-A/B rejected when slots identical** — `PresetManager::savePreset` with `kind=ab` and identical slots returns failure with the expected error enum/message.
3. **Plugin state round-trip** — full `getStateInformation` / `setStateInformation` cycle preserves slots + active + setting.
4. **Loading an A/B preset into a Standard build with `<MorphConfig>` present** — morph config silently ignored, A/B preset works normally.

### Manual Test Checklist (added to `docs/manual-test-checklist.md`)

1. Snap A↔B with include-discrete OFF → no audible click at crossover even if Ghost Modes differ between slots.
2. Snap A↔B with include-discrete ON → click audible only if Ghost Mode or Binaural Mode actually differ.
3. Load an A/B preset, don't edit, snap back and forth → discrete params flip (designer-authored overrides setting).
4. Load A/B preset, tweak any knob, snap → discrete params no longer flip (designer-authored cleared).
5. Active slot modified indicator (top-level asterisk) matches behavior; inactive slot red dot appears/clears correctly.
6. Save project in Ableton, reopen → both slots restore with matching contents, active slot preserved.
7. Browser A/B column renders correct badges for Single / A/B / A/B+Morph presets; sort works; filter dropdown filters correctly.
8. Save modal disables A/B radio when slots are identical, shows helper message.

---

## Pro Seam / Out of Scope

### Locked Architectural Seam

The following interfaces are stable and will be consumed by the future Pro `MorphEngine`:

- `ABSlotManager::getSlot(Slot)` — read-only `ValueTree&` accessor. Morph engine reads both slots each block.
- The `<MorphConfig>` preset child and `presetKind="ab_morph"` metadata value. Standard build reads but ignores; Pro build writes on save and reads on load.
- `PresetManager::savePreset(..., PresetKind kind)` — the `kind` enum has an `ab_morph` member. Standard builds simply never reach the save path for that kind (radio hidden).

### Not Designed Here

- **Morph DSP engine** (parameter-level interpolator, optional dual-engine crossfader for discrete params, per-block evaluation, zipper-noise smoothing, automation parameter exposure). Separate spec when Pro work begins.
- **Morph knob UI** (placement, visual style, automation binding, touch/edit semantics when morph is mid-position).
- **License key / runtime unlock** — currently out of scope; compile-time `KAIGEN_PRO_BUILD` only.
- **Factory A/B presets** — designer-authored A/B presets can ship in the Factory pack with no format changes; saving them is a content task, not a code task.
- **A/B undo/redo history** — slot storage is flat; there is no per-slot edit timeline. Could be future feature.
- **Migration of existing factory/user presets to presetKind metadata** — unnecessary; absence of `presetKind` metadata is interpreted as `"single"` by browser/load logic.

---

## Open Questions Carried Forward

None for Standard build. All decisions pinned.

For Pro Morph Module (future spec):
- Parameter-level interpolation only, or hybrid with optional dual-engine DSP crossfade for the four discrete params?
- Morph knob touch/edit semantics — write to nearer slot, lock morph position, or snap-to-nearest-on-touch?
- Morph curve shape — linear only, or multi-curve (S-curve, exponential, user-drawn)?
