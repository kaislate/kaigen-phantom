# PR3b-rev: Matrix View Replaces Drawer-Based Macro Editor

**Status:** Design — supersedes the drawer + per-macro editor in `docs/superpowers/plans/2026-05-05-pr3b-macro-editor.md`.
**Branch:** `feature/pr3b-macro-editor` (continues; no new branch).
**Last drawer commit before pivot:** `0303490`.
**Date:** 2026-05-06.

---

## Background

PR3a delivered the C++ modulation framework (Modulator base, Routing struct, ModulationEngine, getModulatedValue, lock-free atomic snapshot). PR3b's first attempt added a **drawer + per-macro editor** as the UI: clicking a macro slot in the bottom row opened an above-row drawer with a knob, a name field, an "+ add destination" dropdown, and a list of routings.

Manual smoke testing surfaced four issues:

1. **Empty space in the drawer** — drawer was sized for the dropdown picker but wraps tightly around the routing list, leaving dead space.
2. **Macro audio path uncertainty** — the user couldn't audibly confirm a routed macro affected its destination. (A live `base → modulated` diagnostic was added but never confirmed.)
3. **Macros 3/4 didn't switch the engine focus tab.** (Patched.)
4. **Destination swap required delete + add.** (Patched with a `<select>` element.)

Beyond those, the user's deeper objection was structural: the drawer can show only one macro at a time, hides routing relationships across modulators, and feels heavyweight (open → click → delete → add). The pivot is to a **matrix view inspired by Vital and Roar (Ableton)** — modulators as rows, destinations as columns, click intersections to add/edit/remove routings, with all modulators visible at once and routing relationships obvious from the grid pattern.

---

## Goal

Replace the drawer + per-macro editor with a matrix view that occupies the same vertical space when expanded, surfaces all routings at once, makes adding/editing/removing routings a single click, and is forward-compatible with PR4 (LFOs) and PR5 (Random) modulators.

---

## Architecture

### What stays (already shipped)

- **C++ modulation framework**: Modulator, Routing, ModulationEngine, atomic-snapshot reads, getModulatedValue, persistence in plugin state, validation against per-engine prefix.
- **Native bindings on `WebBrowserComponent`**: `modulationGetState`, `modulationGetLiveState`, `modulationAddRouting`, `modulationRemoveRouting`, `modulationSetRoutingDepth`, `modulationSetMacroName`. The matrix view consumes the same bindings with no signature changes.
- **`Source/Modulation/*`** — no C++ changes.
- **Bottom-row 11-slot panel** in `index.html` — kept as the always-visible **slots view** with live animations added (see Animated Modulator Strip below).

### What's removed

- `Source/WebUI/macro-editor.js` — the entire drawer-rendering module. **Delete.**
- The drawer container (`#modulation-drawer`) in `index.html` and the related CSS in `styles.css`.
- The drawer-mechanics half of `modulation-panel.js` (open/close, growEditor/shrinkEditor on slot click, the `kaigenRenderMacroEditor` integration). The slot-click handler that auto-switches engine focus is preserved but reused in the matrix view's modulator-row click handler.
- The `<select>`-based destination swap, the live `base → modulated (mod=N)` diagnostic strip — all subsumed by the matrix.

### What's new

Two sibling views inside the bottom panel, toggled by a **SLOTS / MATRIX** segmented control at top-left of the panel:

1. **SLOTS view** (default) — the current 11-slot row, but each slot's static colored dot is upgraded to a **live animation** appropriate to its type. This gives an at-a-glance "who's modulating right now" readout when the user is focused elsewhere in the editor.
2. **MATRIX view** — the new full-width grid. Modulators are rows; destinations are columns grouped into expandable categories. Engine A on top, Engine B below.

Both views live in the same DOM region (`#modulation-panel`) and toggle visibility via classes. The plugin window grows to accommodate the matrix only when matrix mode is active (820 → 980, same height-management approach as the old drawer).

