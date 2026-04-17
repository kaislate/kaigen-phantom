# Knob Icy Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign every `phantom-knob` in the WebView UI to match the icy-white reference mockup — OLED summit on a volcano slope that fades seamlessly into the bezel, ringed by an icy-white value arc, with a handwritten Kalam label.

**Architecture:** All changes are confined to three WebUI files: `Source/WebUI/knob.js` (SVG rendering + interaction), `Source/WebUI/styles.css` (layout / bezel), and `Source/WebUI/index.html` (font loading). DSP, parameter model, and the JUCE↔JS bridge are untouched. Changes are layered smallest-to-biggest: font load → color swap → gradient + drop-shadow → halo → size bump → dynamic drag-state sizing.

**Tech Stack:** JUCE 8.0.4 WebView2 (Chromium-based), HTML/CSS/JS (no framework), SVG via shadow DOM, Google Fonts, CMake build auto-copies VST3 to `%LOCALAPPDATA%/Programs/Common/VST3`.

**Spec reference:** `docs/superpowers/specs/2026-04-16-knob-icy-redesign-design.md`

**Verification approach:** No unit tests cover the WebUI (tests cover DSP only). Each task ends with a **build + visual check** — build the plugin and load it in Ableton (or open `Source/WebUI/index.html` in a browser with a stub for the JUCE bridge) to confirm the change landed visually. Do not claim a task complete without confirming the visual effect.

---

## File Structure

| File | Responsibility | Change type |
|---|---|---|
| `Source/WebUI/index.html` | Document shell, imports | Add Kalam `<link>` |
| `Source/WebUI/knob.js` | `phantom-knob` custom element — SVG rendering, pointer interaction, state | Main rework: colors, gradient, drop-shadow, border-radius/box-shadow halo, size tier, drag-state sizing |
| `Source/WebUI/styles.css` | Plugin layout & bezel | Adjust panel gaps for resized small knobs (only if overflow observed) |

---

## Build / Verify Reference

Used by most tasks:

**Build the plugin:**
```bash
cmake --build build --config Release --target KaigenPhantom_VST3
```
Expected: completes with no errors. VST3 is auto-copied to `%LOCALAPPDATA%/Programs/Common/VST3`.

**Optional — copy VST3 to Ableton test location (per project memory):**
```bash
cp "build/KaigenPhantom_artefacts/Release/VST3/Kaigen Phantom.vst3/Contents/x86_64-win/Kaigen Phantom.vst3" "/c/Users/kaislate/Downloads/KAIGEN/"
```

**Load the plugin in Ableton Live 12** (user runs this manually) to verify the visual change. Fully quit + restart Ableton if a prior build is cached.

**Quick browser preview (no DSP/bridge):** Open `Source/WebUI/index.html` directly in Chrome with DevTools. Knobs render with default values since JUCE bridge is absent; interaction and visuals are still accurate.

---

## Task 1: Add Kalam font to `index.html`

**Files:**
- Modify: `Source/WebUI/index.html:5`

- [ ] **Step 1: Add Google Fonts `<link>` for Kalam**

Replace the `<link>` block at line 5 with two links (Space Grotesk stays):

```html
<link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@300;400;500;600&display=swap" rel="stylesheet">
<link href="https://fonts.googleapis.com/css2?family=Kalam:wght@400;700&display=swap" rel="stylesheet">
<link rel="stylesheet" href="/styles.css">
```

- [ ] **Step 2: Build and confirm page still loads**

Run: `cmake --build build --config Release --target KaigenPhantom_VST3`
Expected: builds successfully, BinaryData bundles the updated `index.html`.

Open the plugin (or preview `index.html` in a browser) — UI still renders identically (no visual change yet because nothing uses Kalam yet). In DevTools → Network, Kalam requests should appear.

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/index.html
git commit -m "feat(ui): load Kalam font for knob labels"
```

---

## Task 2: Swap knob label to icy white Kalam, lowercase

**Files:**
- Modify: `Source/WebUI/knob.js:246-249` (label `<text>` element in `_render`)

**Context:** The label inside the OLED is currently rendered as:

```js
<text x="${cx}" y="${labelY}" text-anchor="middle" dominant-baseline="central"
  font-family="'Courier New',monospace" font-size="${labelFontSize}" font-weight="400"
  letter-spacing="1" fill="rgba(75,138,210,0.80)"
  style="text-transform:uppercase">${label.toUpperCase()}</text>
