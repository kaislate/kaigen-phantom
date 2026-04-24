# Ableton Integration Paths — Devices Panel, Max for Live, Push

**Status:** Exploratory notes, 2026-04-23. Not committed to any path. Captured during the Pro morph module brainstorm as a parallel thread worth remembering.

**Context:** Phantom already works in Ableton Live 12 today via the VST3. Users can load it, automate it, save presets, use Push to encoder-map parameters via VST3 auto-mapping. This doc is about *deeper* integration beyond the baseline.

---

## The Three Interpretations of "Wrapper-Style UI in the Devices Panel"

When an Ableton user says "I want this plugin to feel more native in the device strip," they usually mean one of three different things with very different implementation costs.

### Path 1 — Ableton Rack Preset (`.adg` file)

**Effort:** ~1 day of design work. Zero new code.

Build an Instrument Rack or Audio Effect Rack that wraps Phantom (the VST3) and exposes up to 8 Macro knobs mapped to Phantom's most important parameters. Ship the `.adg` file alongside the plugin — users drop it into their Live library and get a Live-native horizontal strip on top of the device chain with 8 curated controls.

Phantom's WebView UI is still there, but the everyday interaction happens in Live's device strip. No code change to the plugin. Macros automatically become the primary Push knobs when the device is selected.

**Biggest user-experience win for smallest effort.** This is the obvious no-regret move.

### Path 2 — Max for Live Wrapper Device

**Effort:** ~3–6 weeks.

Write an M4L device that hosts Phantom VST3 internally and provides a Live-native UI layer on top. The M4L device can:

- Draw its own controls in Max's graphical patcher
- Expose parameters Live-natively (not just VST3 auto-mapped)
- Render custom content on the Push's LCD display (waveforms, meters, the recipe wheel, A/B state indicators)
- Use Live's object model for deep DAW integration (clip launching, chain-targeting, device macros)
- Ship parameter banks and page navigation tuned to how Phantom's controls group

The WebView UI is still accessible if the user clicks through; it just isn't the primary interaction surface for the Ableton + Push workflow.

Ableton-only, but the VST3 still ships for everyone else.

### Path 3 — Full Max/gen~ DSP Port

**Effort:** Months.

Port the C++ DSP chain to Max's gen~ DSL. Phantom becomes a native M4L device with no VST3 dependency. Lose all non-Ableton users (Logic, Cubase, Reaper, FL, Studio One). Almost certainly a strategic mistake unless Phantom pivots to being Ableton-exclusive. Not recommended.

---

## Push — Three Tiers

| Tier | Effort | Result |
|------|--------|--------|
| Today (baseline) | 0 | VST3 auto-maps APVTS parameters to Push's 8 encoders with paging. Labels come from `Parameters.h`. Works fine for power users. |
| With Path 1 (Rack preset) | 1 day | Macros become the primary Push knobs. Same 8 you expose are the 8 Push sees. Cleaner UX, zero new code. |
| With Path 2 (M4L wrapper) | Part of the 3–6-week budget | Custom display names, custom screen graphics, multi-page navigation tuned to your UX, custom pad mappings. Only route that feels truly "Ableton-native" on Push. |

---

## Tradeoff Summary

| Approach | Effort | Ableton lock-in | Push quality | Users it helps |
|---|---|---|---|---|
| Do nothing (ship VST3) | 0 | None | Functional | Everyone |
| Rack preset (Path 1) | 1 day | None | Good | Ableton + Push users |
| M4L wrapper (Path 2) | 3–6 weeks | Partial (VST3 still ships) | Excellent | Ableton-first users |
| Full M4L port (Path 3) | Months | Total | Excellent | Only Ableton users |

---

## Recommendation (preliminary)

1. **Do Path 1 (Rack preset) as a no-regret next step.** It's an evening's design work, ships as a single file alongside the VST3, improves the experience meaningfully for Ableton + Push users, and costs nothing elsewhere. Could even happen before morph lands.
2. **Queue Path 2 (M4L wrapper) as a standalone project after morph ships.** It's a real differentiator for the Ableton-native subset of your users. But it's its own spec → plan → implementation cycle, fully separate from the DSP work. Wants dedicated focus, not interleaving with C++ development.
3. **Skip Path 3.** Reconsider only if product strategy changes to Ableton-exclusive.

---

## Open Questions (if we pursue any of these)

### For Path 1 (Rack preset)

- Which 8 parameters deserve Macro slots? Candidates: Mode, Ghost, Phantom Threshold, Phantom Strength, Harmonic Saturation, Env Attack, Env Release, Output Gain. Needs curation.
- Do we ship one rack or several (e.g., "Bass Mode", "Resyn Mode", "Psychoacoustic Lead")?
- Naming / labeling convention for macros — full parameter names or shortened "HRM SAT", "GHOST", etc.?
- A/B snap + morph live inside the VST3 — macros can't expose them as knobs cleanly. Does that matter? (Probably fine — users click into the VST3 UI for A/B anyway.)

### For Path 2 (M4L wrapper)

- Which Push pages matter? Recipe wheel page? A/B compare page? Morph page?
- Does the M4L device replicate the WebView UI, or only augment it with Push-specific views?
- M4L doesn't have native access to the WebView's canvas. Do we duplicate spectrum/oscilloscope rendering in Max, or accept that those are WebView-only?
- Licensing — M4L requires a separate user license. Does shipping an M4L wrapper gate users who don't have it? (It probably does — note this in docs.)

### For Path 3

- Skipped. Not recommending.

---

## Out of Scope (for all paths)

- Standalone Max (non-Live) port. Even less user overlap than pure M4L.
- VST3 UI changes — orthogonal. The rack/M4L layer sits on top; the WebView stays as-is.
- Cross-DAW "rack" equivalents (Logic's Summing Stacks, etc.). Other DAWs have different abstractions; a Live-native Rack preset doesn't translate.
