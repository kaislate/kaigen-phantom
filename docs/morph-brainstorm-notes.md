# Pro Morph Module — Brainstorm Working Notes

**Status:** Active brainstorm (not a final spec). Consolidates into `docs/superpowers/specs/2026-04-23-morph-design.md` when open questions close.

**Context:** Pro SKU of Kaigen Phantom. Layers morph capabilities on top of the already-shipped Standard-build A/B compare system (PR #12, commit `ec969dc`). Gated by compile-time flag `KAIGEN_PRO_BUILD` (not yet defined).

---

## Major Pivot (2026-04-23 mid-brainstorm)

**From:** "Morph as slot crossfade" — morph knob sweeps between two fully-configured snapshots (slots A and B), using hybrid single-engine-plus-dual-engine DSP.

**To:** "Morph as per-parameter modulation with floating arc widgets" — single engine, per-knob arc widgets that let the user declare exactly which knobs move and by how much when morph is swept 0→1.

**Why we pivoted:** The slot-crossfade model forced users to either match every parameter between slots (to avoid unintended morphing) or accept a rigid "all-or-nothing" behavior. The arc model lets a producer say "only these 3 knobs sweep over 8 beats during this drop" declaratively, per knob. It's more expressive for real use cases, simpler in DSP, and doesn't conflict with the A/B compare system shipped yesterday.

---

## Locked Decisions (as of 2026-04-23)

### Architecture

1. **A/B compare stays exactly as shipped.** No rework to PR #12. It remains a Standard-build feature (binary slot toggle, manual snap).
2. **Arc-based morph is a new, independent Pro-only feature.** It doesn't depend on A/B slots at all. A/B compare and morph are two separate tools serving two different use cases:
   - **A/B compare** — "which of my two saved settings is better?" Binary toggle.
   - **Arc morph** — "sweep these knobs for a performance gesture." Continuous modulation.
3. **DSP strategy: single engine with per-parameter arc interpolation.** Per block, for each continuous parameter with a non-zero arc: `target = base + arc * morph`, push to engine. CPU cost near zero on top of the ~10% Release baseline.
4. **Product positioning:**
   - Standard: ships today, A/B snap only, no morph UI.
   - Pro: everything Standard has + arc morph (+ future roadmap below).
   - Two separate binaries. Compile flag `KAIGEN_PRO_BUILD` gates Pro-only code.
   - No Pro upsell UI in Standard builds (no locked toggles, no nags).
5. **Graceful degradation:** A/B+Morph presets loaded into Standard build treat as plain A/B. Arc data in the preset is silently ignored. (Extends to arc-morph presets when the format lands.)

### Interaction model

6. **Arcs are bipolar** — positive or negative delta per parameter.
7. **Arcs only apply to continuous parameters.** Discrete params (Mode, Bypass, Ghost Mode, Binaural Mode) don't get arcs; they stay at their base value during morph. If a user wants a structural change as part of a "scene," they do it outside morph or use Scene Crossfade (see roadmap).
8. **Arc gesture: hybrid (direct drag + capture mode).**
   - **Direct drag** on the arc widget above each knob for single-knob precision.
   - **Capture mode** (click-and-hold a capture button / morph knob) temporarily takes the plugin to morph=1; user drags the actual knobs to desired targets; release sets arcs from the difference. Fast batch setup.
9. **Morph is explicit enable/disable.** Arcs only visible when morph is enabled. Off = Pro looks like Standard.
10. **Morph amount is an automatable APVTS parameter.** DAW can sweep it. Internal smoothing (~10–20 ms, fixed time constant) prevents zipper noise on fast automation.

### Still pending (open questions below)

- Morph control placement in header (visual mockup pushed, user hasn't picked)
- Arc clamping behavior when base + arc * morph exceeds param range
- Arc visual treatment polish (the warm-amber arc above a knob — reference mockup in visual companion)
- Preset format for arc data
- Exactly which parameters get arcs (all continuous, or only the main panel subset)
- Whether Scene Crossfade ships with Pro v1 or as a v2 update

---

## Pro v1 Scope (what we're building first)

- Morph enable/disable toggle
- Per-knob arc widgets (bipolar) on all continuous parameters (likely; to confirm)
- Morph amount knob (header, placement TBD)
- Direct arc drag + capture-mode gesture
- Arc rendering that's subtle at 0 and visually prominent as depth increases
- APVTS-exposed morph amount parameter for DAW automation
- Preset format extension: `<MorphConfig>` already stubbed from A/B spec; extend with arc values
- Save/load flow for arc-morph presets
- Graceful degradation: morph-arc presets load into Standard as plain A/B where possible

---

## Pro v2+ Roadmap — Genuinely Differentiated Capabilities

These came up during brainstorm as "what makes Pro irresistibly valuable beyond arc morph alone?" The user likes all three as future capabilities. Not necessarily v1 commitments but **kept in sight during v1 architecture so we don't paint ourselves into a corner.**

### 1. Multiple morph lanes

Not just ONE morph knob driving arcs. Offer 4 (or 8) independent morph knobs, each with its own independent arc assignments. Every knob can be "armed" on multiple morph lanes simultaneously at different depths.

- Example: Morph A sweeps the filter, Morph B sweeps the harmonic recipe, Morph C sweeps stereo width. All three can be automated independently.
- Similar to Ableton's 16 macros but with arc depth control, not just 1:1 mapping.
- Produces performance gestures no plugin chain can match.
- **Architectural note for v1:** design the arc storage per-parameter-per-lane from the start. v1 = 1 lane; v2 adds more lanes without data model rewrite.

### 2. Internal morph modulators (LFO / envelope / step sequencer)

Pro includes built-in modulation sources that can drive the morph knob(s) automatically:

- **LFO** — syncable to DAW tempo, various shapes (sine, triangle, square, sample-and-hold).
- **Envelope** — triggered by input audio level (side-chain-style) or MIDI note-on.
- **Step sequencer** — rhythmic morph patterns.

Users don't need to leave the plugin for automated movement. External plugins can't do this because automation is DAW-level, not plugin-internal.

- **Architectural note for v1:** the morph knob internally reads from a "modulation source" abstraction. v1 = source is "APVTS value" (user/DAW). v2 adds more sources.

### 3. MIDI-triggered morph targets

Play a chord → morph jumps to a preset position. Play a single note → morph returns to 0. Map any MIDI note to a specific morph position per lane.

- Turns Phantom into an expressive real-time instrument for live performance, not just a studio effect.
- Cleanly combines with existing MIDI_TRIGGER_ENABLED / MIDI_GATE_RELEASE parameters.
- **Architectural note for v1:** keep MIDI handling loosely coupled to the morph target; designing a MIDI→morph mapping is a v2 spec. v1 just needs the morph knob to be driveable by something other than "user drag."

### 4. Scene Crossfade (opt-in dual-engine mode) — still under consideration

Separate from arc morph. Would reuse A/B slots (already shipped) and crossfade them at the audio level via a second `PhantomEngine` instance running slot B's config in parallel. Unlocks:

- Effect ↔ Resyn mode sweeps (discrete Mode param can't participate in arcs).
- Smooth Ghost Mode / Binaural Mode transitions.
- Layer mode (additive mix of two scenes instead of crossfade).

**Tension:** "two plugin instances + inverted gain crossfade" can replicate most of the audio behavior externally today. Differentiation is workflow/preset/automation/integration/discoverability — real but softer than arc morph's uniqueness.

**Tentative plan:** not headline Pro feature. Either opt-in v2 mode or drop entirely in favor of arc morph + v2+ roadmap above. Revisit before spec lock.

### 5. Arc preset layer (if time permits)

Save/load just the arc configuration separately from the base patch. Lets users apply a "morph recipe" (e.g., "filter sweep + recipe shift") to any base sound. Light feature, high utility.

---

## Why this roadmap matters for Pro justification

"You can already do this with two plugin instances + gain crossfade" is a fair critique of Scene Crossfade. But:

- **Arc morph** has no external equivalent — per-parameter modulation depth with a single morph knob is a modulation system, not audio summing. Genuinely unique.
- **Multi-lane morph** has no external equivalent — can't get independent macro-like behavior from external tools.
- **Internal modulators** have no external equivalent — automation is DAW-level, not plugin-internal.
- **MIDI-triggered morph** has no external equivalent — per-note mapping to morph positions requires internal implementation.

Pro's value proposition is "a modulation system for sound morphing," not "a crossfader for two patches." That's a real differentiator that external workarounds can't replicate.

---

## Non-Blocking: Current Baseline Perf

- **Release build idle CPU: ~10%** (measured 2026-04-23 in Ableton Live 12, no audio playing, editor visible).
- CPU drops only on bypass/disable. Audio thread (not WebView) is the main consumer.
- Arc morph adds near-zero CPU; target total for Pro v1 morph = ~10–11%.

### Quick-win optimizations (post-morph follow-up)

Flagged during CPU investigation. Not blocking morph, 1–3% potential savings:

- Precompute Hann window in `prepareToPlay` instead of recomputing each FFT completion.
- Precompute log-spaced bin boundaries.
- Silence early-out on FFT (skip transform when input peak < threshold).
- Skip output-spectrum FFT when advanced panel is closed (no consumer).

### Deeper optimization (unverified)

- `WaveletSynth` zero-crossing detection may not short-circuit on silence. If not, this is probably the biggest single win. Hasn't been measured yet.

---

## Multi-core / threading discussion (2026-04-23)

**User asked:** would the plugin benefit from being multi-core aware (16-core CPU)?

**Answer captured:** generally no, plugins inside a single track run on a single audio thread; the DAW handles track-level parallelism across cores. Internal worker threads fight real-time constraints (wakeup latency > work saved for small buffers). SIMD auto-vectorization (one core, data-level parallelism) already kicks in via the compiler. The one scenario where worker threads might pay off is Scene Crossfade dual-engine — parallelize engine1 and engine2 on separate threads with a sync barrier before the crossfade. Would only benefit larger buffer sizes. Verdict: build morph as single-threaded; revisit for Scene Crossfade if profiling ever shows a need.

---

## Related: Ableton Integration Paths

Separate thread during this session — captured at `docs/ableton-integration-paths.md`. Summary: three paths (Rack preset / M4L wrapper / full M4L port). Path 1 (Rack preset with 8 macros) is a no-regret quick win, Path 2 is a standalone project for after morph ships. Parked pending focus on morph.

---

## Out of Scope for Pro v1

- Scene Crossfade dual-engine mode (deferred to v2 or dropped)
- Multi-lane morph (reserved for v2; v1 architected to support it without rewrite)
- Internal modulators (LFO / envelope / sequencer) — v2+
- MIDI-triggered morph targets — v2+
- Arc preset layer — v2+ (or late v1 if time permits)
- License key / runtime unlock — compile flag only
- User-drawn morph curves
- Per-parameter morph curves (individual shaping per param). Global smoothing only.
- Morph between more than one pair of states (v1 = one morph lane)

---

## Carryover Followups from A/B Compare PR #12

From the final code review, still relevant to morph:

- Promote preset-format node-name literals (`ABSlots` / `SlotB` / `MorphConfig`) to shared `PresetFormat.h`. Morph adds more literals (`MorphArcs`, `MorphAmount`, per-lane fields); this is the right moment to unify.
- `savePreset`'s identical-slot check uses `toXmlString()` — inefficient. Morph will need fast slot-diff patterns; time to fix.
- Multi-step snap round-trip test (A→B→A→B). Morph will want "sweep morph 0→1→0→1" round-trip equivalent.
- `designerAuthored` clear-on-save decision — same question for morph saves.

---

## Next Steps in the Brainstorm

1. **User picks morph placement** from visual companion (pending from last push).
2. **Arc clamping behavior** — what happens when base + arc * morph exceeds parameter range?
3. **Scene Crossfade — v1 or defer to v2?**
4. **Preset format for arcs** (XML structure decision).
5. **Which parameters get arcs** (all continuous, or subset).
6. **Write final spec**, user review, hand to writing-plans.
