# Morph OLED-Ridge Arc Editor — Handoff Notes

**Written:** 2026-04-24, 2:17 AM local. Context-compacting-imminent snapshot so a fresh session can resume without losing the thread.

**Status:** Pre-work. Morph system ships and works end-to-end in the unified build as of PR #15 (commit `9fd618d`). This doc captures the next UI improvement we're about to design and build: moving arc-depth editing onto the OLED ridge around the recipe wheel.

---

## What's Already Shipped (morph v1)

The unified-binary morph system lives in every build now. Key features:

- **Modulation panel** at the top of the plugin: MORPH label, enable dot, Lane 1 badge, draggable morph slider, value readout, CAPTURE button, armed-count status, optional SCENE row (when Scene Crossfade toggled in settings).
- **Capture mode:** click CAPTURE → drag knobs to desired positions → click COMMIT. Arcs computed from delta from baseline.
- **Direct arc drag:** small blue handle on the outer ring of each armed knob can be dragged to adjust depth (but the knob rings are small and cramped, which is the problem this work addresses).
- **Morph sweep:** drag the horizontal morph slider 0→1 to sweep all armed arcs.
- **Scene Crossfade:** opt-in via settings. Dual-engine crossfade between A/B slots; audio mixes cleanly without flicker; scene=1.0 matches snap-to-B audio exactly.
- **Preset format:** `<MorphConfig>` with `<ArcLane>` + `<SceneCrossfade>` children; `capturedBase` persisted per arc.

Relevant files:
- `Source/MorphEngine.h/cpp` — DSP and state
- `Source/WebUI/morph.js` — panel + ring rendering + gestures
- `Source/WebUI/knob.js` — `PhantomKnob::setMorphState()` + `_renderMorphRing()`
- `Source/PluginProcessor.h/cpp` — wires MorphEngine, `syncEngineFromValueLookup`
- `Source/PluginEditor.cpp` — native bridge + WebSliderRelays for morph params

Unit tests: 100 Pro, 77 Standard — all passing.

---

## The Problem

Knob rings render correctly but are physically tiny. When Phantom's smaller knobs (Harmonic Engine row: Saturation, Strength, Skip, etc.) get an arc, the blue ring segment is only a few pixels tall. Users can't read the depth accurately, and fine-tuning the drag handle on a ~22px knob is fiddly. The UX problem is real.

## Locked Design Decision — Option A from brainstorm

Moving the **editing surface** onto the OLED ridge around the recipe wheel. Keeping the **status indicators** (small blue rings) on each knob.

### The UX flow

1. Morph is enabled.
2. User clicks any continuous-parameter knob (on the recipe wheel, harmonic engine, filter, RESYN section, anywhere).
3. That knob becomes "focused" — visual highlight (maybe a subtle blue outline or color shift).
4. The OLED ridge — currently a decorative ring around the center display — **lights up** and becomes an editable arc control for that knob's depth.
5. The OLED text area shows focused param name + current arc value: `GHOST • +22%` or `RECIPE H2 • −18%`.
6. User drags on the ridge to set depth precisely. The knob's small ring updates live to reflect the edit.
7. Click somewhere else (background, another knob, or press Esc) → defocus. OLED returns to its normal state (pitch / preset display).

### Why this UX shape

- **Keeps knobs uncluttered.** Small rings stay informational only. No per-knob drag target.
- **Gives a big surface for precision.** The OLED ridge is ~200px diameter — easily 10× the circumference of a small knob's ring.
- **Echoes the plugin's existing visual language.** The OLED is already the plugin's central "info surface"; making it also the "edit surface" for arcs reinforces its centrality.
- **Low-collision with existing interactions.** The OLED doesn't currently have drag gestures. Clicking a knob while morph is NOT enabled still works normally (knob drag). Only in morph-enabled mode does click-to-focus take precedence.

---

## Open Questions For Tomorrow's Session

Sorted by how much they shape the architecture:

### 1. Visual treatment of the focused state

**(a) OLED ridge becomes a Pigments-style arc editor.** Full 270° ring around the OLED, split base-to-target with the same bright/dim fill behavior as the small knob rings but bigger. Single drag handle. Matches the knob aesthetic scaled up.

**(b) OLED ridge becomes a horizontal-arc editor.** The ridge renders as a straight-ish bar mapped to depth. Less visually consistent with the rest, but potentially easier to drag linearly.

**(c) Something hybrid.** Ridge shows the arc *plus* an inline "current depth" numeric slider inside the OLED text area.

**Lean:** (a) — consistency wins, and the user explicitly invoked Arturia Pigments as the reference.

### 2. How does focus release work?

- Click the background to defocus? (Need a specific clickable surface.)
- Click the focused knob again? (Toggle.)
- Click a different knob? (Moves focus to that knob.)
- Esc key? (Already wired as capture-cancel; would need to disambiguate.)

**Lean:** clicking the focused knob again toggles focus off; clicking a different knob moves focus; no background-click deselect (too accident-prone while adjusting slider).

### 3. What gets focused? Only morphable params?

Continuous params have arcs; discrete params don't. When user clicks a discrete param (Mode, Ghost Mode, etc.) with morph enabled — what happens?

**Lean:** click-to-focus only works on continuous params. Discrete knobs still respond normally. Optionally show a brief "can't morph this" hint as a non-error.