```

- [ ] **Step 1: Replace the label `<text>` element**

In `Source/WebUI/knob.js`, locate the label text (right above the template-literal close). Replace with:

```js
<text x="${cx}" y="${labelY}" text-anchor="middle" dominant-baseline="central"
  font-family="'Kalam', cursive" font-size="${labelFontSize}" font-weight="400"
  fill="#FFFFFF">${label.toLowerCase()}</text>
```

Changes: `font-family` → Kalam; `fill` → `#FFFFFF`; removed `letter-spacing="1"` (doesn't suit handwritten); removed `style="text-transform:uppercase"`; `${label.toUpperCase()}` → `${label.toLowerCase()}`.

- [ ] **Step 2: Build + visual verify**

Run: `cmake --build build --config Release --target KaigenPhantom_VST3`

Load in Ableton or browser. Every knob label (e.g. `saturation`, `strength`, `in`, `out`) should now read in lowercase white handwritten Kalam instead of uppercase blue Courier.

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/knob.js
git commit -m "feat(ui): swap knob labels to icy white Kalam lowercase"
```

---

## Task 3: Apply icy-white accent to remaining knob strokes

**Files:**
- Modify: `Source/WebUI/knob.js` — inside `_render`, the SVG template literal

**Context:** Per spec §2, five stroke values change. Arc value stroke (`#fff`) is already the target color (no change). Outer halo-ring stroke `rgba(255,255,255,0.10)` is not in the spec table — leave it unchanged.

- [ ] **Step 1: Update the inner OLED lip highlight**

Find:
```js
<circle cx="${cx}" cy="${cy}" r="${oledR}" fill="none"
  stroke="rgba(255,255,255,0.22)" stroke-width="1.5"/>
```
Change stroke to `rgba(255,255,255,0.28)`.

- [ ] **Step 2: Update the outer OLED bezel ring**

Find:
```js
<circle cx="${cx}" cy="${cy}" r="${oledR + 1.5}" fill="none"
  stroke="rgba(0,0,0,0.80)" stroke-width="1.5"/>
```
Change stroke to `rgba(0,0,0,0.92)`.

- [ ] **Step 3: Update the arc track stroke**

Find:
```js
<path d="${trackPath}" fill="none"
  stroke="rgba(255,255,255,0.07)" stroke-width="3.5" stroke-linecap="round"/>
```
Change stroke to `rgba(255,255,255,0.06)`.

- [ ] **Step 4: Update the arc glow stroke**

Find:
```js
<path d="${valPath}" fill="none"
  stroke="rgba(255,255,255,0.3)" stroke-width="6" stroke-linecap="round"
  filter="url(#glow-${sz})"/>
```
Change stroke to `rgba(255,255,255,0.45)`.

- [ ] **Step 5: Build + visual verify**

Run: `cmake --build build --config Release --target KaigenPhantom_VST3`

Load in Ableton or browser. Turn one knob from 0 → 1.0. The arc should sweep brighter icy white with a more pronounced soft glow beneath it. The OLED rim reads slightly crisper (tighter bezel line, brighter inner highlight).

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/knob.js
git commit -m "feat(ui): icy-white accent on knob arc and OLED rim"
```

---

## Task 4: Invert volcano gradient and remove drop-shadow

**Files:**
- Modify: `Source/WebUI/knob.js` — `:host` CSS in the template, and the `<radialGradient>` stops in `_render`

**Context:** Per spec §1. Today's gradient has focal point at `32%, 28%` (upper-left highlight) and darkens to `rgba(100,102,104,1)` at the outer edge — darker than the bezel, so the knob "grounds" against the background. We want the opposite: brightest near the OLED lip, fading to the exact bezel color at the outer edge, with no drop-shadow.

The panel background behind most knobs is `rgba(0,0,0,0.02)` over `#AEAFB1` ≈ `#ABACAE` → `rgba(171,172,174,1)`.

- [ ] **Step 1: Remove the `filter: drop-shadow(...)` from `:host`**

