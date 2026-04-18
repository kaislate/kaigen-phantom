# Advanced Mode — Design Spec

**Date:** 2026-04-17
**Scope:** New "Advanced mode" for the Kaigen Phantom plugin that reveals 16 deeper DSP controls on a circuit-board-themed panel between the existing knob rows and the visualization section. The plugin "breaks apart" vertically to expose this hardware-internals view when enabled.
**Spec location:** `docs/superpowers/specs/2026-04-17-advanced-mode-design.md`

## Motivation

The main plugin UI currently exposes all 21 parameters at once, which makes the default view dense and hides the hierarchy of what most users actually need. Users asked for a simplified default that surfaces only the creative "hero" controls, with deeper DSP knobs tucked behind an advanced toggle. The visual metaphor is "opening the case on a piece of hardware" — the plugin physically splits between the knobs and the oscilloscope, revealing an animated circuit-board interior where the advanced controls live.

## Non-Goals

- Not an overhaul of the basic-mode knobs. Basic-mode `<phantom-knob>` rendering, sizing, geometry, and styling in `Source/WebUI/knob.js` are LOCKED by this spec. Any change there is explicitly out of scope.
- Not a plugin-state preset system (Arturia-style top-bar load/save is a separate, future feature).
- Not an editor for the recipe wheel's harmonic modes — the recipe wheel stays where it is with its existing behavior.
- Not a change to DSP, parameter ranges, or the APVTS schema beyond adding one boolean (`advanced_open`).

---

## §1 — Architecture

- **Single HTML file.** Advanced mode is a CSS class toggle on `.wrap`: `.wrap.advanced-open`.
- **New DOM section** `.advanced-panel` inserted between the existing `.ctrl-row` blocks and the `.viz-section` inside `.right-panel`. Hidden by default (`max-height: 0; overflow: hidden; opacity: 0`); reveals on class flip via CSS transition.
- **JUCE editor resize.** JS calls a new native function `setEditorHeight(n)` registered in `PluginEditor`. The editor calls `setSize(1600, n)`. Basic mode height = 820px. Advanced mode height = 1020px (+200px).
- **State persistence.** One new APVTS parameter `advanced_open` (bool, default `false`) so the toggle state survives plugin-state save/load. Not intended for automation; host may still see it listed.
- **Event flow.** User clicks toggle (header button or seam latch) → JS flips the CSS class → JS writes `advanced_open = true/false` via the JUCE bridge → JS calls `setEditorHeight(1020)` / `setEditorHeight(820)` in parallel with the CSS transition → Circuit-board Canvas rAF loop starts/stops accordingly.

---

## §2 — Advanced Panel Layout

The advanced panel is 1600px wide × 200px tall. It sits between the knob rows and the viz-section inside `.right-panel`. Black background with the circuit-board Canvas behind the controls.

**Control groupings, left to right:**

| Section | Controls | Count |
|---|---|---|
| **Phantom** | `phantom_threshold` (Crossover) | 1 |
| **Wavelet Synth** | `synth_duty` (Push), `synth_h1` (H1), `synth_sub` (Sub), `synth_wavelet_length` (Length), `synth_gate_threshold` (Gate), `synth_min_samples` (Min), `synth_max_samples` (Max), `tracking_speed` (Track) | 8 |
| **Punch & Envelope** | `punch_amount` (Amount), `synth_boost_threshold` (Threshold), `synth_boost_amount` (Boost), `env_attack_ms` (Attack), `env_release_ms` (Release) | 5 |
| **Binaural** | `binaural_mode` (combobox), `binaural_width` (Width) | 2 |

**Section dividers:** the same etched 2px linear-gradient separator used in the `.preset-grid` (dark 50% | white 50%), but rendered white-on-black for the dark background. A single vertical bar between each section.

**Section labels:** small uppercase Space Grotesk 9px labels above each section, color `rgba(255,255,255,0.32)`, matching the inverted treatment of the etched basic-mode `.el`.

**Layout math (approximate, all based on 44×60 mini-knob footprint from §3):**
- Phantom: 1 × 44 + horizontal margin ≈ 55px
- Wavelet: 8 × 44 + inter-knob margins ≈ 380px
- Punch & Envelope: 5 × 44 + margins ≈ 240px
- Binaural: 90px combobox + 44px knob + margin ≈ 150px
- 3 section separators × ~16px ≈ 48px
- Padding 24px L + 24px R ≈ 48px
- Total controls + padding ≈ 920px; remaining ~680px of panel width is breathing room where circuit-board traces route between sections.

---

## §3 — Advanced Knob (new component)

**File:** `Source/WebUI/knob-mini.js` (new). Loaded via `<script src="/knob-mini.js">` in `index.html` after `knob.js`.

