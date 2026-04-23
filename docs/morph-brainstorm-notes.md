# Pro Morph Module — Brainstorm Working Notes

**Status:** Active brainstorm (not a final spec). Consolidates into `docs/superpowers/specs/2026-04-23-morph-design.md` when we close open questions.

**Context:** Pro SKU of Kaigen Phantom. Layers continuous morph (0–1 blend between A/B slots) on top of the already-shipped Standard-build A/B compare system (PR #12, commit `ec969dc`). Gated by compile-time flag `KAIGEN_PRO_BUILD` (not yet defined).

---

## Decisions Already Locked (from A/B compare brainstorm + this session)

### Product positioning

- **Standard Phantom** (shipping today): discrete A/B snap, no morph.
- **Pro Phantom** (this project): adds continuous morph knob (0→1 blend between the same A/B slots). Morph amount is **automatable** — it's a first-class APVTS parameter so DAWs can sweep it over time.
- **No Pro upsell UI in Standard builds.** No locked toggles, no "Upgrade" nags. Pro and Standard ship as separate binaries. Website does the marketing.
- **Gating mechanism:** compile-time `KAIGEN_PRO_BUILD` flag only. Two binaries. Runtime license-key unlock is a future concern, not in scope for this module.
- **Graceful degradation:** A/B+Morph presets loaded into Standard build behave as plain A/B presets. Morph metadata silently ignored. Already wired in the Standard build's `PresetManager::loadPresetInto`.

### Seam (already in `master`, shipped 2026-04-23)

- `ABSlotManager::getSlot(Slot) const -> const juce::ValueTree&` — read-only accessor the morph engine consumes.
- `<MorphConfig>` optional preset child — attributes `defaultPosition` (0–1 float) and `curve` (string; linear only for now, extensible later).
- `presetKind="ab_morph"` metadata value.
- `PresetManager::savePreset(..., PresetKind kind, ...)` accepts `ABMorph` kind. Standard build doesn't expose it in UI but the code path accepts it.

### Recipe preset format (already)

- Preset root is an APVTS state tree.
- Optional `<SlotB>` child holds the B-slot state tree.
- Optional `<MorphConfig>` child for morph-capable presets.
- `<Metadata>` child has `presetKind` property.
- Standard build reads all of these but ignores `<MorphConfig>` on load.

---

## Open Questions (still to decide)

### 1. DSP strategy — 3 options on the table

| | Behavior | CPU (on ~10% baseline) | Impl complexity |
|---|---|---|---|
| (a) Parameter-level interp, single engine | Lerp continuous params; discrete params snap at morph=0.5 (or stick to nearest slot) | ~10% (≈0% added) | Low |
| (b) Dual-engine always-on | Two `PhantomEngine` instances; audio crossfade. Everything smooth. | ~20% (always) | Medium |
| (c) Hybrid (leaning) | Single engine when discrete params match OR morph is at 0/1; dual engine only when discrete params differ AND morph ∈ (0, 1) | ~10% typical, ~20% when actively morphing discrete | High |

**Recommendation (Claude):** (c) — honest pricing, users only pay when using the feature against differing discrete configs. User not yet committed.

### 2. Discrete-param handling when morph is mid-position

- Four discrete params: Mode, Bypass, Ghost Mode, Binaural Mode.
- In (a): must pick a rule — step-switch at 0.5, lock-to-nearest-slot, or exclude-from-morph-entirely.
- In (b) and (c): dual-engine path crossfades audio output across the discrete boundary smoothly; no rule needed since both engines run in their respective slot's full config.
- **Interaction with existing "Include discrete params in A/B snap" setting** (per-project, Standard feature): does it apply to morph too? If OFF, should morph only affect continuous params? If ON, morph behavior needs a strategy per above. Need to pin this.

### 3. Morph knob edit semantics — "what happens when user tweaks a knob while morph ∈ (0, 1)?"

Candidate rules:
- **(i) Write to nearest slot, lock morph position.** User's edit goes to A if morph<0.5 else B. Morph knob stays where it was.
- **(ii) Snap morph to 0 or 1 on knob-touch.** Closer endpoint wins. Edit goes to the slot the user just snapped to.
- **(iii) Write to both slots proportionally.** Edit = blended delta applied weighted; feels magical but might produce surprising results.
- **(iv) Write to the slot corresponding to last-pressed A/B button.** Morph and edit are orthogonal — morph just blends, edits always go to the "active" slot via the A/B buttons.

Leaning (iv) — it preserves the Standard mental model. Each slot has an "active" state already; morph is an additional blend applied after the fact. Needs user confirm.

### 4. Curve shape (morph knob → engine blend value)

- Linear (0 at morph=0, 1 at morph=1) — simplest, most predictable.
- S-curve (sinusoidal ease-in-out) — smoother transitions, more "musical" sweeps.
- Exponential — useful for recipe weights that scale nonlinearly (e.g., harmonic amplitude).
- User-drawn (advanced) — overkill for v1.
- **Per-preset default curve** stored in `<MorphConfig curve="...">` — already in the preset format; need to decide what curves to support as enum values.

Leaning: **ship linear first, add S-curve as a second curve option in `<MorphConfig>` for presets that want it.** Defer user-drawn. User not confirmed.

### 5. Automation smoothing

- Morph is an automatable APVTS parameter. DAW can slam it 0→1 in a single block.
- Without smoothing, that's a jump of all 39 continuous params in one audio block → zipper noise.
- With smoothing: multi-ms time constant on the internal morph value, driven by the raw automation.
- Typical approach: single-pole IIR with per-block alpha, time constant ~20 ms. Expose "Morph Response Time" as advanced setting?

Leaning: fixed internal time constant (10–20 ms), not exposed. Expose only if users complain.

### 6. UI — morph control placement and style

Options in header cluster:
- **Horizontal slider above the A/B cluster** — matches FabFilter Pro-Q automation look. Compact.
- **Dedicated small knob next to A/B pills** — consistent with the plugin's knob-heavy aesthetic.
- **Morph value on the A/B cluster itself** — e.g., a gradient fill on the A or B pill showing morph position. Clever but might be hard to read at a glance.

Visual companion will help here. User already consented to it.

### 7. Automation feedback visual

- When DAW is driving morph (vs. user dragging), show some indicator? A small dot on the slider? A glow? Nothing?
- This is a polish question; punt until slider is placed.

### 8. Interaction with other engines

- `WaveletSynth` ("Resyn mode") has internal state (wavelet capture buffer, zero-crossing history). Two instances in dual-engine path will have independent capture buffers — they might miss/duplicate zero-crossings if they see the same input stream.
- `BassExtractor` has LR4 filter state. Two instances have independent filter state, which drifts across blocks — sum of two mid-state filters ≠ a single correct filter. This is the "phase coherence" risk mentioned earlier.
- **Open design question in dual-engine mode:** share some internal state (wavelet capture, filter) between the two engines to avoid divergence, or accept the drift and crossfade audio outputs?

### 9. Save flow updates (Pro build only)

- Save modal: Pro build re-enables the `A/B + Morph` radio option (already stubbed; hidden in Standard).
- Selecting A/B + Morph writes the preset's `<MorphConfig>` with the current morph knob value as `defaultPosition`.
- Curve: always `linear` for v1, or let designer pick at save time? Probably keep save-time simple — always write `linear`; future feature adds a curve selector.

### 10. Preset load flow (Pro build)

- Loading `presetKind="ab_morph"` preset: populate both slots (same as AB load), additionally read `<MorphConfig>` and set the morph knob to `defaultPosition`.
- Should it also apply the curve? Yes — curve is part of preset intent. Load reads it, stores it per-preset, applies on every block.
- Loading into a session that already has morph mid-position: overwrites, as expected.

---

## Non-Blocking but Related: Current Baseline Perf

- **Release build idle CPU: ~10%.** Measured 2026-04-23 in Ableton Live 12 with plugin open, no audio playing, editor visible.
- CPU drops to near-zero only on bypass or plugin-disable.
- WebView is not the main consumer (editor-closed test doesn't reduce CPU).
- Main consumer is `processBlock` running on silence.

### Quick-win optimizations available (found during investigation)

These are independent of morph but would add headroom:

- **Precompute Hann window** in `prepareToPlay`. Currently recomputed fresh on every FFT completion (8192 `std::cos` calls × 2 spectra × ~6 completions/sec ≈ 98k cos/sec).
- **Precompute log-spaced bin boundaries** in `prepareToPlay`. Currently recomputed every FFT completion (80 `std::pow(10, ...)` × 2 spectra).
- **Silence early-out on FFT**: if input peak < threshold for the full 8192-sample buffer, skip the transform and zero the bins.
- **Skip output FFT when advanced panel closed** (output spectrum is only drawn in advanced mode; no consumer → no work).

Estimated aggregate savings: 1–3% CPU. Worth a followup PR but not blocking morph.

### Deeper optimization, to investigate if needed

- `WaveletSynth` zero-crossing detection may not short-circuit on silence. If it's running full period-tracking + crossing-detection logic on a zero buffer, there's meaningful room here. Hasn't been measured yet.

---

## Out of Scope for This Module (explicit)

- License key / runtime unlock. Compile flag only.
- User-drawn morph curves.
- Automation lane visual differentiation beyond a simple indicator.
- Morph between more than two slots (e.g., A/B/C/D grid morph).
- Per-parameter morph curves (individual continuous params having their own blend shape). Global curve only.
- Factory morph presets — content task, separate work.
- Performance optimizations to the existing `processBlock` (filed as followup).

---

## Carryover Followups from A/B Compare PR #12 (relevant to morph)

From the final code review of the A/B PR, post-merge items we agreed to tackle later and which interact with morph:

- Promote preset-format node-name literals to a shared `PresetFormat.h`. Morph adds more literals (`<MorphConfig>`, `defaultPosition`, `curve`) — this is a good moment to unify.
- `savePreset`'s identical-slot check and `abGetState`'s `slotsIdentical` use `toXmlString()` comparison. Morph will read from both slots on every audio block; any slow slot-access patterns should be cached, not recomputed.
- Multi-step snap round-trip test (A→B→A→B). Morph testing will want similar "sweep morph 0→1→0→1" round-trip to lock the stored-tree semantic under continuous pressure.
- Decide whether `designerAuthored` should clear on `savePreset` (spec says yes, code says no). Same question for morph: does saving a new A/B+Morph preset while `designerAuthored` was set preserve or clear it?

---

## Next Steps in the Brainstorm

1. **User picks DSP strategy** (open question 1) — this gates most other design decisions.
2. **Pin discrete-param interaction** (question 2) — depends on (1).
3. **Pin edit semantics** (question 3).
4. **Curve + smoothing** (questions 4, 5) — quick.
5. **UI placement** (question 6) — use visual companion.
6. **Save/load flow deltas** (questions 9, 10) — small.
7. **Dual-engine internal-state sharing rule** (question 8) — only if we end up in (b) or (c). Probably a followup spec of its own.
8. **Write final spec, get user review, hand to writing-plans.**