Find the `:host` block in the template literal near the top of `knob.js`:
```js
:host {
  display: inline-block;
  /* Light plastic: bright highlight top-left, grounded shadow bottom-right */
  filter: drop-shadow(-3px -4px 8px rgba(255,255,255,0.52)) drop-shadow(4px 5px 11px rgba(0,0,0,0.38));
  cursor: ns-resize;
  user-select: none;
  -webkit-user-select: none;
}
```
Replace with (no `filter` line, no grounded-shadow comment):
```js
:host {
  display: inline-block;
  cursor: ns-resize;
  user-select: none;
  -webkit-user-select: none;
}
```

- [ ] **Step 2: Replace the `<radialGradient>` stops**

Find the gradient in `_render`:
```js
<radialGradient id="vg-${sz}" cx="32%" cy="28%" r="72%" fx="32%" fy="28%">
  <!-- Plastic ring matches bezel — bright top-left, shadowed bottom-right -->
  <stop offset="0%"   stop-color="rgba(210,212,215,1)"/>
  <stop offset="32%"  stop-color="rgba(218,220,222,1)"/>
  <stop offset="50%"  stop-color="rgba(200,202,204,1)"/>
  <stop offset="65%"  stop-color="rgba(178,180,182,1)"/>
  <stop offset="80%"  stop-color="rgba(148,150,152,1)"/>
  <stop offset="100%" stop-color="rgba(100,102,104,1)"/>
</radialGradient>
```
Replace with:
```js
<radialGradient id="vg-${sz}" cx="50%" cy="50%" r="50%" fx="50%" fy="50%">
  <stop offset="0%"   stop-color="rgba(220,222,225,1)"/>
  <stop offset="70%"  stop-color="rgba(220,222,225,1)"/>
  <stop offset="85%"  stop-color="rgba(198,200,203,1)"/>
  <stop offset="100%" stop-color="rgba(171,172,174,1)"/>
</radialGradient>
```

This centers the gradient, keeps the inner 70% bright (hidden by the OLED anyway), then fades from bright to bezel color across the visible ring.

- [ ] **Step 3: Build + visual verify**

Run: `cmake --build build --config Release --target KaigenPhantom_VST3`

Load in Ableton or browser. Each knob's outer edge should now be invisible — the slope fades smoothly into the panel background. The OLED still reads as the summit, with the inner lip catching light. There should be **no dark outline** around the knob anymore.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/knob.js
git commit -m "feat(ui): invert knob volcano gradient, remove drop-shadow"
```

---

## Task 5: Add icy halo glow behind each knob

**Files:**
- Modify: `Source/WebUI/knob.js` — `:host` CSS in the template

**Context:** Per spec §1. The reference image shows a soft white radial glow behind the knob. Since `:host` is already sized to the knob diameter, we can paint the halo as a `box-shadow` (which renders outside the element's border) with `border-radius: 50%` to make the glow circular.

- [ ] **Step 1: Add halo to `:host`**

The `:host` block (after Task 4) currently reads:
```js
:host {
  display: inline-block;
  cursor: ns-resize;
  user-select: none;
  -webkit-user-select: none;
}
```
Replace with:
```js
:host {
  display: inline-block;
  border-radius: 50%;
  /* Icy halo — soft white radial glow behind the knob, blending into bezel */
  box-shadow:
    0 0 24px 6px rgba(255,255,255,0.22),
    0 0 48px 14px rgba(255,255,255,0.10);
  cursor: ns-resize;
  user-select: none;
  -webkit-user-select: none;
}
```

Two-layer shadow: a brighter inner glow for presence, a wider outer glow for the soft falloff.

- [ ] **Step 2: Build + visual verify**

Run: `cmake --build build --config Release --target KaigenPhantom_VST3`

Load in Ableton or browser. Each knob now has a visible white halo in the bezel around it, matching the reference mockup. Halos on adjacent knobs may overlap slightly — this is expected and reads as "light on plastic". If halos appear harsh or boxy, the `border-radius: 50%` is missing or the element is not square. Inspect in DevTools to confirm.

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/knob.js
git commit -m "feat(ui): add icy halo glow behind each knob"
```

---

## Task 6: Bump small-knob size 40→56px

**Files:**
- Modify: `Source/WebUI/knob.js:57-61` — `getSizeTier` function
- Modify: `Source/WebUI/knob.js:74-76` — `:host([size=...])` CSS rules

**Context:** Per spec §6. Kalam at under ~8px is fuzzy; small knobs need more room for the two-line layout. Small knobs appear in the envelope, filter, punch, stereo, and output panels.

