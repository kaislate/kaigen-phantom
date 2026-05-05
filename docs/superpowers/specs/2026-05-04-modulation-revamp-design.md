# Modulation System Revamp — Design

**Date:** 2026-05-04
**Status:** Brainstorm complete; ready for implementation planning.
**Supersedes:** `2026-04-23-morph-design.md` (the per-knob arc morph), the OLED-ridge editor work (`docs/morph-oled-ridge-handoff.md`), and the existing A/B compare feature (`2026-04-22-ab-compare-design.md`).

---

## Vision

Replace the current "single engine + per-knob arc morph + opt-in Scene Crossfade + A/B snapshot compare" model with a **dual-engine, Pigments/Vital-style modulation system**:

- **Two PhantomEngine instances run continuously.** Engine A and Engine B each carry their own complete state (recipe wheel, harmonic engine, Resyn settings, etc.).
- **Morph is now an audio crossfade** between the two engines' outputs. At morph=0 you hear A only; at morph=1 you hear B only; at morph=0.5 they're summed 50/50. This is the existing Scene Crossfade behavior promoted from opt-in to always-on.
- **Modulators (LFOs, Random, Macros) replace per-knob arc morph.** Drag a modulator handle onto any knob to wire it. The knob shows a colored ring representing the modulator's depth swing. Multiple modulators per knob render as overlapping arcs in their source colors.
- **Modulators are scoped per engine.** Engine A has its own LFOs, Random, and Macros; Engine B has its own. Position in the bottom modulation row makes the scope obvious.

The previous A/B compare feature (snapshot two states in one engine and toggle) is retired — A and B are now the engines themselves, and an `A | B | LINK` tab at the top of the editor selects which engine's parameters you're editing.

The previous arc-morph capture mode and per-knob morph-arc-handle UI are removed entirely. Custom LFO shapes (point editor with curve segments) replace arc-style "interpolate this knob from A to B" gestures with a more general modulation language.

---

## Goals

- **Make modulation the centerpiece of the plugin's expressive surface.** Today the plugin is parameter-rich but static; this gives every knob the ability to breathe.
- **Match Pigments/Vital ergonomics.** Drag-to-assign, color-coded rings, drawer editor, custom LFO shapes — none of this should feel novel to users coming from those tools.
- **Preserve the existing PhantomEngine DSP unchanged.** The audio path stays identical for one engine; we just instantiate it twice and crossfade.
- **Phased shippable delivery.** Each PR leaves the plugin functional and improves on the prior state.

## Non-goals (v1)

- **Cross-engine modulator routing.** A's modulators do not target B's params and vice versa. This was deliberately excluded — the per-engine scope is part of the mental model.
- **Modulator-modulating-modulator (mod matrix recursion).** A macro can target params; it cannot target an LFO's rate. Defer to v2.
- **Polyphonic / per-voice modulators.** Modulators are global. The plugin is not a synth.
- **Perlin random.** S&H + Smooth ship in v1; Perlin deferred.
- **More than one Morph "lane."** v2 may add additional crossfades or routing curves; v1 is one morph knob, one crossfade.
- **Modulator type glyphs in slot dots.** Color identity is enough for v1. Revisit if user testing flags accessibility issues.
- **MIDI-triggered morph positions / `.morphpack` portable arc presets.** Already deferred items from morph v1; this revamp doesn't change that.

---

## Architecture

### Approach: Mirrored per-engine modulation

Two `PhantomEngine` instances each paired with an independent `ModulationEngine`. A separate `MorphCrossfader` mixes their audio outputs.

```
PhantomProcessor
├── apvts (single AudioProcessorValueTreeState; params prefixed by scope)
├── presetManager
├── engineFocus  (A | B | LINK — which tab is active in editor)
│
├── engineA : PhantomEngine
├── modEngineA : ModulationEngine
│      └── owns: LFO 1, LFO 2, Random A, Macro 1, Macro 2
│      └── per-block: applies modulated values to engineA's param state
│
├── engineB : PhantomEngine
├── modEngineB : ModulationEngine
│      └── owns: LFO 3, LFO 4, Random B, Macro 3, Macro 4
│      └── per-block: applies modulated values to engineB's param state
│
└── crossfader : MorphCrossfader
       └── reads morph_amount, mixes engineA + engineB output buffers
```