### 4. Does the focused arc display tick marks or value grid?

The OLED ridge is big enough to show helpful tick marks at common depths (0, ±25%, ±50%, ±75%, ±100%). Plus text like `+22%` above or below.

**Lean:** yes, subtle minor ticks every 25%, major at 0/100%, with the current value textually displayed.

### 5. What happens to the small knob ring when that knob is focused?

- **(a) It stays as-is** (both small ring and OLED ridge show the same depth, updating in sync).
- **(b) It fades/hides** (OLED is the "active" view; small ring suppressed to reduce visual noise).

**Lean:** (a). Redundancy is fine; users can glance at either.

### 6. Capture mode interaction

Capture mode is a batch workflow: hit CAPTURE, drag a bunch of knobs, commit. Does click-to-focus interfere with that?

**Probably:** during capture, click-to-focus is disabled. Knob drags work normally for capture purposes. Click-to-focus only works *outside* capture mode.

### 7. How should OLED text behave when focused?

Currently OLED shows pitch (`---` or `170 Hz`) and preset (`Stable`, etc). When focused for morph:

- Replace with `GHOST • +22%` style?
- Show both (pitch above, morph below)?
- Toggle between them?

**Lean:** fully replace while focused. Morph-focused is a brief modal state; pitch/preset info returns when defocused.

---

## Implementation Sketch (for spec-writing tomorrow)

### UI layer changes (morph.js + knob.js + CSS)

- **Knob → OLED focus relay.** Add event listeners on every `<phantom-knob data-param="...">` that, when morph is enabled, intercept the first click to route to focus-mode instead of knob-drag. Store focused param ID in `state.morph.focusedParamID`.
- **OLED ridge render hook.** The recipe wheel is a Canvas2D element (see `recipe-wheel.js`). Extend it so when `state.morph.focusedParamID != null`, it draws an additional "arc editor ring" on top of its existing rendering. Track base position, current arc depth, current morph-applied value. Pigments-style (ring option from Q1).
- **OLED ridge drag handling.** Hit-test clicks/drags on the ridge region of the canvas. Convert pointer angle to depth. Call `native.morphSetArcDepth(paramID, newDepth)`.
- **OLED text override.** When focused, recipe-wheel's OLED text switches from pitch/preset to `PARAM NAME • ±XX%`.
- **Defocus handlers.** Click on focused knob again = defocus. Click on different knob with arc eligibility = move focus. Click elsewhere on wheel/canvas = no change.

### No DSP / C++ changes needed

Focus is purely a UI state. `native.morphSetArcDepth` already exists and works; we're just adding a new editing surface that calls it.

### File touch list (estimate)

- `Source/WebUI/morph.js` — add focus state, click-router, OLED ridge drag handler
- `Source/WebUI/recipe-wheel.js` — add conditional "arc editor ring" overlay + OLED text override
- `Source/WebUI/styles.css` — minor styling for focused-knob outline
- `Source/WebUI/index.html` — possibly none (if focus purely canvas-driven)
- Unit tests — none (pure UI)
- Manual test checklist — add focus-mode entries

---

## Risks / Things To Watch

- **Canvas2D hit-testing accuracy.** Recipe wheel canvas is already doing hit-testing for the spokes. Adding ridge hit-testing is additional but similar work.
- **Preventing conflict with existing spoke drag.** Recipe wheel's H2-H8 spokes currently respond to drags. Click-to-focus on the WHEEL CENTER (OLED area) must be distinguishable from spoke interaction. Probably: ridge focus is only active when `state.morph.focusedParamID != null` and `state.morph.enabled`. Otherwise wheel behaves as today.
- **Recipe H2-H8 focus special case.** The harmonic spokes share identity with the recipe-wheel drag interface. If user clicks one to focus, the ridge becomes the editor for that harmonic. When the ridge edits depth, the spoke visualization should reflect the arc progression just like any other knob.
- **Rendering perf.** Recipe wheel is already one of the 30fps canvas consumers. Adding a conditional ridge overlay shouldn't measurably impact CPU, but worth eyeballing.

---

## Resume Instructions For Fresh Session

1. Read this doc first.
2. Read `docs/superpowers/specs/2026-04-23-morph-design.md` for the broader morph spec.
3. Review `Source/WebUI/morph.js` and `Source/WebUI/recipe-wheel.js` to understand the current rendering + interaction code.
4. Run brainstorming skill to pin down the open questions above, then writing-plans to turn it into an implementation plan.
5. Subagent-driven-development to ship. Expect ~1–2 hours of subagent dispatches.

Before starting fresh work, verify unit tests still pass (`cmake --build build --target KaigenPhantomTests --config Debug && ./build/tests/Debug/KaigenPhantomTests.exe`) — expect 100/100.

## Adjacent v2 Items We've Deferred

Not this work's scope but worth remembering:

- **Multiple morph lanes** (`<ArcLane id="2">`, etc.). Storage format already supports it.
- **Internal morph modulators** (LFO/envelope/sequencer driving the morph knob).
- **MIDI-triggered morph positions** (note-on → jump to specific morph value).
- **Arc preset layer** (`.morphpack` portable arc configs).
- **Knob-ring differential rendering** (30fps full rebuild is wasteful — skip unchanged knobs).
- **Scene Crossfade: shared pitch tracker** between primary + secondary (avoid duplicate zero-crossing detection in Resyn mode).
