# Recipe wheel: auto-switch to Custom + save/delete pills — design

**Date:** 2026-05-22
**Branch:** `integration/native-plus-reverb`
**Status:** approved (in-conversation)

## Goal

When the user manually edits an H value (recipe wheel spoke or H knob),
the plugin should automatically promote the current recipe to a Custom
slot rather than silently mutating a built-in preset's display state.
Each Custom slot gets a small save + delete pill below its WordSelector
entry — the save pill blinks when the slot has unsaved changes, the
delete pill clears the slot back to empty.

## Behaviour summary

### Slot state (per engine, per Custom slot)

Three logical states, derived live from `{filled, isCurrentPreset, savedH vs currentH}`:

| State | Meaning |
|---|---|
| `EMPTY` | `filled == false`. No stored values. Slot is available as an auto-switch target. |
| `SAVED` | `filled == true`. Stored values match the current live H values. (Only meaningful when this slot IS the currently selected preset; otherwise state is "filled-not-current".) |
| `DIRTY` | `filled == true` AND this slot is currently selected AND current live H values ≠ stored values. Save pill blinks. |

A fourth informal state: when an `EMPTY` slot is the currently selected
preset (which happens when the user just auto-switched into it), the
save pill blinks because the user has pending edits with nowhere to
commit them yet. Internally this is just `EMPTY + isCurrentPreset`; the
pill rendering rule decides to blink.

### Editing rules

When the user moves a recipe-wheel spoke or an H mini-knob:

1. If the active `recipe_preset` is a **Custom slot** (indices 6, 7, 8):
   - Edit goes through normally.
   - If `filled` and live H now differs from `savedH` → slot becomes
     `DIRTY` (save pill blinks).

2. If the active `recipe_preset` is a **built-in** (indices 0–5):
   - Find the first `EMPTY` Custom slot for the active engine.
   - **Found:** change `recipe_preset` to that slot's index. The edit then
     applies to the (now selected) Custom slot. Since it's `EMPTY +
     isCurrentPreset`, the save pill blinks.
   - **None found** (all three Custom slots are `filled`): the H edit is
     **silently rejected** — the live H param does not change. The wheel
     briefly flashes (visual cue that the edit was refused).

### Save / delete pill behaviour

Each Custom slot has two small circular pill widgets below its
WordSelector entry, ~10 × 10 px each, side by side.

**Save pill (left of pair):**

| Condition | Visual |
|---|---|
| This slot is NOT the active preset | grey (dimmed) |
| This slot IS the active preset, state = `SAVED` | green solid |
| This slot IS the active preset, state = `DIRTY` or `EMPTY` with edits | red blinking (~2 Hz) |

Click action (only when active): write the current 7 H param values into
`recipeSlots[engine][slot].savedH`, set `filled = true`. Save pill goes
solid green.

**Delete pill (right of pair):**

| Condition | Visual |
|---|---|
| `filled == false` | grey (dimmed, no-op) |
| `filled == true` | red (active, clickable) |

Click action: set `filled = false`, zero out `savedH`. If this slot is
currently selected, the preset changes to **the first non-empty Custom
slot if any**, else to the **last selected built-in** (we track this),
else fallback to "Warm" (index 0). The newly-selected preset's H values
load into the live params via the standard preset-load path.

## Storage

```cpp
// In PhantomProcessor (new):
struct RecipeSlot
{
    bool                     filled { false };
    std::array<float, 7>     savedH {};   // H2..H8, normalised [0..1]
};

std::array<std::array<RecipeSlot, 3>, 2> recipeSlots;  // [engineIdx][slotIdx]