**Tag:** `<phantom-mini-knob data-param="..." label="..." default-value="...">`

**Geometry:**
- 36px diameter, SVG viewBox `0 0 36 36`, all internal math from that.
- `:host` is `display: inline-block; width: 44px; height: 60px;` — 44×60 footprint includes the knob plus label room beneath.

**Visual (all rendered inside SVG, dark-mode):**
- Face: solid dark disc, `fill="#1A1B1C"`.
- Arc track: 270° sweep (135° → 405°), `stroke="rgba(255,255,255,0.08)"`, stroke-width 2, `stroke-linecap="round"`.
- Arc value: same sweep but ending at `135 + 270*value`, `stroke="#FFFFFF"`, stroke-width 2.2.
- Indicator tick: a short radial line from the disc edge inward by 4px, rotated to match the value's angle on the arc. `stroke="#FFFFFF"`, stroke-width 1.5, `stroke-linecap="round"`. Gives a classic plugin-knob pointer read.
- No OLED text, no triple-stack glow, no halo.
- Optional subtle rim: a 0.5px `stroke="rgba(255,255,255,0.15)"` circle at the disc's edge for definition against the circuit-board dark.

**Label (below knob, outside SVG):**
- Plain `<div class="mini-label">${label.toLowerCase()}</div>` inside the shadow DOM.
- 8px Kalam, white `#FFFFFF`, text-align center, letter-spacing 1px, lowercase.

**Drag readout:**
- While dragging (pointer captured), a `<div class="mini-readout">` positioned `position: absolute; top: -16px; left: 50%; transform: translateX(-50%);` shows the current value (e.g. `0.50` or `12 ms`).
- Fades in on pointerdown (opacity 0 → 1 in 80ms), fades out on pointerup (opacity 1 → 0 in 150ms).
- Font: 9px Courier New, white on faint dark pill (`rgba(0,0,0,0.72)`), 2px padding.

**Shared API with `<phantom-knob>`:**
- Same observed attributes: `value`, `display-value`, `label`, `default-value`, `data-param`.
- Same events: `knob-change` with `{ name, value }` detail.
- Same pointer behaviors: vertical drag to change, double-click to reset to `default-value`.
- Same JUCE bridge: the relay-binding code in `phantom.js` currently queries `document.querySelectorAll('phantom-knob[data-param]')`. Implementation must extend that query to include `phantom-mini-knob[data-param]` so every mini-knob's `data-param` registers a WebSliderRelay the same way.

---

## §4 — Circuit Board Visual

**DOM:**
```html
<canvas id="circuitCanvas" class="circuit-canvas"></canvas>
```
Positioned `absolute; inset: 0; z-index: 0;` inside `.advanced-panel`. Controls layer at `z-index: 2`.

**File:** `Source/WebUI/circuit-board.js` (new). Loaded in `index.html` after existing canvas scripts.

**Static layer** (drawn once on canvas resize):
- Background: solid `#0A0B0C`.
- Connection-point grid: 1px white dots at 30px intervals, `alpha 0.08`.
- Authored PCB traces: white lines (`rgba(255,255,255,0.18)`, 1px width) routed in 90°/45° angles between control positions. Routes are a simple declarative list in JS (array of point arrays); not computed from the DOM. Terminate at small "solder pad" circles (2px filled, same alpha) at control anchor positions.

**Animated layer** (30fps via `requestAnimationFrame`):
- Light pulses: every 800–1500ms (random in range) a new pulse spawns at a random trace endpoint, travels along that trace at 120px/sec. Rendered as a 5px white disc with a shadowBlur glow of 8px, full alpha.
- LED nodes: at each trace junction, a 3px filled disc, default `alpha 0.20`. When a pulse arrives at a junction, the LED flashes to `alpha 1.0` with bloom, decaying back to `0.20` over 300ms via a simple per-LED alpha envelope stored in a state object.
- Concurrent pulses: 4–6 active at once. Each pulse is a `{ pathIndex, t, startTime }` tuple; advanced `t` per frame, removed when `t >= 1`.

**Opacity coupling with panel:**
- Canvas CSS `opacity: 0` by default.
- Class `.wrap.advanced-open .circuit-canvas { opacity: 1; transition: opacity 400ms ease; }`.
- rAF loop started only when `.wrap.advanced-open` is true; stopped when the closing transition completes.

**Performance target:** <2% CPU on a modest laptop. If the animation profile reveals hot paths, first optimization is dropping to 20fps (`setInterval` or throttled rAF); second is reducing concurrent pulse count.

---

## §5 — Break-apart Transition

**Opening (basic → advanced):**