### Why mirrored, not unified

A single `ModulationEngine` holding all 10 modulators with a per-modulator `scope: A|B` field was considered and rejected:

- The per-engine invariant ("Engine A modulators only target Engine A params") is structurally enforced when the engines are separate. With a unified engine, it becomes runtime data that must be validated.
- Per-engine state save/restore is trivial when each engine has its own modulation container.
- Modulator code (LFO/Random/Macro classes) is shared — only the *instance state* duplicates, which is lightweight (a few floats + small ValueTree per modulator).
- Mirrors the user's mental model from the UI (left of morph = A; right of morph = B).

### File layout

```
Source/
  Modulation/
    Modulator.h               — abstract base (process, getOutput, getValueTree, fromValueTree)
    LFO.h/cpp                 — LFO modulator (canned + custom shapes)
    Random.h/cpp              — Random modulator (S&H + Smooth)
    Macro.h/cpp               — Macro modulator (user-controllable knob + destinations)
    CustomShape.h/cpp         — point + segment data + sample-time interpolation
    ModulationEngine.h/cpp    — per-engine container; per-block apply
    Routing.h/cpp             — Routing record (sourceID, paramID, depth, polarity)
  DualEngineHost.h/cpp        — owns 2x PhantomEngine + 2x ModulationEngine
  MorphCrossfader.h/cpp       — audio-level A/B mix (linear / equal-power / S-curve)
  PhantomProcessor.h/cpp      — heavily revised to host DualEngineHost
  PluginEditor.h/cpp          — heavily revised for tabs + new modulation panel
  WebUI/
    modulation-panel.js       — bottom row (11 slots) + drawer state machine
    lfo-editor.js             — drawer content for LFO (waveform preview + shape pills + sliders)
    custom-shape-editor.js    — interactive point editor for LFO custom shapes
    random-editor.js          — drawer content for Random
    macro-editor.js           — drawer content for Macro (big knob + destinations list)
    morph-editor.js           — drawer content for Morph (crossfade curve, levels, B-bypass)
    routing.js                — drag-to-assign protocol (mousedown → highlight → drop)
    knob.js                   — UPDATED: Pigments-style bipolar ring + source tabs + tooltip
    preset-system.js          — UPDATED: migration of legacy formats
```

Files removed:
- `Source/MorphEngine.h/cpp` — split between `MorphCrossfader` (audio crossfade) and `ModulationEngine` (parameter modulation logic generalized).
- `Source/ABSlotManager.h/cpp` — A/B compare retired.
- `Source/WebUI/morph.js` — replaced by `modulation-panel.js`.

Files unchanged:
- `Source/Engines/*` — `PhantomEngine` and its DSP children carry over verbatim. They're now instantiated twice.
- `Source/PresetManager.h/cpp` — extended with the new format and a legacy-load adapter, but core flow (`<Metadata>` child, favorites index, pack scan) is intact.

### APVTS layout

One APVTS per `PhantomProcessor`. Param IDs prefixed by scope:

- **Engine A params:** `a_cutoff`, `a_drive`, `a_resyn_mix`, etc. — every existing param is duplicated for engine B.
- **Engine B params:** `b_cutoff`, `b_drive`, `b_resyn_mix`, etc.
- **Morph:** `morph_amount` (0–1, automatable), `morph_curve` (linear / eq-power / s-curve), `morph_a_level_db`, `morph_b_level_db`, `morph_bypass_idle_engine` (bool — see *Per-block flow* for semantics).
- **Modulators (settings, not assignments):** `mod_lfo1_rate`, `mod_lfo1_shape`, `mod_lfo1_phase`, `mod_lfo1_smooth`, `mod_lfo1_sync`, `mod_lfo1_retrig`, etc. for LFOs 1–4. `mod_rand1_rate`, `mod_rand1_smooth`, `mod_rand1_mode`, `mod_rand1_bipolar` for the two Random sources. Macro values: `macro1`, `macro2`, `macro3`, `macro4` (the user-facing knobs — automatable so DAWs can write to them).
- **LFO custom shape data + modulator routings** are NOT in APVTS. They're stored on a separate `<ModulationConfig>` ValueTree child, serialized into the plugin state and into presets. Routings are dynamic per-preset; APVTS is for fixed-cardinality params only.