- [ ] **Step 1: Update the size tier lookup**

Find:
```js
function getSizeTier(attr) {
  if (attr === 'large')  return { sz: 114, inset: 14 };
  if (attr === 'small')  return { sz: 40,  inset: 6  };
  return                        { sz: 88,  inset: 11 };  // 'medium' and default
}
```
Replace with:
```js
function getSizeTier(attr) {
  if (attr === 'large')  return { sz: 114, inset: 14 };
  if (attr === 'small')  return { sz: 56,  inset: 8  };
  return                        { sz: 88,  inset: 11 };  // 'medium' and default
}
```

- [ ] **Step 2: Update the `:host([size=...])` CSS rules**

Find:
```js
:host([size="large"])  { width: 114px; height: 114px; }
:host([size="medium"]) { width: 88px;  height: 88px;  }
:host([size="small"])  { width: 40px;  height: 40px;  }
```
Replace with:
```js
:host([size="large"])  { width: 114px; height: 114px; }
:host([size="medium"]) { width: 88px;  height: 88px;  }
:host([size="small"])  { width: 56px;  height: 56px;  }
```

- [ ] **Step 3: Build + layout verify**

Run: `cmake --build build --config Release --target KaigenPhantom_VST3`

Load in Ableton or browser. No panel in the current layout uses `size="small"` knobs visibly (the H2-H8 recipe knobs use `size="small"` but are `display:none`). Confirm by walking every panel:
- Ghost panel: large + medium
- Harmonic: large + medium × 10
- Stereo: large
- Envelope: medium × 2
- Filter: medium × 2
- Punch: medium × 3
- Output: medium + large
- Settings overlay (Binaural Width): medium
- Recipe H2–H8: small (hidden)

If a future panel uses `size="small"` and overflows its row, reduce the panel's internal `gap` in `styles.css` (e.g. `.knob-row { gap: 3px; }`). None is expected to be needed by this task.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/knob.js
git commit -m "feat(ui): bump small knob size from 40 to 56px"
```

---

## Task 7: Dynamic drag-state OLED sizing

**Files:**
- Modify: `Source/WebUI/knob.js` — add persistent-DOM rendering + drag state

**Context:** Per spec §5. At rest, value and label font-sizes are equal. During a drag, the value grows and the label shrinks, with a 150ms transition. On release, sizes ease back to equal.

The current `_render()` replaces the entire SVG with `svg.innerHTML = '...'` on every value change. This destroys the text nodes and prevents CSS transitions from animating `font-size`. We fix this by:
1. Building the SVG structure **once** on connect (static nodes: gradient defs, volcano circle, OLED circle, lip rings, arc track, arc glow, arc value, value text ×3, label text).
2. On each value change, updating only the dynamic attributes: the two arc `<path>` `d` attributes, the four text content strings.
3. Toggling a `data-dragging="true|false"` attribute on the SVG element; CSS rules in the shadow stylesheet select on that attribute to choose font-sizes, with `transition: font-size 150ms ease` on all text elements.

Size table from spec §5 (updated for new small=56):

| Size | Rest (value+label) | Drag (value / label) |
|---|---|---|
| Small  | 9 / 9   | 13 / 6 |
| Medium | 12 / 12 | 18 / 9 |
| Large  | 14 / 14 | 22 / 10 |

- [ ] **Step 1: Extend the shadow-DOM stylesheet with font-size CSS rules**

In the template literal near the top of `knob.js`, the `<style>` block currently ends like this:
```js
:host([size="large"])  { width: 114px; height: 114px; }
:host([size="medium"]) { width: 88px;  height: 88px;  }
:host([size="small"])  { width: 56px;  height: 56px;  }
svg { display: block; width: 100%; height: 100%; }
.label-below { display: none; }
```
Add the following rules **immediately after** `svg { ... }`:
```js
/* Transition font-size between rest and drag states */
.value-text, .label-text { transition: font-size 150ms ease; }

/* Rest sizes (default) */
:host([size="small"])  .value-text,
:host([size="small"])  .label-text { font-size: 9px; }
:host([size="medium"]) .value-text,
:host([size="medium"]) .label-text { font-size: 12px; }
:host([size="large"])  .value-text,
:host([size="large"])  .label-text { font-size: 14px; }
:host(:not([size="small"]):not([size="medium"]):not([size="large"])) .value-text,
:host(:not([size="small"]):not([size="medium"]):not([size="large"])) .label-text { font-size: 12px; }