---

## Data flow (unchanged from PR3b)

```
JS                                   C++ (audio thread)
─────────────────────────────────────────────────────────
matrix click on empty cell           modulationAddRouting(...)
   ↓                                    ↓
window.kaigenRefreshMatrix()         ModulationEngine writes
calls modulationGetState()           atomic shared_ptr<routings>
   ↓                                    ↓
re-render the matrix                 audio thread reads via
   ↓                                  std::atomic_load
matrix.css drives cell color/fill    syncEngineFromPrefix() applies
   ↓                                  modulator value with depth
listener on modulationStateChanged   ↓
event re-pulls live state            DSP receives modulated value
```

The single new wiring is a **periodic poll** (15 Hz) of `modulationGetLiveState` to drive the modulator-row animations and per-cell live-modulation indicators (see "Animations" below). This replaces the per-routing `base → modulated` diagnostic from the drawer.

---

## Visual design

The mockup at `.superpowers/brainstorm/.../matrix-layout.html` is the reference. It is the spec for visual layout. Key elements, formalized:

### Bottom-panel header

```
[ SLOTS ] [ MATRIX ]                               5 ROUTINGS · 2 MACROS ACTIVE
```

- Segmented toggle with `is-active` styling (teal glow) on the selected mode.
- Right-side counter: total routings across both engines + active macros (any macro with depth > 0 anywhere). Updates on state change.

### Slots view (existing 11-slot row, with live animations)

Layout unchanged. Each slot's `.mod-slot-dot` is replaced by a type-specific live element:

| Slot type | Live element | Driven by |
|-----------|--------------|-----------|
| Macro 1–4 | Conic-gradient ring sized to current macro value (0–1). | `modulationGetLiveState().macros[macroN].value` |
| Morph | Conic ring sized to morph_amount (0–1). | APVTS `morph_amount` (already wired). |
| LFO 1–4 | Mini SVG waveform (placeholder static path; greyed `is-disabled`). | n/a until PR4. |
| Random A/B | 3×3 scatter dot grid (placeholder static; greyed). | n/a until PR5. |

The dot was previously an inert color swatch. Replacing it does **not** change the slot's role as a click-target — clicking a macro slot in slots view now toggles to **matrix view with that macro's row scrolled into view and briefly highlighted**, instead of opening a drawer. This preserves the user's mental model: "click MAC 2 to edit MAC 2's routings."

### Matrix view

Stacked layout:

```
ENGINE A                                    [legend: bipolar fill = polarity, intensity = depth]
                  ┌──RECIPE────┐ ┌─FILTER─┐ ┌─ENVELOPE─┐ ┌──RESYN──┐ ┌─STEREO+─┐ ...
                  │ H2 H3 H4...│ │ LPF HPF│ │ ATK  REL │ │ ...     │ │ ...     │
MAC 1 [●ring]    │ ▓▓▓ . . .  │ │ . .   │ │ . .      │ │ . . .   │ │ . . .   │
MAC 2 [●ring]    │ . ▓▓▓ . .  │ │ . .   │ │ . .      │ │ . . .   │ │ . . .   │
LFO 1 [≈≈]       │ . . . . .  │ │ . .   │ │ . .      │ │ . . .   │ │ . . .   │
…
                                  ─────────────────────
ENGINE B
…
```

#### Modulator rows (left strip)

Per row, a fixed 130 px wide cell containing:

- **Live-animated icon** (28×28 px). Macros = conic ring sized to value. LFOs = mini waveform (placeholder). Random = scatter (placeholder).
- **Name label** (uppercase, 9 px tracked). Color-coded by type:
  - Macro = `#5DD3E0` (teal)
  - LFO = `#4A90E2` (blue) — greyed at 55% opacity until PR4
  - Random = `#9990E0` (purple) — greyed until PR5