### WebView slider routing

When the editor's active tab is **A**, every `<phantom-knob data-param="cutoff">` binds its slider relay to `a_cutoff`. When the tab is **B**, the same component rebinds to `b_cutoff`. **LINK** mode binds both — slider sets both `a_cutoff` and `b_cutoff` simultaneously.

The implementation: `data-param="cutoff"` is the *logical* param name. A small adapter layer in `juce-frontend.js` (or a wrapper around `getSliderState`) maps logical name + active tab to the real APVTS param ID.

### Per-block flow

Per audio block, ordered:

1. **Modulators tick.** Each modulator advances its phase/state and emits a current output value (−1..+1 for bipolar, 0..1 for unipolar).
2. **Routings apply.** For each modulator routing `{source, param, depth}`, the modulator's output is multiplied by depth and added to the param's user-set value, clamped to the param's range. Result is written to the audio engine's internal cached param value (NOT to APVTS — that stays user-visible).
3. **Audio engines process.** Engine A processes its input buffer with its modulated params. Engine B processes the same input with its modulated params.
4. **Crossfader mixes.** Reads `morph_amount`, applies the crossfade curve, weighted-sums the two engine output buffers into the main output.

**Idle-engine bypass.** If `morph_bypass_idle_engine` is on, the engine producing zero output is skipped:
- At `morph_amount == 0` exactly (within float epsilon), Engine B's `process()` is skipped.
- At `morph_amount == 1` exactly, Engine A's `process()` is skipped.
- The check runs at block start. If `morph_amount` crosses the boundary mid-block (smoothed value at block start is 0 but at block end is non-zero, or vice versa), both engines run for that block — avoids a click on re-engage.
- This is the CPU-saver mitigation: typical use (parked at A, occasional automation to B) stays close to single-engine cost.

---

## UI design

### Top of the editor: engine tabs

A small `A | B | LINK` toggle in the header strip. On editor open, **A** is the active tab and **LINK** is off.

- Click `A` → all knobs reflect/edit Engine A's state. Recipe wheel, harmonics, Resyn settings, etc., all show A.
- Click `B` → same UI, edits Engine B.
- Click `LINK` → toggles LINK on or off without changing which tab is active. While LINK is on, every knob edit on the active tab writes to *both* engines' equivalent params simultaneously. If A and B currently have different values when LINK is engaged, no values change at engage time — the next slider edit is the one that re-converges them. A small "diverged" indicator appears next to any knob whose A/B values disagree while LINK is on, fading after the first link-mode write to that knob.

LINK only locks **knob values**, not modulator routings. An LFO assigned to `a_cutoff` does not get auto-mirrored to `b_cutoff` when LINK is on.

### Bottom of the editor: modulation panel

Symmetric 11-slot row, always visible. Layout left-to-right:

```
LFO 1   LFO 2   RANDOM   MACRO 1   MACRO 2   MORPH   MACRO 3   MACRO 4   RANDOM   LFO 3   LFO 4
└──────────── ENGINE A ────────────┘    [center]   └──────────── ENGINE B ────────────┘
```

Engine A vs B distinguished by **position only** (left vs right of morph), not color. Thin neutral underline + "ENGINE A" / "ENGINE B" labels mark the side.

#### Slot appearance (collapsed state)