/* Drag sizes (when svg has data-dragging="true") */
:host([size="small"])  svg[data-dragging="true"] .value-text { font-size: 13px; }
:host([size="small"])  svg[data-dragging="true"] .label-text { font-size: 6px; }
:host([size="medium"]) svg[data-dragging="true"] .value-text { font-size: 18px; }
:host([size="medium"]) svg[data-dragging="true"] .label-text { font-size: 9px; }
:host([size="large"])  svg[data-dragging="true"] .value-text { font-size: 22px; }
:host([size="large"])  svg[data-dragging="true"] .label-text { font-size: 10px; }
:host(:not([size="small"]):not([size="medium"]):not([size="large"])) svg[data-dragging="true"] .value-text { font-size: 18px; }
:host(:not([size="small"]):not([size="medium"]):not([size="large"])) svg[data-dragging="true"] .label-text { font-size: 9px; }
```

The `:host(:not(...))` rules cover the default (medium) case for knobs that don't specify a `size` attribute — matches current fallback behavior in `getSizeTier`.

- [ ] **Step 2: Add a `_isDragging` field and initialize in constructor**

In the class constructor, after `this._dragging = false;`, add:
```js
this._isDragging = false;
```

(Note: `_dragging` is the existing pointer-captured flag; `_isDragging` is the visual state. They're conceptually the same but we use a separate name to leave room for future tweaks like "only enter drag-state after 80ms of sustained dragging" if the snap feels jumpy.)

- [ ] **Step 3: Set and re-render on pointerdown/up**

In `_onPointerDown`, after `this._dragging = true;` add:
```js
this._isDragging = true;
this._updateDragState();
```

In `_onPointerUp`, after `this._dragging = false;` add:
```js
this._isDragging = false;
this._updateDragState();
```

- [ ] **Step 4: Add `_updateDragState` method**

Inside the class, add a new method (place it above `_render`):
```js
_updateDragState() {
  if (this._svg) this._svg.setAttribute('data-dragging', this._isDragging ? 'true' : 'false');
}
```

- [ ] **Step 5: Remove `font-size` attributes from the text elements in `_render`**

Because CSS now controls `font-size`, the inline SVG attributes would override our CSS. Remove them from the six `<text>` elements in `_render` (three value-text glow layers and one label-text, plus the two waveform-mode value-text copies). Also add the `class` attribute so the CSS selectors match.

**Non-waveform branch — value text triple glow (three elements):** Change each from:
```js
<text x="${cx}" y="${valueY}" text-anchor="middle" dominant-baseline="central"
  font-family="'Courier New',monospace" font-weight="700" font-size="${valueFontSize}"
  fill="#fff" opacity="0.3">${displayText}</text>
```
to:
```js
<text class="value-text" x="${cx}" y="${valueY}" text-anchor="middle" dominant-baseline="central"
  font-family="'Courier New',monospace" font-weight="700"
  fill="#fff" opacity="0.3">${displayText}</text>
```
(Remove `font-size="${valueFontSize}"`, add `class="value-text"`. Apply the same change to the opacity-0.6 and opacity-1 copies.)

**Non-waveform branch — label text:** Change from (the result of Task 2):
```js
<text x="${cx}" y="${labelY}" text-anchor="middle" dominant-baseline="central"
  font-family="'Kalam', cursive" font-size="${labelFontSize}" font-weight="400"
  fill="#FFFFFF">${label.toLowerCase()}</text>
```
to:
```js
<text class="label-text" x="${cx}" y="${labelY}" text-anchor="middle" dominant-baseline="central"
  font-family="'Kalam', cursive" font-weight="400"
  fill="#FFFFFF">${label.toLowerCase()}</text>