| t | Event |
|---|---|
| 0ms | JS calls `nativeFunction.setEditorHeight(1020)` → JUCE `setSize(1600, 1020)`. |
| 0ms | rAF loop for circuit-board Canvas starts (Canvas still at `opacity: 0`). |
| 50ms | `.wrap` gets class `.advanced-open`. CSS transitions begin: `.advanced-panel` `max-height: 0 → 200px`, `opacity: 0 → 1`, `transform: scaleY(0) → scaleY(1)` with `transform-origin: center`. Duration 400ms, ease-out. |
| 50ms | Canvas CSS `opacity` transitions `0 → 1` over 400ms. |
| 450ms | Transitions complete. |

**Closing (advanced → basic):**

| t | Event |
|---|---|
| 0ms | `.wrap` loses class `.advanced-open`. CSS transitions reverse: `.advanced-panel` collapses over 400ms. Canvas fades over 400ms. |
| 400ms | JS calls `setEditorHeight(820)` → JUCE `setSize(1600, 820)`. Panel DOM remains but is collapsed to 0 height. |
| 400ms | rAF loop stops; canvas state reset. |

**Fallback if DAW stutters during resize animation:**
- New CSS class `.wrap.stutter-safe` on `.wrap` switches to two-phase resize: JUCE resizes instantly at t=0, then content animations play inside the already-expanded frame over 400ms.
- User-facing setting: future. For v1 ship, use the smooth mode; if Ableton (or other hosts) visibly stutters, hand-set the class and ship that way.

---

## §6 — Toggle UI

Two affordances for one toggle. Both call the same JS function `toggleAdvanced()`.

**Header button:**
- New `<button class="hdr-btn" id="advanced-btn" title="Advanced controls">` placed immediately right of the existing settings gear, left of the KAIGEN logo.
- Icon: a chevron SVG inside the button. Default: down-facing (`▾`). Class `.hdr-btn.active` rotates it 180° (CSS `transform: rotate(180deg); transition: transform 250ms ease`).
- Same size/shape as existing `hdr-btn` (30×30 circle, neumorphic pill).

**Seam latch:**
- New element `<div class="seam-latch"></div>` positioned at the top of the `.viz-section` (spanning horizontally centered, overlapping the `.gap-h` above it).
- Size: 80px × 16px pill, `border-radius: 8px`, neumorphic inset (same box-shadow pattern as `.mt` mode toggle).
- Content: two small 3px dots, `background: rgba(0,0,0,0.32)`, spaced ~8px apart inside the pill.
- Hover: dots glow icy-white (`rgba(255,255,255,0.88)` with subtle `box-shadow: 0 0 4px rgba(255,255,255,0.6)`).
- `.advanced-open` state: pill keeps the same size but adds a tiny chevron-up SVG between the two dots, indicating the "collapse" affordance.
- Positioned absolutely inside `.viz-section` with `top: -8px` (half the latch overlaps into the `.gap-h`).

**No keyboard shortcut.** Two click affordances are sufficient.

---

## §7 — Acceptance Criteria

1. With `advanced_open = false` (default), the plugin renders at 1600×820 with no `.advanced-panel` visible. No circuit-board Canvas running. No visible degradation from today's basic mode.
2. Clicking either the header button or the seam latch with `advanced_open = false` triggers the opening transition; after ~450ms the plugin is 1600×1020, the advanced panel is visible with all 16 controls on the circuit-board background, and the animated pulses are visible.
3. Clicking either affordance again triggers the closing transition; after ~400ms the panel is collapsed, the Canvas is faded out, the rAF loop has stopped, and the plugin window returns to 1600×820.
4. All 16 advanced controls are wired through the existing JUCE parameter bridge: dragging any of them moves the corresponding APVTS parameter, and DAW automation of any advanced param updates the corresponding knob.
5. Saving the plugin state with `advanced_open = true` and reloading restores the plugin to advanced-open on load.
6. Basic-mode knobs are visually and behaviorally unchanged from the commit immediately prior to this feature merge. No regressions in basic-mode rendering.
7. `<phantom-mini-knob>` supports the same drag-reset-double-click behavior as `<phantom-knob>`.
8. All existing tests pass.

---

## §8 — Out of Scope / Deferred

- Top-bar full-plugin-state preset system (Arturia-style). Separate future feature.
- Keyboard shortcut for advanced toggle. Not requested.
- Circuit-board pulse customization (user-controlled speed, density). Values hard-coded.
- Per-DAW tuning of the transition mode. `.stutter-safe` class exists but is manually set; no auto-detect.
- Advanced-mode recipe wheel or H2-H8 individual knobs. Recipe wheel lives in basic mode only.
- Mobile/touch-specific tuning of the mini-knob. Pointer Events cover this via the existing model.
- Migrating any existing basic-mode control into advanced-mode — the advanced bucket is strictly the 16 controls listed in §2.