Each slot is a small panel: colored dot on top (the modulator's identity color), name beneath ("LFO 1", "MACRO 1", etc.). Morph's slot is larger and has phantom-white styling to mark it as the headline. Clicking a slot opens the editor drawer.

#### Drawer (expanded state)

Click a slot → a drawer slides up *above* the row, plugin window grows by ~140px. Row stays visible underneath with the active slot highlighted (border becomes the source color). Click another slot → drawer reshapes to that modulator's editor instantly. Click the active slot again or a collapse caret → drawer hides.

This keeps all 11 modulators visible while editing one — important for the drag-to-assign gesture (you can drag from any slot to any knob without closing the editor).

### Drawer content per type

- **LFO** — waveform preview area (with live cursor riding the curve) on the left; on the right: shape pills (Sine / Tri / Sqr / Saw / Rand / Custom), Rate slider (sync default-on, shows note divisions like 1/4 1/8 …), Phase, Smooth, Sync + Retrig toggles. Selecting **CUSTOM** turns the preview area into an interactive point editor (see *LFO custom shapes* below).
- **Random** — live output preview on the left (a stepped or smoothed graph showing the random walk); right: mode pills (S&H / Smooth), Rate, Smooth, Bipolar + Sync toggles.
- **Macro** — left: big macro knob + numeric value + editable name field ("MOTION", "SHAPE", whatever the user types). Right: destinations list — each routed param shown as a row with target name, depth slider (bipolar, centered at zero), and a remove button. Toggle: **EXPOSE TO HOST AUTOMATION** (default on; the macro is an APVTS param so DAWs can automate it).
- **Morph** — left: phantom-white morph knob + numeric. Right: crossfade curve picker (Linear / Eq-Power / S-Curve), per-side level trim (`A LEVEL`, `B LEVEL`), and the **BYPASS IDLE ENGINE** CPU-saver toggle (default on; bypasses Engine B when morph=0 exactly and Engine A when morph=1 exactly).

### LFO custom shapes

When **CUSTOM** is selected on an LFO, the preview area becomes interactive:

- **Anchored endpoints** at phase 0 and phase 1 (loop seam) — Y-draggable, X-fixed.
- **Click empty canvas** → adds a new point. Y interpolated from the existing curve at that X. Cap at 16 points.
- **Drag a point** → moves it. X constrained between adjacent neighbors (no reordering). Holding Shift snaps Y to common values (0, ±0.25, ±0.5, ±0.75, ±1).
- **Each segment between two adjacent points has a curve type:** Linear / Exp / Log / S-curve / Hold. Selected via a small pill row in the side panel when a point is selected, OR by dragging the segment's mid-handle (a small translucent dot at the midpoint of the segment) — the handle position auto-snaps to the nearest canonical curve type.
- **Right-click a point** → context menu: Delete, Set segment-after curve, Copy phase/value.
- **Side panel reflects the selected point**: numeric X (phase) + Y (value) for precise entry; pill row for segment-after curve type.
- **Toolbar**: Undo, Clear, **Preset shapes** dropdown (gallop, swell, dip-rebound, plucky envelope, etc.) — ships with a handful of useful starting points.

### Knob rendering — Pigments-style ring

Every continuous-param knob renders modulation overlays:

- **Single outer ring** at one radius (no concentric rings).
- **Bipolar arc anchored at the pointer.** Each routed source paints an arc swinging from the current pointer position outward. Positive depth swings clockwise; negative counter-clockwise. Length proportional to depth.
- **Source tabs above the knob.** Tiny colored dots, one per assigned source, in the source's color. At a glance: "this knob has 3 modulators." Click a tab → matrix popover (see below).
- **Overlap is allowed.** Multiple arcs paint at slight transparency on the same ring; source tabs disambiguate.
- **Drag arc end** → sets that source's depth directly (per-source drag handle, color-matched).
- **Hover tooltip** on the knob body → list of routed sources with depths (e.g., `LFO 1 +50% · MACRO 1 −20%`). Read-only at-a-glance.

### Matrix popover

Click a source tab on a knob → small popup over the knob showing:
- One row per routed source: colored dot + name + depth slider (bipolar) + numeric depth + remove button.
- Click outside or press Escape to close.

This gives precision editing without leaving the main editor view. Equivalent to Pigments' modulation-routing context menu.

### Drag-to-assign

- **Mousedown on a slot's center dot** initiates drag. The dot becomes a draggable colored ghost following the cursor.
- **During drag**, eligible knobs (continuous params on the active engine tab) glow with the source's color. Discrete params do not glow. LINK mode does not affect drag-to-assign — even when LINK is on, the routing is created on the active tab's engine only. (LINK locks knob values, not modulator routings; this was a deliberate decision.)
- **Drop on an eligible knob** → assignment created at **+50% default depth**. Arc appears on the knob's ring immediately. Drawer (if open) updates its routing list.
- **Drop elsewhere** → cancels the drag with a brief shrink animation back to the slot.
- **LINK mode + drop** → routing applies to active tab's engine only. LINK affects knob *values*, not modulator routings.

### Hover tooltip

Hovering any continuous-param knob (when modulators are routed to it) shows a small tooltip listing routed sources and their depths. Disappears on mouseleave. Read-only. Replaces the at-a-glance information that the matrix popover provides on click.

### Color palette

| Type   | Color           | Hex       |
|--------|-----------------|-----------|
| LFO    | Steel blue      | `#4A90E2` |
| Macro  | Cyan/teal       | `#5DD3E0` |
| Random | Indigo/violet   | `#6B5DC9` |
| Morph  | Phantom white   | `#EFEFF2` |

All cool family. Phantom white morph reads as the headline. No engine-A/B tinting on modulators — engine scope is communicated by position.

---

## Data model

### Plugin state (saved with `getStateInformation`)

The plugin state ValueTree gains a new top-level child:

```xml
<PluginState>
  <APVTSState>...</APVTSState>            <!-- existing -->
  <ModulationConfig>
    <Modulator id="lfo1" scope="A" type="LFO">
      <CustomShape>
        <Point x="0.00" y="0.00" curveAfter="linear" bend="0"/>
        <Point x="0.25" y="0.71" curveAfter="s-curve" bend="0"/>
        <Point x="0.50" y="0.00" curveAfter="exp" bend="0.4"/>
        <Point x="1.00" y="0.00" curveAfter="linear" bend="0"/>
      </CustomShape>
    </Modulator>
    <!-- ... LFO 2, Random A, Macro 1, etc. -->

    <Routings>
      <Route source="lfo1" param="a_cutoff" depth="0.50"/>
      <Route source="lfo1" param="a_drive"  depth="-0.22"/>
      <Route source="macro1" param="a_cutoff" depth="0.75"/>
    </Routings>
  </ModulationConfig>
</PluginState>
```

Notes:
- `CustomShape` only present for LFOs whose shape is set to "Custom".
- `Routings` is global across both ModulationEngines. Each `source` value implicitly identifies the engine via the slot's known scope. Each `param` is a fully-qualified APVTS param ID (so `a_cutoff` not just `cutoff`).
- Macro value (the user-controllable knob position) lives in APVTS as `macro1` etc., not here.

### Preset format extension

`.fxp` presets gain `<ModulationConfig>` as a child of the existing state ValueTree. Preset save:

```xml
<state>
  <APVTSState>...</APVTSState>             <!-- existing, now with a_*/b_* params -->
  <Metadata name="..." designer="..." .../>  <!-- existing -->
  <ModulationConfig>...</ModulationConfig>   <!-- new -->
</state>
```

The existing `<SlotB>` (for A/B compare's snapshot) is removed from the new format. The existing `<MorphConfig>` (for arc-morph) is removed.

### Migration

`PresetManager` and `getStateInformation`/`setStateInformation` both run a legacy adapter on load. Logic:

1. **State has `a_*` / `b_*` params (new format):** load directly.
2. **State has only un-prefixed params + optional `<SlotB>` + optional `<MorphConfig>` (legacy):**
   - Promote un-prefixed params to `a_*` (Engine A gets the loaded state).
   - If `<SlotB>` present, populate `b_*` from it. Otherwise, mirror Engine A's values to Engine B (so morph at 0 or 1 sounds identical until the user diverges them).
   - Drop `<MorphConfig>` arc data — no equivalent in the new model. If desired, log a warning to the console once.
   - `<ModulationConfig>` is empty (no modulator routings inherited).
3. **Preset is from a future version:** load best-effort; ignore unknown nodes.

The user's existing presets continue to load and produce sound. The Pro morph arcs they painstakingly captured are dropped — this is the conscious tradeoff for the new model.

### LINK mode and APVTS

A separate plugin-level (non-APVTS) flag tracks the `engineFocus` state (A | B | LINK). It's persistent across editor opens (saved to plugin state, not preset state, since it's a UI/edit-mode preference). When LINK is on, every `setNormalisedValue` call from a knob targets both `a_*` and `b_*` versions of the param.

---

## Phased delivery — six PRs

### PR 1 — Dual-engine baseline

- Promote Scene Crossfade to always-on. Two `PhantomEngine` instances run continuously, mixed by `morph_amount`.
- Retire `ABSlotManager` and the existing `MorphEngine` (delete both files; remove from the build).
- Migrate APVTS to `a_*` / `b_*` prefixes. WebView bridge (juce-frontend.js + relevant bindings) updated to dispatch through a logical-name layer that routes by current engine focus (default A; tab UI not yet present).
- Preset migration adapter shipped (legacy presets load with engine A populated; engine B mirrors A).
- A/B compare UI elements (header buttons, save-modal options) removed.
- **Result:** plugin runs end-to-end. Morph behavior is now audio-crossfade only. No tab UI yet — engine A is the fixed editing target for this PR. No modulators yet either; the bottom panel still shows the legacy morph-only row, with the morph slider now actually crossfading audio. Existing PhantomEngine DSP is untouched.

### PR 2 — A | B | LINK tab UI

- Add the `A | B | LINK` tab toggle to the header.
- `engineFocus` state machine + plugin-state persistence.
- WebView slider routing tied to `engineFocus`. LINK applies to knob value writes.
- Verify LINK does *not* mirror modulator routings (when modulators land in PR 3).
- **Result:** users can edit A's state and B's state independently, link them when desired. Two complete engine sound configurations editable.

### PR 3 — Macros

- Introduce `Modulation/Modulator.h`, `Modulation/Macro.h/cpp`, `Modulation/ModulationEngine.h/cpp`, `Modulation/Routing.h/cpp`.
- 4 macros (2 per engine). Each is an APVTS-exposed param (`macro1`–`macro4`) so DAWs can automate them.
- `ModulationConfig` ValueTree introduced. Routings stored there.
- New WebUI: `modulation-panel.js` (bottom row replaces existing morph-only strip), `macro-editor.js`, `routing.js` (drag-to-assign).
- Updated `knob.js`: Pigments-style bipolar ring, source tabs, hover tooltip, matrix popover.
- Updated `preset-system.js`: serializes/deserializes `<ModulationConfig>`.
- **Result:** users can drag macros onto knobs, set depth, see colored rings, hear the modulation. The drawer state machine (collapsed row + click-to-expand drawer) is established here as the pattern for subsequent PRs.

### PR 4 — LFOs

- Add `Modulation/LFO.h/cpp` with the canned shapes (Sine, Tri, Sqr, Saw, Rand).
- 4 LFOs (2 per engine). Settings exposed in APVTS (`mod_lfo1_rate`, `mod_lfo1_shape`, etc.).
- WebUI: `lfo-editor.js` (drawer content). Waveform preview with live cursor.
- LFO 1/2 slots + LFO 3/4 slots populated in the bottom row.
- **Result:** users can wire LFOs to knobs same as macros. Tempo-sync default on. Custom shape not yet available.

### PR 5 — Random sources

- Add `Modulation/Random.h/cpp` with S&H and Smooth modes.
- 2 Random sources (1 per engine). Settings exposed in APVTS.
- WebUI: `random-editor.js`. Live output preview.
- Random slots populated in the bottom row.
- **Result:** Random modulators usable. Perlin mode deferred to v2.

### PR 6 — Custom LFO shapes

- Add `Modulation/CustomShape.h/cpp` (point + segment data; sample-time interpolation).
- LFO `shape="custom"` reads from per-LFO `<CustomShape>` ValueTree.
- WebUI: `custom-shape-editor.js` — interactive point editor, segment curve picker, preset shapes dropdown.
- Storage round-trip in `<ModulationConfig>` already established in PR 3 — only the editor UI is new.
- **Result:** complete v1 modulation system shipped.

Each PR is a working plugin. The user can stop at any phase and have a strictly better product than the prior phase delivered.

---

## Testing

### Unit tests (Catch2)

- **`MorphCrossfaderTests`** — equal-power, linear, S-curve crossfades produce expected gains at canonical positions (0, 0.25, 0.5, 0.75, 1). Bypass-at-zero short-circuit verified.
- **`ModulationEngineTests`** — modulator add/remove, routing add/remove/depth-update, per-block apply produces expected modulated values for a fixture engine. Per-engine scope invariant: routings to other-engine params are rejected.
- **`LFOTests`** — each canned shape produces expected values at canonical phases. Sync mode rate quantization. Retrig resets phase.
- **`CustomShapeTests`** — point insert, drag (X-bounds enforcement), segment curve types produce expected interpolations between adjacent points. Loop seam handling.
- **`RandomTests`** — S&H produces stepped output at the rate; Smooth interpolates; Bipolar produces values in [-1, 1] and unipolar in [0, 1]. Sync.
- **`MacroTests`** — destinations apply with depth, sum correctly with other modulators on same param, host automation triggers re-application.
- **`PresetMigrationTests`** — legacy preset (with `<SlotB>` and `<MorphConfig>`) loads, engine A has the un-prefixed state, engine B has SlotB or mirrors A, no `<MorphConfig>` artifact remains.

### Manual test checklist (per PR)

Each PR ships with its own manual test checklist covering the new UI surface in isolation + integration with prior phases. PR 6's checklist is the full system check.

---

## Risks and open considerations

### CPU baseline

Two PhantomEngines doubling DSP cost is the principal risk. ~10% idle CPU today × 2 = ~20%. The `morph_b_bypass_at_zero` toggle (default on) keeps typical use near current cost when morph is parked at A or B. Worst case (mid-morph, both engines fully running) is ~20% idle, which is still well within budget for a serious plugin in a project context.

If users in beta report CPU concerns, the deferred quick-wins from morph v1 can be picked up: Hann window cache, log-bin precompute, FFT silence early-out — collectively 1–3% per engine, ~2–6% saved doubled.

### WebView slider rebinding

The logical-name → APVTS-param mapping based on `engineFocus` is the most invasive single change to the WebView bridge in PR 1/2. Risk: bridge calls become stateful in a way they aren't today (current calls are direct param IDs). Mitigation: thin adapter layer; the rest of the JS stays unaware. Adapter has its own unit tests and a documented rule (`data-param="cutoff"` is logical; APVTS param ID is derived).

### Drag-to-assign across overlapping UI

The drag-from-slot-to-knob gesture crosses the boundary between the bottom panel and the main editor. WebView2 pointer events propagate but the plugin uses a mix of canvas knobs (recipe wheel) and DOM knobs (`<phantom-knob>`). The drag protocol must hit-test both. PR 3's routing.js needs to handle:
- DOM `<phantom-knob>` elements: pointerover events with `data-param` lookup.
- Canvas knobs (recipe wheel H2-H8): convert pointer position to wheel slot via existing recipe-wheel.js hit-test, then derive logical param.

This is a known-tractable problem (Vital does the same), but it's the most novel part of the WebUI work and warrants extra manual QA.

### LINK mode value divergence

When the user activates LINK while engines have different values, the implementation does nothing at engage time — no snapping. The next slider edit on the active tab writes to both engines, re-converging that one knob. A small "diverged" indicator next to any knob whose A/B values disagree while LINK is on tells the user which knobs still need to be touched to fully sync. The indicator fades after the first link-mode write. This matches the Augmented Strings approach.

### Custom shape performance

Sampling a custom shape per audio block at audio rate would be expensive. The shape is sampled at LFO rate (Hz or note divisions), which is much slower — typically 0.1–20 Hz. We compute one shape value per block (or per several blocks at very low LFO rates) and smooth it. No issue.

### Modulator output smoothing

Without smoothing, an LFO at low rate causes audible zipper noise on a modulated cutoff. The existing per-block smoothing in MorphEngine (`smoothingAlpha` based on sample rate) carries over to ModulationEngine. Per-modulator-per-target smoothed output, applied at audio rate.

---

## What is *not* changing

- `PhantomEngine` and all its DSP children (`BassExtractor`, `Waveshaper`, `EnvelopeFollower`, `WaveletSynth`, `BinauralStage`, `StereoWidener`) — unchanged.
- The recipe wheel, harmonic engine, and Resyn UI sections — unchanged.
- `PresetManager` core (pack scan, favorites, cover art, metadata in `<Metadata>` child) — unchanged. The format extension is additive; the migration adapter is the only new logic.
- The keyboard pass-through stack (the four-layer fix) — unchanged.
- The neumorphic theme, the `.wheel-mount` knob aesthetic, the focus-rescan timer — all preserved.

---

## Open items deferred to v2+

- Cross-engine modulator routing (A's LFO targets B's params)
- Modulator-modulating-modulator (mod matrix recursion)
- Multiple morph lanes / curves
- Polyphonic / per-voice modulators
- Perlin random
- MIDI-triggered morph positions
- `.morphpack` portable arc presets (now `.modpack` portable modulator preset, since arcs are gone)
- Type glyphs in slot dots (accessibility)
- Preset preview waveform on the modulation panel ("hear the LFO before assigning")