```

**Waveform branch — the three waveform-mode value text elements** (they appear when `data-oled="waveform"` is set, which is used by the `synth_step` knob in index.html). Apply the same transform: remove `font-size="${waveNumFontSize}"` and add `class="value-text"` to each of the three stacked `<text>` elements. This keeps the waveform knob's number-display consistent.

- [ ] **Step 6: Delete the now-unused `valueFontSize`, `labelFontSize`, and `waveNumFontSize` locals**

In `_render`, these lines (near the size-tier lookup) are no longer referenced:
```js
const valueFontSize = isLarge ? 17 : 12;
const labelFontSize = isLarge ? 7  : 6;
const waveNumFontSize = isLarge ? 10 : 8;
```
Delete all three lines. The compiler (well, WebView JS engine) will not error on unused locals, but dead code invites drift.

- [ ] **Step 7: Ensure `_render` writes the drag-state attribute**

`_updateDragState` sets the attribute on the persistent `this._svg` node. But `_render` replaces `this._svg.innerHTML` — that replaces the _children_, not the attributes. The attribute persists across renders. No change needed; this step is a verification that the logic holds. Confirm by reading the code flow: `setAttribute('data-dragging', ...)` on `this._svg` survives `innerHTML = ...` calls because `innerHTML` only affects descendants.

- [ ] **Step 8: Build + visual verify**

Run: `cmake --build build --config Release --target KaigenPhantom_VST3`

Load in Ableton or browser. For each size tier:
1. At rest, the value (e.g. `0.50`) and the label (e.g. `strength`) are the same size.
2. While dragging, the value grows prominent and the label shrinks, transitioning over ~150ms.
3. On release, both return to equal size, transitioning over ~150ms.
4. The transition is smooth (no snap, no layout jump).

If the transition doesn't play, confirm with DevTools that the `svg` element gains `data-dragging="true"` on pointerdown and that the CSS rules match.

- [ ] **Step 9: Commit**

```bash
git add Source/WebUI/knob.js
git commit -m "feat(ui): dynamic knob OLED sizing while dragging"
```

---

## Task 8: Final acceptance pass

**Files:**
- No file changes unless a regression is found.

- [ ] **Step 1: Walk the acceptance criteria from spec §7**

Load the plugin in Ableton (fully quit Ableton first to bust VST3 cache). Verify each criterion:

1. Every knob renders with icy-white arc, black OLED summit, volcano slope fading to bezel, halo glow behind, Kalam label, Courier value — yes/no per panel.
2. Label size == value size at rest.
3. On pointerdown, value grows and label shrinks per size table, transitioning ~150ms.
4. On pointerup, sizes ease back.
5. No layout overflow in any panel; every knob visible and clickable.
6. No DSP regression; run an audio signal through the plugin and confirm controls still take effect.
7. All existing tests still pass.

- [ ] **Step 2: Run the test suite**

Run: `cmake --build build --config Release --target PhantomTests && ctest --test-dir build -C Release`
Expected: 25/25 test cases pass, 2761+ assertions.

- [ ] **Step 3: Copy to Ableton test location**

Run:
```bash
cp -r "build/KaigenPhantom_artefacts/Release/VST3/Kaigen Phantom.vst3" "/c/Users/kaislate/Downloads/KAIGEN/"
```
(Optional — user's memory notes this as their Ableton deploy step.)

- [ ] **Step 4: If regressions exist, fix and commit per issue**

Any regression is its own task: isolate it, fix `knob.js` / `styles.css` / `index.html`, verify, commit with message starting `fix(ui): ...`.

- [ ] **Step 5: No-op commit if everything passed**

If nothing needed fixing, no commit is needed for Task 8 — it's a verification step. Leave the branch at the last implementation commit (Task 7).

---

## Self-Review (inline for plan author)

**Spec coverage check:**
- §1 (seamless volcano) — covered by Tasks 4 + 5.
- §2 (icy-white accent) — covered by Tasks 2 + 3.
- §3 (typography) — covered by Tasks 1 + 2.
- §4 (arc indicator — color only) — covered by Task 3.
- §5 (dynamic OLED sizing) — covered by Task 7.
- §6 (small knob size bump) — covered by Task 6.
- §7 (acceptance) — covered by Task 8.
- §8 (out of scope) — no tasks, correct.

**Placeholder scan:** No TBDs, no "handle edge cases", no "similar to Task N" without the code shown. Every code change shows both before and after.

**Type/name consistency:** `_isDragging` defined in Task 7 Step 2, used in Steps 3–4. `_updateDragState` defined in Step 4, called in Step 3. `class="value-text"` and `class="label-text"` used in Step 1 CSS and Step 5 SVG. Consistent.

**Ambiguity:** Task 7 Step 5 explicitly covers the waveform-branch text elements, which are easy to miss since they live in a different template literal branch. All six text elements enumerated.