// Also track last-selected built-in per engine for the delete-cleanup
// fallback. Initialised to 0 ("Warm").
std::array<int, 2> lastBuiltInPreset { 0, 0 };
```

Serialised in `getStateInformation` / `setStateInformation` as a child
`ValueTree` named `"recipeSlots"` keyed by `engine.slot` indices.

**Compatibility with old saved state:** missing `recipeSlots` tree =
all slots `EMPTY`, `lastBuiltInPreset` = 0. Old projects load cleanly.

## Native preset-loading (new subsystem)

The native side currently has no preset-load logic — when
`a_recipe_preset` / `b_recipe_preset` changes, nothing writes to the H
params. (The WebView's JS does this, so users on the WebView never
noticed.) For this feature to work natively, we need:

**`PhantomProcessor::applyRecipePreset(int engineIdx, int presetIdx)`:**

- For built-in indices 0–5: look up the hardcoded amplitude array
  (`kWarmAmps`, `kAggressiveAmps`, `kHollowAmps`, `kDenseAmps`,
  `kStableAmps`, `kWeirdAmps`) and write each value `× 100` into the
  active engine's `a_/b_recipe_h2..h8` APVTS params.
- For custom indices 6–8: if `recipeSlots[engineIdx][slotIdx].filled`,
  write `savedH[i] × 100` into the H params. If not filled, do nothing
  (leave H values as they were — the user is editing into an empty slot).
- Sets a private `bool loadingPreset` flag for the duration of the
  writes so the H-change listener (next section) doesn't treat the
  preset-load as a user edit.

**Hook**: `parameterChanged` already exists on `PhantomProcessor`; add a
listener for `a_recipe_preset` and `b_recipe_preset` that calls
`applyRecipePreset` when the value changes.

## H-change listener / auto-switch logic

When any `a_recipe_h2..h8` (or `b_` equivalent) APVTS param changes:

```text
if (loadingPreset) return;                   // ignore self-induced writes

const int engineIdx = paramID.startsWith("b_") ? 1 : 0;
const int currentPreset = (int) recipePresetParam(engineIdx)->load();

if (currentPreset >= 6 && currentPreset <= 8) {
    // On a custom slot — just let the change happen. UI re-derives DIRTY.
    return;
}

// On a built-in. Auto-switch.
const int emptySlot = findFirstEmptyCustomSlot(engineIdx);
if (emptySlot < 0) {
    // All customs full → revert the H change.
    revertHParam(paramID);
    triggerWheelLockFlash(engineIdx);
    return;
}

// Remember which built-in we came from for delete-fallback.
lastBuiltInPreset[engineIdx] = currentPreset;

// Switch preset to the empty slot (offset 6 = Cust 1, etc.).
setLoadingPresetGuard(true);
recipePresetParam(engineIdx)->setValueNotifyingHost(slotToNormalised(6 + emptySlot));
setLoadingPresetGuard(false);
```

**Revert mechanism:** keep a per-engine `std::array<float, 7> previousH`
buffer updated *before* every preset-load + every accepted edit. On
reject, write `previousH` back into the param.

**Guard race:** the `loadingPreset` flag is single-threaded (message
thread only — APVTS listener fires on the message thread by default).
No atomic needed.

## Per-engine recipe preset selector

Today `recipePresetSelector` in `LeftPanel` is bound only to
`a_recipe_preset` and the inline comment says it's "UI-only, excluded
from per-engine sync." That breaks the auto-switch story when the user is
focused on engine B — switching B's preset wouldn't be reflected.

**Fix:** the `WordSelector` already supports `setEnginePrefix`. Add
`recipePresetSelector.setEnginePrefix(activePrefix, mirrorPrefix)` in
`LeftPanel::setEnginePrefix` (the comment-out line gets restored). On
engine focus change, the selector retargets to `b_recipe_preset` and the
slot pills (next section) re-fetch state for engine B.

## RecipeSlotPills widget

**New file:** `Source/UI/widgets/RecipeSlotPills.{h,cpp}`.

A `juce::Component` that paints and handles the 3-row × 2-pill array
beneath the WordSelector. Sized to match the WordSelector's bottom-row
horizontal extent (just the Cust 1/2/3 columns; the top two rows of the
selector are built-ins and get no pills).

**Layout** (per row, ~12 px tall, one row per Custom slot):

```text
[ save-dot ]  [ delete-dot ]
```

- Each dot ~6 × 6 px circle, ~4 px gap between them.
- Row centred horizontally under the corresponding "Cust N" word.

**State source:** holds a reference to `PhantomProcessor`. On a 4 Hz
`juce::Timer` (matches a smooth blink period of 250 ms on/off):

- Read active engine focus (`processor.getEngineFocus().activeTab`).
- For each slot `s ∈ {0,1,2}`:
  - Read `recipeSlots[engineIdx][s].filled`.
  - Determine `isCurrent = (currentPresetIdx == 6 + s)`.
  - For DIRTY check: compare live H params to `savedH`.
  - Compute pill colours.
- Toggle internal `blinkPhase` and repaint.

**Mouse handling:** `mouseDown` checks which pill was hit and calls the
appropriate `processor.saveRecipeSlot(engineIdx, s)` or
`processor.clearRecipeSlot(engineIdx, s)`. Hit testing is per-row +
per-dot (left vs right halves of the row, ignoring the gap).

**Integration:** `LeftPanel` owns the new widget alongside
`recipePresetSelector`. Layout in `LeftPanel::resized()` places the pills
strip directly below the WordSelector, taking ~40 px of vertical space
(3 rows × ~12 px + padding).

## PhantomProcessor public surface (additions)

```cpp
// Slot data accessors (UI-thread reads).
const RecipeSlot& getRecipeSlot(int engineIdx, int slotIdx) const noexcept;