- **Live value readout** (right-aligned, monospace) — current modulator output (0.00–1.00 for macros). Greyed PR4/PR5 placeholders show "PR4" / "PR5" text instead.
- **Click target**: clicking a macro modulator-row label inline-expands a name editor (single-line input replacing the label). Press Enter to commit (calls `modulationSetMacroName`), Esc to cancel. Same name editor as the drawer's, just inlined.

Row order within each engine: **Macros → LFOs → Random**. Engine A: MAC 1, MAC 2, LFO 1, LFO 2, RAND. Engine B: MAC 3, MAC 4, LFO 3, LFO 4, RAND. Morph is **not** a row in matrix view (it's a global fader, not an engine-scoped modulator).

#### Destination columns (right grid)

Columns are grouped into **expandable category headers**. Each category collapses to show ~3 representative columns and an `EXPAND ▸` affordance; clicking expands inline to show all params in that category. State is per-engine and persisted in plugin state alongside the existing `EditorFocus` / `SpectrumView` blocks (new sub-element `<MatrixView>`).

Categories per engine (grouped by `Source/Parameters.h` section comments):

| Category | Params (continuous only — choice/bool params are excluded) |
|----------|------------|
| **GHOST** | ghost, phantom_threshold, phantom_strength, output_gain |
| **RECIPE** | recipe_h2, recipe_h3, recipe_h4, recipe_h5, recipe_h6, recipe_h7, recipe_h8, harmonic_saturation |
| **SHAPE** | synth_step, synth_duty, synth_skip |
| **ENVELOPE** | env_attack_ms, env_release_ms |
| **FILTER** | synth_lpf_hz, synth_hpf_hz |
| **RESYN** | synth_wavelet_length, synth_gate_threshold, synth_h1, synth_sub |
| **PITCH** | synth_min_samples, synth_max_samples, tracking_speed, punch_amount, synth_boost_threshold, synth_boost_amount |
| **STEREO** | binaural_width, stereo_width |
| **MIDI** | midi_gate_release |

Total ~30 columns per engine. Excluded params (choice/bool): mode, ghost_mode, binaural_mode, env_source, midi_trigger_enabled, punch_enabled, recipe_preset, synth_filter_slope. These are not modulation-eligible. The matrix should not present them as columns, and `modulationAddRouting` should already reject them via the existing param-validation path (verify in implementation).

Column header label is the param's leaf name in uppercase, abbreviated to ~6 chars (e.g. `phantom_threshold` → `PHTHR`, `synth_lpf_hz` → `LPF`, `synth_wavelet_length` → `WVLEN`). The full param ID is shown in a tooltip on hover.

#### Cells

Each cell is one (modulator, destination) intersection. Three states:

| State | Visual | Click behavior |
|-------|--------|----------------|
| **Empty** | 30 px cell, transparent dark fill, dim text. | Click → adds routing at +50% depth. Cell transitions to "active" state. |
| **Active** | Cell filled with a horizontal gradient — left-to-right for positive depth, right-to-left for negative — in the modulator's color. Numeric depth overlay in white (`+50`, `-25`). 1 px inset border in the modulator's color. | Drag vertically inside cell → adjusts depth (−100 to +100, snap to ±0). Right-click or shift-click → removes routing. Releasing drag commits via `modulationSetRoutingDepth`. |
| **Live-modulating** (active + the modulator is currently affecting it) | Same as active, but with a faint pulsing glow (CSS animation pegged to the live modulator value). | Same as active. |

Drag-to-adjust depth is the primary depth-editing interaction. As an additional affordance, right-clicking an active cell opens a small popover with `−100 / 0 / +50 / +100` quick-set buttons and a Remove option — useful for users who don't realize cells are draggable. The popover pattern is already used for the morph crossfader's right-click options; reuse the existing `popover.js` helper if practical, otherwise inline the minimal popover.

#### Engine A vs B separation

A horizontal divider gradient runs full-width between Engine A's last row and Engine B's first row. Each block has its own column headers. A label (`ENGINE A` / `ENGINE B`) sits at the top of each block, color-tinted (A = blue, B = teal) to match the existing tab paradigm.

---

## Animations

A 15 Hz polling loop in `matrix.js` calls `modulationGetLiveState` on a `setInterval`, throttled to pause when the editor is not visible (use `document.visibilityState`).

```js
const liveState = modulationGetLiveState();
// liveState = {
//   macros: { macro1: { value: 0.65 }, macro2: { value: 0.22 }, ... }
// }
```

Each macro's `--v` CSS custom property on its conic ring is updated to the current value × 100. The conic-gradient renders the fill purely from CSS, so this is one DOM property write per macro per frame — cheap.

Per-cell live-modulating glow: for each active routing, if `liveState.macros[mod].value * routing.depth > 0.05`, the cell receives an `is-modulating` class. This drives a CSS animation that pulses the cell's box-shadow at intensity proportional to the current modulation contribution. Removed when the contribution drops below threshold.

LFO and Random animations are **placeholders** in this PR — static SVG paths and a static dot grid, both greyed. Their live-data wiring lands in PR4 and PR5.

---

## Persistence

The matrix view's per-engine category-expansion state is local UI state, not preset state. Save under a new `<MatrixView>` element inside plugin state:

```xml
<MatrixView>
  <Engine id="A" expanded="GHOST,RECIPE" />
  <Engine id="B" expanded="" />
</Engine>
```

The active mode (slots vs matrix) is also persisted — `<MatrixView mode="matrix">` — so reopening the plugin returns to the user's last-active view.

Routing data itself is **already persisted** by PR3a's `<ModulationConfig>` and is not duplicated here.

---

## Error handling

- `modulationAddRouting` returns `false` if the destination is invalid (wrong prefix, not modulation-eligible, or already routed by this modulator). The matrix should ignore the click silently — the cell stays empty. Don't show an error.
- `modulationGetState` returns the canonical state. The matrix re-renders fully from this on every state change; never trust local mutation as authoritative.
- If the live-state poll throws (binding gone, race during teardown), catch and stop the polling loop; don't crash the editor.

---

## Testing

### C++ (Catch2)

No new C++ tests. The existing PR3a/PR3b modulation-framework tests continue to cover the data layer.

### JavaScript

Unit tests via the existing browser test harness aren't set up for this codebase. Manual smoke is the testing gate. Implementer-side smoke checklist (in the plan):

1. Build Standalone, open, switch to MATRIX mode.
2. Click empty Engine A / Macro 1 / Ghost cell → verify cell lights up at +50%.
3. Drag the cell down → verify depth crosses zero and goes negative; cell flips fill direction.
4. Right-click cell → popover opens with quick-set + remove.
5. Set macro 1 knob to 0.5 → verify Ghost knob's modulated reading changes by ~½ × range × 0.5 = ¼ of its range. Use an automation envelope or Live's macro mapping — the user's earlier concern was that the audio chain might not be working, so this needs to be verified audibly.
6. Repeat 2–5 for Engine B / Macro 3 to confirm engine separation.
7. Toggle to SLOTS view → verify macro 1 ring fills to 0.5.
8. Save → reload preset → verify routings + matrix mode + expansion state persist.
9. Add 5+ routings → confirm no UI lag (15 Hz poll is the load floor).

### Build & lint

`build.bat` clean + Debug. No warnings beyond baseline. The matrix runs against the unchanged PR3a/PR3b C++ — there's no risk of regression in the audio path from this PR.

---

## File map

| File | Status | Responsibility |
|------|--------|----------------|
| `Source/WebUI/macro-editor.js` | **Delete** | Drawer macro editor — replaced by matrix view. |
| `Source/WebUI/modulation-panel.js` | **Reduce** | Keep only the engine-focus auto-switch + the slot-click → matrix-mode handoff. Drawer mechanics removed. |
| `Source/WebUI/matrix.js` | **Create** | Matrix view controller: render grid, wire cell click/drag/right-click, run live-state poll, manage category expansion state. |
| `Source/WebUI/matrix.css` | **Create** | Matrix-specific styles: row layout, cell states (empty/active/live-modulating), category headers, A/B divider. (Could also live in `styles.css`; create as a separate file if the diff readability benefits.) |
| `Source/WebUI/index.html` | **Modify** | Remove `#modulation-drawer`. Add SLOTS/MATRIX toggle. Add matrix container (`#modulation-matrix`, hidden by default). Replace static `.mod-slot-dot` markup with the live-animation containers per slot. |
| `Source/WebUI/styles.css` | **Modify** | Remove drawer styles. Add SLOTS/MATRIX toggle styles. Update slot-dot styles to host the new live elements. |
| `Source/WebUI/preset-system.js` | **Modify** | Persist + restore `<MatrixView>` block. |
| `Source/PluginEditor.cpp` | **Verify** | The native bindings are already correct; confirm none rely on `macro-editor.js` being loaded. The `<script src="/macro-editor.js">` line in `index.html` must be removed. |
| `Source/PluginProcessor.cpp` | **Modify** | Add `<MatrixView>` element to plugin-state save/load. (Same pattern as `<EditorFocus>`/`<SpectrumView>`.) |
| `docs/superpowers/plans/2026-05-05-pr3b-macro-editor.md` | **Append note** | Add a top-of-doc note pointing to this revamp; mark drawer-related tasks superseded. |

No new BinaryData entries — all new files are added via `juce_add_binary_data` in `CMakeLists.txt` (matrix.js, matrix.css if separate).

---

## Out of scope (deferred)

Same exclusions as the original PR3b plan, plus:

- **Drag-to-assign**: dragging a modulator label onto a destination cell to add a routing. The click-empty-cell flow is sufficient for this PR.
- **Cell tooltip on hover** showing the full param ID + current depth + modulator name. Nice-to-have; deferred to PR3c if needed.
- **Multi-select** of cells (e.g. shift-drag to set depth across a row). Out of scope.
- **Visual indication of ranges** in the cell (showing how far +50% reaches in the param's range). The numeric `+50` is sufficient for this PR.
- **LFO and Random animations** beyond static placeholders. Land with PR4 / PR5.

---

## Open implementation questions

These are flagged for the implementer; the spec picks defaults but the implementer should confirm they don't surprise during build:

- **Cell drag interaction on Windows + WebView2**: the existing knob-drag pattern from `phantom-knob` works inside WebView2 with cursor-lock fallback. The matrix cells should reuse that pattern (or the simpler pointer events) — verify the cursor doesn't escape the cell during a long drag.
- **15 Hz polling cost**: 4 macros × `modulationGetLiveState` round-trip × 15/sec = 60 round-trips/sec across the JS↔C++ bridge. Should be fine but check the editor's CPU profile.
- **`harmonic_saturation` and similar long names**: ensure the 6-char column abbreviation table is unambiguous. Build the list during implementation; if collisions appear (`synth_h1` vs `synth_sub`...), fall back to longer column widths for those categories.

---

## Self-review

- Spec covers each of the user's matrix-view requests: animated modulators on the right (here, the modulator-row icons), grid view, columns as destinations, replaces the drawer entirely, references Vital + Roar.
- All four open questions from the brainstorm have explicit answers (column count = expandable groups; depth display = numeric overlay + bipolar fill; A/B = stacked; row order = by type).
- File map enumerates every file touched. No "TBD" or placeholder requirements.
- Bug 2 from the smoke (audio-path uncertainty) is addressed by the audible smoke step (step 5 in Testing) — the implementer must verify, not just visually confirm.
- Forward compatibility: LFO/Random rows are present but greyed; PR4/PR5 only need to wire animation data + un-grey, not restructure the matrix.
