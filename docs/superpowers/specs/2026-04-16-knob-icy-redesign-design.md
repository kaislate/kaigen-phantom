# Knob Icy Redesign — Design Spec

**Date:** 2026-04-16
**Scope:** `Source/WebUI/knob.js` + `Source/WebUI/styles.css` + `Source/WebUI/index.html`
**Goal:** Align every rotary knob in the plugin UI with the user's reference mockup (`C:\Users\kaislate\Downloads\New Project.png`): a black OLED summit raised above a seamless volcano slope that melts into the Studio Gray bezel, ringed by an icy-white value arc, with a handwritten label.

## Motivation

The current knobs (commit `bb32432`) use a steel-blue accent on a gray volcano with a visible outer edge (drop-shadow) and a Courier New label. The reference mockup reads as softer, friendlier, and more continuous with the bezel — the knob feels less like a screwed-on part and more like a raised spot on the bezel surface. We want that visual direction, keeping our existing neumorphic material language.

## Non-Goals

- No change to section labels (`.el`), preset strip (`.lw`), harmonic labels (`.h-label`), KAIGEN/PHANTOM logos, center recipe-wheel OLED, spectrum analyzer, or oscilloscope.
- No change to DSP behavior, parameter model, APVTS, or JUCE↔JS bridge.
- No change to knob interaction (vertical drag, double-click reset).
- No change to arc sweep geometry (still 135° → 405°, 270° total).

---

## §1 — Seamless Volcano Geometry

**Intent:** The knob stops reading as a distinct object. The OLED becomes the peak of a gentle slope that fades into the bezel without a visible outer edge.

**Changes to `knob.js`:**

- Remove the `filter: drop-shadow(...)` declaration on `:host`. This is what draws the hard outline today.
- Invert the `<radialGradient id="vg-{sz}">` stops. Today: brightest at `32%,28%`, darkest at outer edge — reads as a dome. New: brightest near the OLED lip, progressively darker outward, with the final stop matching the bezel color.
- Final outer gradient stop = the blended color of the bezel panel at that location. Panels use `rgba(0,0,0,0.02)` over `#AEAFB1`, which resolves to approximately `#A9AAAC`. Use this as the `100%` stop so there is zero contrast at the knob boundary.
- Keep the three existing rings (inner lip, outer bezel ring, outer glow) around the OLED — they define the crater lip, which stays crisp.

**Changes to `styles.css`:**

- Add a `::before` or sibling pseudo-element to each knob's grid cell drawing a `radial-gradient` halo behind the knob. Approximate spec: `background: radial-gradient(circle at center, rgba(255,255,255,0.28) 0%, rgba(255,255,255,0.12) 35%, transparent 72%);` sized to ~160% of the knob diameter, centered on the knob. The halo provides the soft bright spot visible in the reference image and disguises any residual edge of the knob.
- Halo must sit behind the knob (`z-index` below the SVG). It's purely visual; it does not intercept pointer events.

**Scale:** All geometric values scale with the knob size (40/56/88/114 → per §5 sizes). The halo diameter is computed from the knob's own diameter in CSS custom properties to avoid size-specific CSS.

---

## §2 — Icy White Accent

**Intent:** Replace all instances of the steel-blue accent (introduced in commit `bb32432`) with pure icy white. One accent color across the entire knob surface.

**Concrete color spec (in `knob.js` SVG output):**

| Element | Current value | New value |
|---|---|---|
| Arc track | `rgba(255,255,255,0.07)` | `rgba(255,255,255,0.06)` (barely-there) |
| Arc value stroke | `#fff` | `#fff` (unchanged) |
| Arc glow stroke | `rgba(255,255,255,0.3)` at blur σ=2 | `rgba(255,255,255,0.45)` at blur σ=2 |
| Inner OLED lip highlight | `rgba(255,255,255,0.22)` | `rgba(255,255,255,0.28)` |
| Outer OLED bezel ring | `rgba(0,0,0,0.80)` | `rgba(0,0,0,0.92)` |
| Label fill | `rgba(75,138,210,0.80)` (steel blue) | `#FFFFFF` |

All other knob colors unchanged.

---

## §3 — Typography

**Intent:** The label text changes from a tight monospaced uppercase to a warm handwritten script. The numeric value stays monospaced so it continues to read as a digital display.

**Implementation:**

- Add a `<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Kalam:wght@400;700&display=swap">` to `index.html` `<head>`. WebView2 fetches and caches this on first load.
- The `<style>` block in `knob.js`'s shadow DOM imports Kalam (shadow DOM does not inherit `@font-face` from the document; use `font-family: 'Kalam', 'Caveat', cursive` and ensure the font is loaded at document level — Chromium does propagate this).
- **Label text** (`.label` in OLED): `font-family: 'Kalam', cursive; font-weight: 400; color: #FFFFFF;` lowercase (e.g., `drive`, `output`, `threshold`).
- **Value text** (the numeric readout): unchanged — `'Courier New', monospace`, `font-weight: 700`, white with triple-stack glow.
- Section labels, preset names, harmonic labels, logos, and the center wheel OLED are **untouched** (scoped out in §Non-Goals).

