# Kaigen Phantom Pro — Morph Module Design Specification

**Date:** 2026-04-23
**Status:** Design Phase
**Scope:** Pro SKU v1 — adds two morph-related features (arc-based parameter modulation + optional dual-engine Scene Crossfade) on top of the Standard-build A/B compare system shipped 2026-04-23 (PR #12, commit `ec969dc`).

---

## Overview

The Pro SKU of Kaigen Phantom adds a **modulation system for sound morphing**: two distinct tools that serve different use cases, bundled in a single paid build.

- **Arc Morph** (primary Pro feature) — a single morph knob plus per-parameter modulation depth widgets ("arcs") shown as colored rings around each continuous knob. Users declare *which* knobs move and *by how much* when morph sweeps 0→1. No external plugin workaround can replicate this.
- **Scene Crossfade** (opt-in Pro mode) — a dual-engine mode that crossfades the full audio output between A and B slots, including structural/discrete parameters that arcs can't touch (Mode, Ghost Mode, Binaural Mode). Uses the A/B slot storage already shipped in Standard.

Pro ships as a separate binary gated by compile-time flag `KAIGEN_PRO_BUILD`. Standard builds contain no Pro-only code paths, no upsell UI, and no locked-feature toggles.

### Visual language reference

UI patterns draw from Arturia Pigments: colored rings around knobs showing modulation depth, a dedicated dark "modulation panel" beneath the main header, live animated fill as modulation sweeps.

### Not in scope for this spec

- Runtime license unlocks (compile-flag gating only for v1)
- Multiple morph lanes (architected-for, shipping as v2)
- Internal modulation sources — LFO, envelope, step sequencer (v2+)
- MIDI-triggered morph position targets (v2+)
- User-drawn curves; per-parameter curve shaping
- Pro preset marketplace

Roadmap items that interact with this spec's architecture are noted inline as "v2 waypoint" so nothing gets foreclosed.

---

## Product Tiering

| Capability                                              | Standard | Pro |
|---------------------------------------------------------|:--------:|:---:|
| Full DSP chain (Effect + Resyn modes)                   | ✓        | ✓   |
| A/B slot storage + binary snap toggle                   | ✓        | ✓   |
| Preset browser + A/B preset format                      | ✓        | ✓   |
| Arc Morph — per-knob modulation depth widgets           | —        | ✓   |
| Arc Morph — morph amount automation parameter           | —        | ✓   |
| Arc Morph — capture-mode gesture                        | —        | ✓   |
| Scene Crossfade — dual-engine A↔B continuous crossfade  | —        | ✓   |
| Morph-extended preset format (load)                     | graceful | ✓   |
| Morph-extended preset format (save)                     | —        | ✓   |

**Graceful degradation:** Standard builds loading a Pro-authored preset ignore morph-specific data (`<ArcLane>`, `<SceneCrossfade>`, morph position) and restore the base patch + A/B slots as normal. Standard never writes morph data on save.

---

## Architecture

### New class: `MorphEngine` (Pro build only)

Owned by `PhantomProcessor` under `#ifdef KAIGEN_PRO_BUILD`. Responsibilities:

1. Store bipolar arc depths per parameter for one lane (v1; multi-lane structure in place for v2).
2. Hold the current morph amount (driven by an APVTS parameter).
3. Per audio block, compute live parameter values as `clamp(base + arc * morph, range.min, range.max)` and push to the engine's APVTS targets.
4. Manage Scene Crossfade opt-in state, including lazy instantiation of a second `PhantomEngine` when enabled AND slots differ meaningfully.
5. Serialize/deserialize arc data and Scene Crossfade state into/from the preset `<MorphConfig>` XML node.

```cpp
// Source/MorphEngine.h (Pro build only)
#pragma once
#if KAIGEN_PRO_BUILD
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
#include <unordered_map>

namespace kaigen::phantom
{

class ABSlotManager;
class PhantomEngine;

class MorphEngine : private juce::AudioProcessorValueTreeState::Listener
{
public:
    MorphEngine(juce::AudioProcessorValueTreeState& apvts,
                ABSlotManager& abSlots,
                PhantomEngine& primaryEngine);
    ~MorphEngine() override;

    // Called per-block from PhantomProcessor::processBlock BEFORE primary engine processing
    // to push interpolated parameter values; called AGAIN AFTER to handle Scene Crossfade
    // audio-level mixing if active.
    void preProcessBlock();
    void postProcessBlock(juce::AudioBuffer<float>& mainBuffer,
                          const juce::AudioBuffer<float>* sidechain);

    // Arc access (single lane in v1)
    void setArcDepth(const juce::String& paramID, float normalizedDepth);  // depth in [-1, +1]
    float getArcDepth(const juce::String& paramID) const;
    bool hasNonZeroArc(const juce::String& paramID) const;
    int armedKnobCount() const;                           // for UI status display

    // Morph amount — mirrors the APVTS MORPH_AMOUNT parameter but smoothed internally.
    float getMorphAmount() const { return smoothedMorph; }

    // Morph enable toggle
    bool isEnabled() const { return enabled; }
    void setEnabled(bool on);

    // Capture mode
    void beginCapture();                                  // freezes morph at 1.0; user drags knobs
    juce::Array<juce::String> endCapture(bool commit);   // returns the paramIDs whose arcs were modified

    // Scene Crossfade (opt-in)
    bool isSceneCrossfadeEnabled() const { return sceneEnabled; }
    void setSceneCrossfadeEnabled(bool on);
    float getScenePosition() const { return smoothedScenePos; }
    void setScenePosition(float pos);  // 0..1; driven by APVTS SCENE_POSITION

    // Preset I/O
    juce::ValueTree toMorphConfigTree() const;
    void fromMorphConfigTree(const juce::ValueTree& morphConfigNode);

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    struct ArcEntry { float depth = 0.0f; };  // per-parameter arc data for one lane

    juce::AudioProcessorValueTreeState& apvts;
    ABSlotManager& abSlots;
    PhantomEngine& primaryEngine;

    std::unordered_map<juce::String, ArcEntry> lane1Arcs;  // v1: one lane; future: std::vector<std::unordered_map<...>>
    juce::String curveName = "linear";                     // global per-lane curve in v1

    bool  enabled = false;
    float rawMorph = 0.0f;
    float smoothedMorph = 0.0f;

    // Capture mode state
    bool  inCapture = false;
    std::unordered_map<juce::String, float> captureBaseline;  // param values at beginCapture time

    // Scene Crossfade state
    bool  sceneEnabled = false;
    float rawScenePos = 0.0f;
    float smoothedScenePos = 0.0f;
    std::unique_ptr<PhantomEngine> secondaryEngine;   // lazy; constructed on first enable

    // Smoothing — single-pole IIR, ~15 ms time constant
    static constexpr float kSmoothingMs = 15.0f;
    float smoothingAlpha = 0.0f;                      // computed in prepareToPlay
    bool suppressArcUpdates = false;                  // guard during internal writes

    juce::StringArray getAllContinuousParamIDs() const;
};

} // namespace
#endif  // KAIGEN_PRO_BUILD
```

### PhantomProcessor integration

```cpp
// Source/PluginProcessor.h additions
class PhantomProcessor : public juce::AudioProcessor
{
    // ... existing members ...
    kaigen::phantom::ABSlotManager abSlots { apvts };

  #ifdef KAIGEN_PRO_BUILD
    // NOTE: MUST be declared AFTER apvts, abSlots, and engine. MorphEngine
    // subscribes to the morph APVTS parameters + reads slot data + drives
    // the primary engine, so all three must be initialized first.
    kaigen::phantom::MorphEngine morph { apvts, abSlots, engine };

    kaigen::phantom::MorphEngine& getMorphEngine() { return morph; }
  #endif
};
```

In `processBlock`, under `#ifdef KAIGEN_PRO_BUILD`:

1. Call `morph.preProcessBlock()` before the existing engine processing. This applies arc-based parameter interpolation for the upcoming block.
2. Run the existing engine.
3. If Scene Crossfade is active, `morph.postProcessBlock(buffer, sidechain)` runs the secondary engine on the same input and mixes its output into the main buffer by `smoothedScenePos`.

The single-engine arc path has essentially zero CPU overhead beyond the interpolation math. Scene Crossfade adds a second engine pass (~10% additional CPU) only when enabled AND the two slots differ meaningfully.

### New APVTS parameters (Pro build only)

Added to `Parameters.h` under `#ifdef KAIGEN_PRO_BUILD`:

```cpp
namespace ParamID {
    inline constexpr auto MORPH_ENABLED    = "morph_enabled";    // bool
    inline constexpr auto MORPH_AMOUNT     = "morph_amount";     // float 0..1
    inline constexpr auto SCENE_ENABLED    = "scene_enabled";    // bool
    inline constexpr auto SCENE_POSITION   = "scene_position";   // float 0..1
}
```

`MORPH_AMOUNT` and `SCENE_POSITION` are **automatable**. `MORPH_ENABLED` and `SCENE_ENABLED` are UI toggles (non-automatable to avoid DAW-level state confusion).

Arc depths themselves are NOT individual APVTS parameters — they live in `MorphEngine`'s internal storage and serialize via `<MorphConfig>`. Exposing 39 individual arc params would pollute the DAW's parameter-automation list.

---

## Data Model

### In-memory arc storage

Per parameter, per lane (v1: one lane):

```
std::unordered_map<juce::String, ArcEntry> lane1Arcs;
// key: APVTS param ID (e.g., "recipe_h2", "phantom_threshold")
// value: { float depth; }  -- depth ∈ [-1.0, +1.0]
```

Only parameters with non-zero depth are present in the map. Querying a paramID not in the map returns `depth = 0.0`.

### Preset file format extension

The A/B compare spec introduced `<MorphConfig>` as a self-closing tag with two attributes (`defaultPosition`, `curve`). This spec extends it to a container:

```xml
<KaigenPhantomState>
  <PARAM id="..." value="..." />   <!-- slot A / root state, unchanged -->
  ...
  <Metadata>
    <PROP name="presetKind" value="ab_morph"/>
    ...
  </Metadata>
  <SlotB>                          <!-- optional, same as A/B spec -->
    <KaigenPhantomState>...</KaigenPhantomState>
  </SlotB>
  <MorphConfig defaultPosition="0.0" curve="linear">     <!-- EXTENDED -->
    <ArcLane id="1">
      <Arc paramID="recipe_h2"          depth="0.35"/>
      <Arc paramID="recipe_h4"          depth="-0.20"/>
      <Arc paramID="phantom_threshold"  depth="0.50"/>
      <!-- only non-zero arcs are listed -->
    </ArcLane>
    <SceneCrossfade enabled="1" position="0.0"/>         <!-- optional -->
  </MorphConfig>
</KaigenPhantomState>
```

**Format rules:**

- `<MorphConfig>` attributes (`defaultPosition`, `curve`) remain at the parent level, governing the whole morph system.
- `<ArcLane id="N">` — each lane gets its own wrapper. V1 writes only `id="1"`; v2 can add more lanes without format change.
- `<Arc paramID depth>` — one entry per armed parameter, per lane. `depth` is signed normalized [-1, +1]. Unlisted params = depth 0.
- `<SceneCrossfade>` — sibling, independent of arc lanes. Attributes: `enabled` (0/1) and `position` (0..1). Absence = disabled, position 0.
- `<MorphConfig>` and its children are optional on any preset. Single/A-B presets without morph data continue to work.

**Standard build behavior:**

- Reads `<MorphConfig>` but ignores everything except existing A/B-era attributes (`defaultPosition`, `curve`). `<ArcLane>` and `<SceneCrossfade>` silently skipped.
- Never writes `<MorphConfig>` with child content. Save flow emits the self-closing attribute-only form when it writes at all (which only happens for `presetKind="ab_morph"` saves — and Standard never writes that kind).

### Plugin state persistence

`PhantomProcessor::getStateInformation` / `setStateInformation`, under `#ifdef KAIGEN_PRO_BUILD`, additionally persist the morph system's live state alongside the existing `<ABSlots>` child. A new `<MorphState>` sibling:

```xml
<MorphState enabled="1" morphAmount="0.45" sceneEnabled="1" scenePosition="0.30">
  <ArcLane id="1">
    <Arc paramID="..." depth="..."/>
    ...
  </ArcLane>
</MorphState>
```

This is the *live* state (as distinct from preset metadata). Per-project: travels with the DAW session save/reload. Standard build peels this child off and discards before `replaceState` (same pattern as `<ABSlots>` handling).

---

## Arc Morph — Behavior

### Enable / disable

- `MORPH_ENABLED` APVTS bool parameter. Default: false.
- Toggled by user via the dark modulation panel's enable dot, and programmatically by preset load (if the loaded preset has non-zero `<ArcLane>` content, morph becomes enabled automatically).
- When disabled: arcs on knobs render invisible (track ring hidden); the morph amount slider greys out; `MorphEngine::preProcessBlock()` is a no-op regardless of stored arc data.
- When enabled: arc ring tracks render (faint when depth 0), morph slider becomes interactive.

### Arc depth setting — two gestures

**Direct drag** on the blue arc endpoint of any knob's ring:

- Pointer-down on the arc-tip handle begins a drag.
- Drag clockwise increases positive depth; drag counter-clockwise increases negative depth. Depth is mapped to angular movement relative to the knob's base position.
- The knob's base position is fixed during arc drag — only the arc segment grows.
- Pointer-up commits. The arc depth for that parameter is saved.

**Capture mode** for batch setup:

- User clicks a "CAPTURE" button in the modulation panel OR click-and-holds the morph slider.
- Internally, `MorphEngine::beginCapture()`:
  1. Stores a baseline snapshot of every continuous parameter's current APVTS value (this becomes the "base" that arcs will be measured against).
  2. Visually freezes the morph amount at 1.0 regardless of slider position.
  3. Enters capture-active UI state (panel highlights, knob ring track rings pulse subtly).
- User drags any knobs to the values they want "at morph = 1." Each knob movement updates live APVTS like a normal knob — but because the engine is receiving post-morph values that include `arc * 1.0`, the user's drag is visually interpreted as "set arc to difference from baseline."
- User clicks "COMMIT" (or releases morph slider hold).
- `MorphEngine::endCapture(true)`:
  1. For every parameter whose current value differs from its captured baseline: compute `arc = (currentValue - baseline) / paramRange`, store as normalized ±1 depth. Clip depth to ±1.
  2. Restore morph amount to its pre-capture slider position.
  3. Return the list of paramIDs whose arcs were modified (for UI feedback — "7 knobs armed").
- Cancel (user presses Esc or clicks "CANCEL"): `endCapture(false)` restores all parameters to their baselines and discards the captured arcs.

### Morph amount

- `MORPH_AMOUNT` APVTS parameter, range 0..1, default 0.0, **automatable**.
- DAW automation writes to the raw parameter.
- Internally, `MorphEngine` smooths via `smoothedMorph = lerp(smoothedMorph, rawMorph, smoothingAlpha)` per block. `smoothingAlpha` is derived from a 15 ms single-pole IIR time constant in `prepareToPlay`.
- Per block, the smoothed value drives all active arc applications.

### Per-block application

For each continuous parameter `p` with `arc != 0`:

```cpp
const float base       = capturedBase[p];      // stored when arc was set (not live value)
const float arcDepth   = lane1Arcs[p].depth;
const float targetRaw  = base + arcDepth * smoothedMorph * paramRange;
const float clamped    = juce::jlimit(range.start, range.end, targetRaw);
apvts.getParameter(p)->setValueNotifyingHost(
    apvts.getParameter(p)->convertTo0to1(clamped));
```

`capturedBase[p]` is the baseline captured when the arc was set — NOT the live value (which IS the computed morphed value). Without this, the base would drift as morph sweeps.

**Suppressing feedback:** each parameter set above would normally fire APVTS listeners, including `ABSlotManager::parameterChanged` which flips the active slot's modified flag, and `MorphEngine::parameterChanged` itself which we don't want retriggering on our own writes. The block is wrapped in `ScopedValueSetter<bool>` on both `abSlots.suppressModifiedUpdates` AND `morph.suppressArcUpdates`.

### Clamping and plateau behavior

- At runtime, parameters clip to their declared `NormalisableRange`.
- Visually: the knob ring only extends to the parameter's max-angle. If the user drags the arc past the ring's edge (i.e., `base + depth > max`), a small "plateau cap" marker renders at the max-angle position indicating "the modulation is asking for more than the knob can reach."
- As morph sweeps 0→1, the live pointer animates along the ring until it hits the clamp angle, then **visibly stops** while morph continues. Users see the plateau.

### Preset load with morph data

When loading a preset where `<MorphConfig>` contains `<ArcLane>` or `<SceneCrossfade>`:

1. Pro: `MorphEngine::fromMorphConfigTree(configNode)` restores arc map + scene state + morph position. `MORPH_ENABLED` flips true if any arc is non-zero OR `defaultPosition != 0`.
2. Standard: the node is silently ignored during `PresetManager::loadPresetInto`; the base patch + A/B slots load as normal.

### Capture baseline lifecycle

The arc's `base` value is captured at the moment the user sets the arc. This raises a question: what happens if the user later adjusts the parameter's base (the knob itself, with morph at 0)?

- Dragging the knob while morph amount = 0.0 is a normal edit. The live APVTS value becomes the new `capturedBase` for any arc on that parameter. Arcs persist by *relative* depth; changing the knob shifts the whole morph range by the same delta.
- Dragging the knob while morph amount ≠ 0.0 and the parameter has an arc is conceptually ambiguous (are we editing the base or the target?). v1 rule: treat as editing the live result. Set the new base to `currentLive - arcDepth * smoothedMorph * paramRange`. This keeps the arc depth intact while honoring the drag.
- This is arguably counterintuitive and may be revisited in v2. A simpler alternative: lock parameter dragging while morph amount > 0. Not doing that in v1 because capture-mode explicitly depends on being able to drag at morph = 1.

### designerAuthored flag (carried over from A/B)

`ABSlotManager` already has a `designerAuthored` flag that tracks whether we're inside a designer-authored preset. The morph engine respects this:

- Loading a preset with morph data sets designer-authored true (same as A/B preset load).
- Any user parameter edit clears designer-authored.
- Clearing is already wired via `parameterChanged` in A/B spec — morph doesn't change this behavior.

---

## Scene Crossfade — Behavior

### Opt-in model

- Defaults OFF. Users enable in Settings under a "Scene Crossfade (dual engine)" toggle.
- Rationale: doubles CPU when active. Users should deliberately opt in.
- When enabled: a second row appears in the modulation panel with a continuous A↔B slider and position display.

### Dual-engine model

- A second `PhantomEngine` instance (`secondaryEngine`) is lazily constructed on first `setSceneCrossfadeEnabled(true)`.
- Per block:
  - Primary engine processes slot A's parameter config (whatever the active slot is per the A/B system).
  - Secondary engine is kept in sync with slot B's parameter config — via `preProcessBlock` setting its param-tree from `abSlots.getSlot(Slot::B)`.
  - Both engines process the same input buffer.
  - Output is `(1 - smoothedScenePos) * primaryOutput + smoothedScenePos * secondaryOutput`.

### Parameter sync

At the start of each block, if Scene Crossfade is enabled:

1. Primary engine: its APVTS state already reflects what the user is editing (plus arc-morph deltas). No sync needed.
2. Secondary engine: copy `abSlots.getSlot(Slot::B)` parameter values into the secondary engine's parameter cache (NOT the APVTS — the secondary engine reads its own parameter mirror). Arc morph does NOT apply to the secondary engine's parameters — Scene Crossfade uses raw slot B state.

This means arc morph and Scene Crossfade are **independent layers**:

- Arc morph modulates slot A / primary engine only.
- Scene Crossfade blends the primary engine's output (with arc morph applied) against the secondary engine's output (pure slot B state).
- At scene position 0.0: pure primary (arc-morphed if morph engaged).
- At scene position 1.0: pure slot B (static).
- Mid-range: audio crossfade of the two.

### Shared state between engines

Both engines process the same input, but some internal DSP state would ideally be shared to avoid divergence:

- **Pitch tracker:** both engines detect fundamental from the same input. Running it twice is wasted work. Solution: primary engine runs pitch detection; secondary engine reads the primary's `engine.getEstimatedHz()` and uses it as its own tracker input. v1 implements this.
- **FFT spectrum input:** input is the same for both; only one FFT needs to run. Primary engine's spectrum remains the source of truth. Secondary engine does NOT compute spectrum. Output spectrum reflects the mixed output (primary-post-engine buffer, which IS the mixed result).
- **Wavelet capture (Resyn mode):** both engines run their own capture. This is acceptable for v1; shared capture is a performance optimization for v2 if CPU is tight.
- **Filter states (LR4, LPF, HPF):** each engine has its own. Acceptable drift; crossfade itself is the mitigation.
- **Envelope follower:** shared-input means similar but not identical results depending on filter state. Acceptable drift.

### Scene position automation

- `SCENE_POSITION` APVTS param, 0..1, automatable.
- Same 15 ms smoothing as morph amount, separate IIR.
- DAW can sweep it over time; users get smooth transitions between the two scenes.

### Interaction with A/B snap buttons

A/B snap buttons remain unchanged: they load slot A or slot B's complete state into live APVTS. Clicking them does NOT cross-fade; it snaps. Scene Crossfade is its own separate control.

If the user A/B snaps while Scene Crossfade is active:

- The click takes effect normally — live APVTS becomes the clicked slot's state.
- `smoothedScenePos` is NOT reset; the secondary engine continues to reflect slot B (unchanged because slot B wasn't the clicked target; if slot B WAS the clicked target, the secondary engine's cache refreshes next block).
- Audio may briefly double-up between primary and secondary engines during the crossfade. This is acceptable — it's a performance, not a session-save moment.

### CPU dynamics

- **Scene Crossfade OFF:** secondary engine does not exist; CPU baseline ~10%.
- **Scene Crossfade ON, slots identical:** secondary engine exists and runs, but produces identical output to primary. CPU ~20%. A future optimization could detect this and bypass secondary, but v1 runs both regardless of identity check for simplicity.
- **Scene Crossfade ON, slots differ:** CPU ~20%, output genuinely different between engines.

### Saving presets with Scene Crossfade

- `<SceneCrossfade enabled="1" position="0.0"/>` is written when the user saves a preset with Scene Crossfade enabled.
- Saved position defaults to 0.0 (user sweeps from scene A) unless the user explicitly chose a non-zero default by dragging the scene slider before save.
- Standard build ignores the tag on load.

---

## UI

### Knob modulation ring (all continuous knobs, Pro build only)

For every continuous knob on the plugin surface, when Pro is built:

**Rendering model — three layers around the knob:**

1. **Full-range track** — a thin 270° arc matching the knob's parameter range, stroke width ~3 px, color `rgba(0, 0, 0, 0.10)`. Rendered only when `MORPH_ENABLED` is true; hidden otherwise (so Standard-looking knobs when morph is off).
2. **Traveled portion indicator** — the portion of the track from the parameter's minimum to the current live value, slightly darker gray `rgba(0, 0, 0, 0.22)`. This is just a more-visible section of the same track; gives context for where the live value sits on the full range.
3. **Modulation segment (blue)** — colored arc from the `base` position to the `target` position (`base + arcDepth * paramRange`), stroke width ~3 px, color `#4A8DD5`. Split into two stroke sub-paths:
   - Bright `#4A8DD5` at full alpha from base to current live value (the "already consumed" portion as morph sweeps).
   - Dim `rgba(74, 141, 213, 0.35)` from current live to target (the "not yet reached" portion).
4. **Base tick** — small 3px circle at the base angle, color `rgba(0, 0, 0, 0.55)`. Always visible when `MORPH_ENABLED` and arc depth != 0.
5. **Drag handle** — small circle at the target-endpoint, color `#4A8DD5`, stroke `rgba(0, 0, 0, 0.3)`. User grabs this to adjust arc depth. Cursor: grab / grabbing.
6. **Live pointer** — the knob's existing pointer line, animated to reflect the live interpolated value. Same visual treatment as Standard.
7. **Ghost pointer** — faint 1.5px line at the base angle, color `rgba(0, 0, 0, 0.22)`. Reminds user where "home" is when live pointer has moved away.
8. **Plateau cap** — when the target math exceeds the knob's max, a small marker at the max-angle indicating "beyond here, value plateaus." Not rendered for normal arcs that fit within range.

Rings render via Canvas2D (matching existing knob rendering). All drawing happens on a per-frame animation loop already running (`recipe-wheel.js` / other canvas tick); morph rings are drawn in the same tick.

Performance: 39 continuous knobs rendering a ~270° arc each per frame at 30 fps is negligible CPU vs. the existing spectrum/oscilloscope work. No perf concerns anticipated.

### Modulation panel (P1 layout — always visible when morph enabled)

A dark horizontal strip inserted between the existing plugin header and the body:

```
┌────────────────────────────────────────────────────────────────┐
│ PHANTOM    [preset pill]  [A][A→B][B]  [Effect|Resyn]   KAIGEN │  ← existing header (unchanged)
├────────────────────────────────────────────────────────────────┤
│ MORPH  ●Lane 1   ════════●═══════   0.45  [CAPTURE]  7 armed   │  ← arc morph row
│ SCENE  ═════●═════                   0.30                      │  ← Scene Crossfade row (only when enabled)
└────────────────────────────────────────────────────────────────┘
  [ plugin body below: wheel, knobs, panels... unchanged ]
```

**Styling specs:**

- Background: `linear-gradient(180deg, #2b2e34, #22252b)` — dark, visually separated from the plugin's light body.
- Text color: `rgba(255, 255, 255, 0.85)` primary, `rgba(255, 255, 255, 0.45)` dim labels.
- Labels (`MORPH`, `SCENE`): 8px monospace-style caps with letter-spacing 2px, color `rgba(255, 255, 255, 0.45)`.
- Lane badge: inline-flex with a 8px colored dot + "Lane 1" text. Color matches the arc blue.
- Slider: 360px max-width, 6px tall, 3px border-radius. Fill is the morph/scene color gradient. Handle is a 14px circle in the morph color with a dark border.
- Value readout: 2-decimal monospace, 40px right-aligned.
- Capture button: 4px 10px padding, subtle border, primary accent in arc blue for enabled state.
- Armed count ("7 armed"): 9px dim text, right-aligned via `margin-left: auto`.

**Collapsed state (`MORPH_ENABLED == false`):**

- Panel still renders but as a single 24px-tall strip showing only `MORPH [○] click to enable`.
- Dot is the click target — clicking it flips `MORPH_ENABLED` to true and the panel expands.
- Scene row is completely hidden when `SCENE_ENABLED == false`.

### Capture mode UI

- User clicks `CAPTURE` button OR click-and-holds the morph slider handle.
- Panel enters "capture active" state:
  - `CAPTURE` button label changes to `COMMIT`; color shifts to warm accent.
  - A companion `CANCEL` button appears temporarily next to it.
  - All knob rings gain a subtle pulsing glow on the track ring (breathing animation, ~1.5s cycle).
  - Morph slider visually freezes at the right (1.0 position); user cannot drag it.
- User drags knobs; live APVTS updates; engine audio reflects the target state (because `smoothedMorph == 1.0` internally).
- `COMMIT`: arcs are computed from (current - baseline) per parameter; panel exits capture state.
- `CANCEL`: every parameter is restored to its captured baseline; panel exits capture state; arcs unchanged.
- Esc key: equivalent to `CANCEL`.

### Settings additions (Pro build)

New **Morph** subsection in the settings modal:

```
MORPH
──────────────────────────────
[x] Enable Arc Morph on startup
    Starts with the modulation panel visible and rings rendered.

[ ] Enable Scene Crossfade (dual-engine)
    Adds a secondary audio engine for structural param crossfading.
    Uses ~2× CPU when active and slots differ meaningfully.
```

The "Enable Arc Morph on startup" setting is a UX convenience — it sets the initial value of `MORPH_ENABLED` when a Pro build first loads without loaded preset data. Otherwise default is off.

Scene Crossfade toggle is the `SCENE_ENABLED` APVTS parameter (non-automatable) reflected here. Toggling it reveals/hides the Scene row in the modulation panel.

### Save modal updates

The A/B spec already added a Preset Kind radio (Single / A/B). Pro build extends it:

- The hidden "A/B + Morph" option from the A/B spec becomes visible in Pro builds.
- When selected, the save emits a `<MorphConfig>` child containing the current `<ArcLane>` and `<SceneCrossfade>` state alongside existing `defaultPosition`/`curve` attributes.
- Disabled with helper text if: no arcs are armed AND no Scene Crossfade state exists. Suggests "set some arcs first or save as A/B."

---

## Native Function Bridge

New native functions (Pro build only) exposed via `withNativeFunction` in `PluginEditor.cpp`:

| Function                       | Args / Returns                                                                 |
|--------------------------------|--------------------------------------------------------------------------------|
| `morphGetState`                | → `{enabled, morphAmount, sceneEnabled, scenePosition, armedCount, inCapture}` |
| `morphSetEnabled`              | `(bool)` — toggles `MORPH_ENABLED`                                             |
| `morphSetSceneEnabled`         | `(bool)` — toggles `SCENE_ENABLED`                                             |
| `morphGetArcDepths`            | → `{paramID: depth, ...}` — all non-zero arcs for UI knob-ring rendering      |
| `morphSetArcDepth`             | `(paramID, depth)` — direct drag updates                                       |
| `morphBeginCapture`            | → `true` — enters capture mode                                                 |
| `morphEndCapture`              | `(bool commit)` — returns list of modified paramIDs for UI feedback            |
| `morphGetContinuousParamIDs`   | → `[...]` — list of all APVTS param IDs that can be armed (used to know which knobs get rings) |

JS side lives in a new `Source/WebUI/morph.js` IIFE (not folded into `preset-system.js`). Rationale: morph logic spans knob-ring rendering across multiple existing canvas files, a new modulation panel, capture-mode UI, and save-modal integration. Keeping it separate prevents `preset-system.js` from growing past ~1500 lines. Follows the same `getNativeFunction` pattern established in the A/B spec — every native function resolved once at init.

JS state synchronization:

- `refreshMorphState()` called on init, after any morph action, and after APVTS parameter changes that could affect morph display (e.g., live pointer positions).
- Arc ring rendering on each continuous knob reads from `state.morph.arcs[paramID]`; un-armed knobs render no visible ring.
- Morph amount is read from the APVTS via existing slider-relay for the `morph_amount` parameter.

---

## File Structure

### New files (Pro build only)

```
Source/
  MorphEngine.h          — class declaration, guarded by #if KAIGEN_PRO_BUILD
  MorphEngine.cpp        — implementation
Source/WebUI/
  morph.js               — IIFE; arc ring rendering hooks, modulation panel
                           wiring, capture-mode UI. Kept separate from
                           preset-system.js to avoid bloating that file further.

tests/
  MorphEngineTests.cpp   — unit tests, ~15 test cases
```

### Modified files

```
Source/
  PluginProcessor.h      — conditional MorphEngine member + accessor
  PluginProcessor.cpp    — conditional pre/postProcessBlock calls; conditional
                           <MorphState> serialization in get/setStateInformation
  PluginEditor.cpp       — conditional 8 new native functions
  Parameters.h           — conditional 4 new APVTS params (MORPH_ENABLED,
                           MORPH_AMOUNT, SCENE_ENABLED, SCENE_POSITION)
  ABSlotManager.cpp      — slotsDifferMeaningfully(slotA, slotB) helper
                           (only useful for Scene Crossfade; can live here
                            since it inspects slots)
  PresetManager.cpp      — presetKind="ab_morph" save path emits extended
                           <MorphConfig>; read path delegates unknown
                           children to MorphEngine

Source/WebUI/
  index.html             — conditional modulation panel markup (Pro guard in JS)
  styles.css             — .mod-panel, .mod-slider, .mod-lane, .knob-ring-*
  preset-system.js       — save modal "A/B + Morph" option unhidden in Pro
  [each knob's rendering file — knob.js, recipe-wheel.js, etc.]
                         — add ring rendering hook reading from morph state

CMakeLists.txt           — KAIGEN_PRO_BUILD option; conditional source inclusion
tests/CMakeLists.txt     — MorphEngineTests + MorphEngine source (Pro build only)
```

### Build flag wiring

In `CMakeLists.txt`:

```cmake
option(KAIGEN_PRO_BUILD "Build the Pro SKU with morph features" OFF)

if(KAIGEN_PRO_BUILD)
    target_compile_definitions(KaigenPhantom PRIVATE KAIGEN_PRO_BUILD=1)
    target_sources(KaigenPhantom PRIVATE
        Source/MorphEngine.cpp
    )
endif()
```

Pro binary is produced by `cmake -S . -B build-pro -DKAIGEN_PRO_BUILD=ON`. Both builds cohabit the same source tree; runtime behavior diverges only on explicit flag.

---

## Testing

### Unit tests — `MorphEngineTests.cpp`

Following the Catch2 v3 pattern established by `ABSlotManagerTests.cpp`:

1. **Construction initializes empty arc map, morph at 0, scene disabled.**
2. **setArcDepth stores value; getArcDepth returns it; hasNonZeroArc detects.**
3. **setArcDepth with 0.0 removes entry from map** (treat 0 as "un-armed").
4. **preProcessBlock with morph=0 produces no parameter changes** — all params stay at their base values.
5. **preProcessBlock with morph=1, arc=+0.5 produces expected target = base + 0.5 * range, clamped to max.**
6. **Clamp behavior: base+arc exceeds max** — verify output plateaus at max.
7. **Clamp behavior: base+arc below min** — verify output plateaus at min.
8. **Internal smoothing lags behind raw morph changes** — set rawMorph 0→1 instantly; verify smoothedMorph takes ~multiple blocks to reach ~1.
9. **Capture mode: beginCapture → drag → endCapture(commit) sets arcs from delta.**
10. **Capture mode: beginCapture → drag → endCapture(cancel) restores params and arcs stay unchanged.**
11. **toMorphConfigTree / fromMorphConfigTree round-trip** preserves arcs, morph amount, scene state.
12. **fromMorphConfigTree with missing children handles gracefully** (e.g., no ArcLane node).
13. **Scene Crossfade: setSceneCrossfadeEnabled(true) creates secondaryEngine on first enable.**
14. **Scene Crossfade: position=0 output matches primary-only; position=1 output matches secondary-only; position=0.5 is mixed.**
15. **Scene Crossfade: secondary engine's pitch is synced from primary** — primary pitch non-zero → secondary pitch equals primary (verified via an engine accessor).

### Integration tests

1. **Preset save/load round-trip with morph data** — arc depths and scene state preserved.
2. **Plugin state save/restore with `<MorphState>` child** — full session round-trip.
3. **Standard build loading a Pro-authored preset** — arcs silently ignored; base patch + A/B slots load correctly.
4. **Pro build loading a Standard-authored preset** — morph engine initializes to empty arcs, disabled state.

### Manual test checklist additions (`MANUAL_TEST_CHECKLIST.md`)

New Pro-only section with ~10 items covering:

- Arc ring render on enabled knobs (rings visible, base tick visible, ghost pointer visible).
- Direct drag of arc endpoint changes depth; visual updates live.
- Capture mode: click CAPTURE → drag knobs → click COMMIT → arcs set as expected.
- Capture mode: cancel path restores originals.
- Morph slider sweeps 0→1; live pointers animate through segments.
- Plateau cap: set arc > range; verify pointer stops at max mid-sweep.
- Preset save with morph data → reload → arcs restored.
- DAW session save/reload preserves live morph state.
- Scene Crossfade enable: second row appears; sweeping position 0→1 crossfades audio.
- Scene Crossfade CPU: spikes to ~20% when enabled AND slots differ; stays ~10% when identical.

---

## Pro Seam / Out of Scope

### Locked extension points for v2+

Architectural seams intentionally preserved for future features:

- **Multiple morph lanes** — `lane1Arcs` is currently a single field. v2 can extend to `std::vector<std::unordered_map<...>>` indexed by lane ID without changing any caller. The `<ArcLane id="N">` XML is designed for it. UI panel grows vertically with additional lane rows.
- **Internal modulation sources** (LFO, envelope, sequencer) — morph amount is currently read from an APVTS parameter. v2 can extend by inserting an `IMorphSource` interface between "the thing driving morph" and `MorphEngine`. Source can be APVTS (current), LFO, envelope, step sequencer, MIDI-triggered. All sources produce a `[0, 1]` value; MorphEngine doesn't care where it came from.
- **MIDI-triggered morph positions** — naturally fits under the source abstraction above. v2 spec.
- **Arc preset layer** — `<ArcLane>` nodes can theoretically be extracted and applied to arbitrary base patches. A `.morphpack` file format wrapping one or more `<ArcLane>` nodes is a plausible v2 feature. No v1 code precludes it.
- **Per-parameter curves** — the curve attribute is currently per-lane. v2 could add optional `curve` per-`<Arc>` node. Not implemented but format extension is non-breaking.

### Explicit non-goals for v1

- License key / activation system (compile-time gating only).
- Curve shapes other than linear (framework for multiple curves present in the `curve` attribute, but linear is the only implementation).
- Modulating arc depths themselves in real time (arc depth is static between capture/drag operations).
- Morph presets separate from patches (the arc layer only lives within a full preset in v1).
- Automation lane visual inside the plugin showing the morph amount's history.
- Visual indication in Standard builds that Pro features exist (no upsell UI).

---

## Open Questions Carried Forward

None for v1. All design decisions pinned during the brainstorm of 2026-04-23. Working notes at `docs/morph-brainstorm-notes.md`.

For v2+ (noted in roadmap above, not blocking v1):

- Should arc capture-mode also capture discrete param changes? Probably not; would violate the "arcs are continuous-only" invariant. Structural changes stay outside morph.
- Should Scene Crossfade be extended to include input-engine-only crossfade (for comparing two input-processing configs without duplicating everything)? Premature optimization.
- Does morph engine need its own per-block gain smoothing to prevent scene crossfade clicks at position 0/1 boundaries? Probably yes; implement in `postProcessBlock`'s mix step. Noted for implementation plan.