// Slot mutations (UI-thread).
void saveRecipeSlot (int engineIdx, int slotIdx);   // copy current H → slot
void clearRecipeSlot(int engineIdx, int slotIdx);   // mark slot empty + delete fallback

// Compatibility queries for the pills widget.
int findFirstEmptyCustomSlot(int engineIdx) const noexcept;   // -1 if none
int getLastBuiltInPreset(int engineIdx) const noexcept;
```

The auto-switch logic + preset-load + H listener are all internal —
`PhantomProcessor` already implements `juce::AudioProcessorValueTreeState::Listener`,
so we extend the existing `parameterChanged` switch.

## Wheel-lock visual cue

When the H-change is rejected (all customs full + on built-in), the
`RecipeWheel` flashes briefly. Add a public `RecipeWheel::triggerLockFlash()`
method — paints a 100 ms red-tinted overlay over the wheel, auto-clears.
Driven by a `juce::Timer` inside the wheel. Processor triggers via a
listener pattern (RecipeWheel listens to a `ChangeBroadcaster` exposed
on the processor) so we don't introduce a direct processor → wheel pointer.

## Out of scope (explicit)

- The WebView UI doesn't get the pills or auto-switch — the WebView is
  the secondary UI (native-only loads by default). If the user explicitly
  switches to WebView later, recipe behaviour there stays as-is.
- No undo for save/delete actions in this commit (could be added later
  via JUCE's UndoManager).
- The `RecipeWheel` doesn't get reverted to show the "previous" H values
  on lock-reject visually — the param simply doesn't change, so the
  wheel doesn't move. The flash is the only feedback.
- Slot rename / labelling (no "Cust 1" → "Cust:warm-edit" feature).
- Shared slots across A/B — explicitly per-engine.

## Migration / compatibility

- Existing presets that don't have a `recipeSlots` ValueTree load with all
  Custom slots `EMPTY` and `lastBuiltInPreset = 0`. No behaviour change
  until the user starts editing H.
- Adding the H-change listener doesn't break existing H-knob behaviour —
  on a Custom slot, edits still go through; only the auto-switch /
  revert path is new.

## Testing notes

- Build 0 errors.
- User listen / interaction test in Ableton:
  - Load default — preset = Warm, no slot filled.
  - Drag H4 spoke → preset auto-switches to Cust 1, save pill blinks red.
  - Click save pill → save pill solid green; H values stored.
  - Drag H4 again → pill blinks red (DIRTY).
  - Click delete pill → preset drops back to Warm, slot empty, pill grey.
  - Repeat to fill Cust 2 and Cust 3.
  - With all three filled, on Warm, drag H4 → wheel flashes red, no
    change to H value.
  - Click delete on Cust 2 → Cust 2 empty, on Warm again drag H4 →
    auto-switches into Cust 2.
- Project save/reload — Custom slot contents survive a host project
  save/reload cycle.

## File summary

| File | Change |
|---|---|
| `Source/PluginProcessor.h` | `RecipeSlot` struct, `recipeSlots`, `lastBuiltInPreset`, accessors, `loadingPreset` flag, `wheelLockBroadcaster` |
| `Source/PluginProcessor.cpp` | Slot mutations, `applyRecipePreset`, H-change auto-switch, recipe_preset listener, serialise/deserialise additions |
| `Source/UI/widgets/RecipeSlotPills.{h,cpp}` | NEW — pill widget |
| `Source/UI/widgets/RecipeWheel.{h,cpp}` | `triggerLockFlash` + listener for the broadcaster |
| `Source/UI/panels/LeftPanel.{h,cpp}` | Add pills member, layout under WordSelector, restore per-engine prefix retargeting on the recipe-preset selector |
| `CMakeLists.txt` | Register new pills source file |