---

## §4 — Arc Indicator

**Intent:** No structural change — only color (handled in §2).

- Sweep still starts at 135°, ends at 405° (270° total). Value 0.0 = no arc, 1.0 = full arc.
- Value change still re-renders the SVG `<path>` with updated `d` attribute.
- Glow layer (blurred, thick) rendered below the sharp arc layer, both in icy white per §2.

No code changes required beyond the color values already listed in §2.

---

## §5 — OLED Content Layout & Dynamic Resize

**Intent:** At rest, label and value are visually balanced — same size. While the user is dragging the knob, the value grows prominent so it's easy to read the number being dialed in. When the user releases, sizes ease back to balance.

**Resting state (OLED contents):**

- Two lines, vertically centered in the OLED well.
- **Top line:** numeric value (Courier New, bold, white with triple-stack glow).
- **Bottom line:** label (Kalam, 400, icy white, lowercase).
- Same `font-size` for both lines per the size tier table below.

**Dragging state (`pointerdown` → `pointerup`):**

- Value `font-size` grows to the "drag" size.
- Label `font-size` shrinks to the "drag" size.
- Transition: 150ms ease on `font-size`.

**Size table (font-size in px):**

| Knob size | Knob diameter | Rest (value + label) | Drag (value / label) |
|---|---|---|---|
| Small   | 56px  | 9 / 9   | 13 / 6 |
| Medium  | 88px  | 12 / 12 | 18 / 9 |
| Large   | 114px | 14 / 14 | 22 / 10 |

**Implementation notes:**

- Current `knob.js` re-renders the whole SVG on each value change via `_render()`. The drag/rest state needs to survive between renders.
- Add a private `_isDragging` flag on the component, toggled in `_onPointerDown` (set `true`) and `_onPointerUp` (set `false`).
- `_render()` picks the rest or drag font-sizes from the size tier based on `_isDragging`.
- SVG `<text>` elements don't animate `font-size` smoothly via CSS if re-rendered. To get the 150ms transition, either:
  - **Option A (recommended):** don't re-render the full SVG on pointer-up — instead, attribute-animate `font-size` on the existing `<text>` nodes. The render function still produces the initial DOM; pointer handlers patch `font-size` attributes with a CSS transition.
  - **Option B (simpler but abrupt):** skip the transition, just swap sizes on `pointerdown`/`pointerup`. The feel may be jarring; only fall back to this if Option A has issues with the existing render architecture.

Double-click reset behavior is unchanged and continues to work.

---

## §6 — Knob Size Adjustments

**Intent:** Raise the small-knob baseline so two lines of text render legibly under §5's layout.

| Tier | Before | After | Rationale |
|---|---|---|---|
| Small  | 40px  | **56px** | Kalam at <8px gets fuzzy; two stacked readable lines need 9px minimum. |
| Medium | 88px  | 88px    | Already comfortable. |
| Large  | 114px | 114px   | Already comfortable. |

**Cascade risk:** The harmonic row (`.harmonic-row` with 7 small knobs in a 340px left column) and the envelope/sidechain/filter panels pack small knobs tightly. After the resize, any overflowing row gets a targeted fix during implementation:

1. Reduce `.harmonic-row` `gap` from 1px to 0 and shrink panel padding.
2. If still overflowing, drop specific rows to a new `size="tiny"` tier (40px, keeping the current behavior) via CSS override on that row only.
3. As a last resort, re-flow the harmonic row into two columns of 3–4 knobs.

Decision per row deferred to implementation. Validation is visual — run the plugin, inspect the layout, adjust in the same implementation session.

---

## §7 — Acceptance Criteria

- Every `phantom-knob` in the UI renders with: icy-white arc, black OLED summit, volcano slope fading into bezel with no visible outer edge, halo glow in the bezel behind the knob, Kalam label, Courier New value.
- Label font-size equals value font-size at rest.
- On `pointerdown`, value grows and label shrinks per §5 size table, transitioning over ~150ms.
- On `pointerup`, sizes ease back to rest.
- No layout overflow in any panel; all knobs visible and clickable.
- No change to parameter wiring, DSP behavior, or the shape of the webview↔JUCE bridge.
- All 25 existing tests still pass (tests do not exercise UI; this is a sanity check).

---

## §8 — Out of Scope / Deferred

- Full theme extraction (dark mode, alternate color schemes) — the icy-white accent is hard-coded.
- Animating the halo with value (e.g., brighter halo at higher values). Static halo per §1.
- Customizable label text per knob via UI (labels are set in HTML attributes).
- Touch input tuning (the pointer-drag model already covers touch through Pointer Events).
