# PR3b-rev: Matrix View Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the drawer + per-macro editor with a Vital/Roar-style matrix view: modulators as rows with live animations, destinations as columns grouped into expandable categories, click intersections to add/edit/remove routings.

**Architecture:** Two sibling views (`SLOTS` and `MATRIX`) inside the bottom modulation panel, toggled by a segmented control. C++ modulation framework is unchanged — same native bindings (`modulationGetState`, `modulationAddRouting`, `modulationRemoveRouting`, `modulationSetRoutingDepth`, `modulationSetMacroName`, `modulationGetLiveState`). New JS modules: `live-modulation.js` (15 Hz poll, single source of live values), `matrix.js` + `matrix.css` (grid view). Drawer + `macro-editor.js` are deleted at the end. New `<MatrixView>` element persists in plugin state alongside `<EditorFocus>` and `<SpectrumView>`.

**Tech Stack:** JUCE 8.0.4, C++20 (MSVC 17.14), WebView2, vanilla JS (no framework), CSS (custom-property-driven), Catch2 v3.5.2 for the C++ persistence helper test.

**Branch:** `feature/pr3b-macro-editor` (continues — no new branch).

**Spec:** `docs/superpowers/specs/2026-05-06-pr3b-rev-matrix-view-design.md`.

---

## File Structure

| File | Status | Responsibility |
|------|--------|----------------|
| `Source/MatrixViewState.h` | **Create** | Header-only `MatrixViewState` struct + `<MatrixView>` ValueTree round-trip. Mirrors `EngineFocus.h` / `SpectrumViewMode.h` pattern. |
| `Source/PluginProcessor.cpp` | **Modify** | Add `MatrixViewState matrixView{}` member; write/read in `getStateInformation` / `setStateInformation`. Add `matrixGetState` / `matrixSetState` native bindings. |
| `Source/PluginProcessor.h` | **Modify** | Add `MatrixViewState matrixView` member, `getMatrixView()` / `setMatrixView()` accessors. |
| `Source/PluginEditor.cpp` | **Modify** | Add `matrixGetState` / `matrixSetState` `addNativeFunction` registrations. Remove the `<script src="/macro-editor.js">` reference path is in `index.html`, not here — verify only. |
| `Source/WebUI/index.html` | **Modify** | Remove `#modulation-drawer`. Add SLOTS/MATRIX toggle + matrix container (`#modulation-matrix`, hidden). Replace static `.mod-slot-dot` markup with type-specific live elements. Add `<script>` tags for `live-modulation.js` and `matrix.js`. Remove `<script src="/macro-editor.js">`. |
| `Source/WebUI/styles.css` | **Modify** | Remove drawer styles. Add SLOTS/MATRIX toggle styles. Update slot-dot styles to host live elements (conic ring for macro/morph, mini-waveform for LFO, scatter for random). |
| `Source/WebUI/matrix.css` | **Create** | All matrix-view styles: row layout, cells, category headers, A/B divider, popover, name editor. Loaded via its own `<link>` to keep the diff focused. |
| `Source/WebUI/live-modulation.js` | **Create** | One module that owns the 15 Hz `modulationGetLiveState` poll. Pauses on `document.visibilitychange`. Dispatches `'kaigen:live-state'` CustomEvent on `document`. Both slots and matrix views subscribe. |
| `Source/WebUI/modulation-panel.js` | **Modify** | Reduce: remove drawer mechanics + macro-editor invocation. Keep engine-focus auto-switch on slot click + the SLOTS/MATRIX toggle handler + slot live-animation subscription. |
| `Source/WebUI/matrix.js` | **Create** | Matrix view controller: render grid from `modulationGetState`, wire cell click/drag/right-click, drive cell live-modulating glow from `kaigen:live-state` events, manage category expansion state via `matrixGetState`/`matrixSetState`. Splits into helpers if it grows past ~500 lines (reasonable splits: `matrix-render.js`, `matrix-interactions.js`). |
| `Source/WebUI/macro-editor.js` | **Delete** | (Final task.) |
| `Source/WebUI/preset-system.js` | **No change required** | Plugin state persistence happens in `PluginProcessor.cpp`. The matrix's expansion state is plugin-level, not preset-level. Verify only. |
| `Source/Modulation/*` | **No change** | C++ framework is stable. |
| `CMakeLists.txt` | **Modify** | Remove `Source/WebUI/macro-editor.js` from `juce_add_binary_data`. Add `Source/WebUI/matrix.js`, `Source/WebUI/matrix.css`, `Source/WebUI/live-modulation.js`. |
| `tests/MatrixViewStatePersistenceTest.cpp` | **Create** | Catch2 round-trip test for `MatrixViewState` ValueTree serialization. |
| `tests/CMakeLists.txt` | **Modify** | Add the new test source to the test target. |
| `docs/superpowers/plans/2026-05-05-pr3b-macro-editor.md` | **Modify (1 line)** | Add a top-of-doc note linking to this plan. |

### Column abbreviation table (used by Tasks 5 and 6)

The matrix shows ~31 destination columns per engine. To keep cells readable, full param names are abbreviated to 5–6 chars:

| Category | Param ID leaf | Column label | Group order |
|----------|---------------|--------------|-------------|
| GHOST | ghost | GHOST | 1 |
| GHOST | phantom_threshold | PHTHR | 2 |
| GHOST | phantom_strength | PSTR | 3 |
| GHOST | output_gain | OUT | 4 |
| RECIPE | recipe_h2 | H2 | 5 |
| RECIPE | recipe_h3 | H3 | 6 |
| RECIPE | recipe_h4 | H4 | 7 |
| RECIPE | recipe_h5 | H5 | 8 |
| RECIPE | recipe_h6 | H6 | 9 |
| RECIPE | recipe_h7 | H7 | 10 |
| RECIPE | recipe_h8 | H8 | 11 |
| RECIPE | harmonic_saturation | HSAT | 12 |
| SHAPE | synth_step | STEP | 13 |
| SHAPE | synth_duty | DUTY | 14 |
| SHAPE | synth_skip | SKIP | 15 |
| ENVELOPE | env_attack_ms | ATK | 16 |
| ENVELOPE | env_release_ms | REL | 17 |
| FILTER | synth_lpf_hz | LPF | 18 |
| FILTER | synth_hpf_hz | HPF | 19 |
| RESYN | synth_wavelet_length | WVLEN | 20 |
| RESYN | synth_gate_threshold | GATE | 21 |
| RESYN | synth_h1 | H1 | 22 |
| RESYN | synth_sub | SUB | 23 |
| PITCH | synth_min_samples | MINSP | 24 |
| PITCH | synth_max_samples | MAXSP | 25 |
| PITCH | tracking_speed | TRACK | 26 |
| PITCH | punch_amount | PUNCH | 27 |
| PITCH | synth_boost_threshold | BTHR | 28 |
| PITCH | synth_boost_amount | BAMT | 29 |
| STEREO | binaural_width | BIN | 30 |
| STEREO | stereo_width | WIDTH | 31 |

Hover tooltip shows the full prefixed param ID (e.g. `a_phantom_threshold`).

Excluded from columns (choice/bool, not modulation-eligible): `mode`, `ghost_mode`, `binaural_mode`, `env_source`, `midi_trigger_enabled`, `punch_enabled`, `recipe_preset`, `synth_filter_slope`.

---

## Task 1: HTML scaffold + CMakeLists registration

Adds the SLOTS/MATRIX toggle, the empty matrix container, and the new live-element placeholders inside slots — but no behavior yet. Drawer is left in place; we'll delete it at the end. Registers the new JS/CSS files in CMake so they're available as binary data, even though they're empty stubs at this point.

**Files:**
- Create: `Source/WebUI/live-modulation.js` (empty stub)
- Create: `Source/WebUI/matrix.js` (empty stub)
- Create: `Source/WebUI/matrix.css` (empty stub)
- Modify: `Source/WebUI/index.html` (add toggle + matrix container; replace dot markup with live containers; add `<script>`/`<link>` tags)
- Modify: `Source/WebUI/styles.css` (add toggle styles)
- Modify: `CMakeLists.txt:60-76` (register new files)

- [ ] **Step 1: Create empty stub files**

```bash
# matrix.js
cat > "Source/WebUI/matrix.js" <<'JS'
// Source/WebUI/matrix.js — matrix view controller. See plan Task 4+.
(function () { 'use strict'; })();
JS

# matrix.css
cat > "Source/WebUI/matrix.css" <<'CSS'
/* Source/WebUI/matrix.css — matrix view styles. Populated in Task 4+. */
CSS

# live-modulation.js
cat > "Source/WebUI/live-modulation.js" <<'JS'
// Source/WebUI/live-modulation.js — 15 Hz poll of modulationGetLiveState.
// Populated in Task 2.
(function () { 'use strict'; })();
JS
```

- [ ] **Step 2: Register the three files in CMakeLists.txt**

Open `CMakeLists.txt`. Find the `juce_add_binary_data(PhantomWebUI SOURCES ...)` block (lines 60–76). Replace it with:

```cmake
juce_add_binary_data(PhantomWebUI SOURCES
    Source/WebUI/index.html
    Source/WebUI/styles.css
    Source/WebUI/matrix.css
    Source/WebUI/knob.js
    Source/WebUI/knob-mini.js
    Source/WebUI/phantom.js
    Source/WebUI/spectrum.js
    Source/WebUI/recipe-wheel.js
    Source/WebUI/oscilloscope.js
    Source/WebUI/circuit-board.js
    Source/WebUI/juce-frontend.js
    Source/WebUI/preset-spectrum.js
    Source/WebUI/preset-system.js
    Source/WebUI/morph.js
    Source/WebUI/modulation-panel.js
    Source/WebUI/macro-editor.js
    Source/WebUI/live-modulation.js
    Source/WebUI/matrix.js
)
```

(Keep `macro-editor.js`, `knob-mini.js`, `recipe-wheel.js`, and `circuit-board.js` — those are still in use. Only `macro-editor.js` is removed in Task 14.)

- [ ] **Step 3: Add SLOTS/MATRIX toggle + matrix container to `index.html`**

Open `Source/WebUI/index.html`. Find the modulation panel block (line 340). Replace:

```html
    <div id="modulation-panel" class="modulation-panel" aria-label="Modulation panel">
      <div id="modulation-drawer" class="modulation-drawer" aria-hidden="true"></div>
```

with:

```html
    <div id="modulation-panel" class="modulation-panel" aria-label="Modulation panel">

      <div class="modulation-mode-bar">
        <div class="modulation-mode-toggle" role="tablist" aria-label="Modulation view mode">
          <button class="mode-btn is-active" data-mode="slots"  role="tab" aria-selected="true">SLOTS</button>
          <button class="mode-btn"           data-mode="matrix" role="tab" aria-selected="false">MATRIX</button>
        </div>
        <div id="modulation-counter" class="modulation-counter">0 ROUTINGS</div>
      </div>

      <div id="modulation-drawer" class="modulation-drawer" aria-hidden="true"></div>

      <div id="modulation-matrix" class="modulation-matrix" aria-hidden="true"></div>
```

- [ ] **Step 4: Replace the static `.mod-slot-dot` markup with type-specific live containers**

Inside `index.html`, the 11 slot buttons currently look like:

```html
<button class="mod-slot mod-slot-lfo" data-slot-type="lfo" data-slot-id="lfo1" disabled ...>
  <span class="mod-slot-dot"></span>
  <span class="mod-slot-name">LFO 1</span>
</button>
```

For each slot button (lfo1, lfo2, randomA, macro1, macro2, morph, macro3, macro4, randomB, lfo3, lfo4), replace the `<span class="mod-slot-dot"></span>` line with a type-specific stub. Macro and morph slots get a conic-ring container, LFO slots get an SVG placeholder, random slots get a 3×3 dot grid:

| Slot type | Replacement markup |
|-----------|-------------------|
| `mod-slot-macro` | `<span class="mod-slot-dot"><span class="mod-slot-ring" style="--v: 0;"></span></span>` |
| `mod-slot-morph` | `<span class="mod-slot-dot"><span class="mod-slot-ring" style="--v: 0;"></span></span>` |
| `mod-slot-lfo` | `<span class="mod-slot-dot"><svg class="mod-slot-lfo-svg" viewBox="0 0 24 14" preserveAspectRatio="none"><path d="M0,7 Q3,0 6,7 T12,7 T18,7 T24,7" fill="none" stroke="currentColor" stroke-width="1.2"/></svg></span>` |
| `mod-slot-random` | `<span class="mod-slot-dot"><span class="mod-slot-scatter"><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i></span></span>` |

The outer `.mod-slot-dot` keeps its existing styles (size, glow, type-specific border/color); the inner element renders the live state.

- [ ] **Step 5: Add `<script>` and `<link>` tags**

Open `Source/WebUI/index.html`. Find the existing `<link rel="stylesheet" href="/styles.css">` near the top. Add immediately after it:

```html
<link rel="stylesheet" href="/matrix.css">
```

Find the existing script block at the bottom:

```html
<script src="/modulation-panel.js"></script>
<script src="/macro-editor.js"></script>
```

Replace with:

```html
<script src="/live-modulation.js"></script>
<script src="/modulation-panel.js"></script>
<script src="/macro-editor.js"></script>
<script src="/matrix.js"></script>
```

(`macro-editor.js` stays until Task 14.)

- [ ] **Step 6: Add SLOTS/MATRIX toggle CSS**

Open `Source/WebUI/styles.css`. Find the `/* ═══ BOTTOM MODULATION PANEL (PR3b) ═══ */` block (~line 722). Insert the toggle styles immediately after the `.modulation-panel` rule:

```css
.modulation-mode-bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 6px 12px 0;
}
.modulation-mode-toggle {
  display: inline-flex;
  gap: 2px;
  background: rgba(20, 24, 30, 0.5);
  padding: 2px;
  border-radius: 4px;
}
.mode-btn {
  background: transparent;
  border: none;
  color: rgba(255, 255, 255, 0.4);
  font: 600 9px/1 'Space Grotesk', sans-serif;
  letter-spacing: 1.5px;
  padding: 5px 12px;
  cursor: pointer;
  border-radius: 3px;
}
.mode-btn:hover { color: rgba(255, 255, 255, 0.65); }
.mode-btn.is-active {
  background: rgba(93, 211, 224, 0.18);
  color: #5DD3E0;
  box-shadow: 0 0 6px rgba(93, 211, 224, 0.25);
}
.modulation-counter {
  font: 600 9px/1 'Space Grotesk', sans-serif;
  letter-spacing: 2px;
  color: rgba(255, 255, 255, 0.35);
}

.modulation-matrix {
  display: none;
  padding: 8px 12px 12px;
}
.modulation-matrix.is-open {
  display: block;
}
```

- [ ] **Step 7: Build to verify the scaffold loads**

Run: `build.bat` (Standalone target).
Expected: build succeeds; opening the Standalone shows the SLOTS/MATRIX toggle above the existing slot row; clicking MATRIX has no effect yet (matrix container is empty and hidden); existing drawer behavior on macro slots still works.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt \
        Source/WebUI/index.html \
        Source/WebUI/styles.css \
        Source/WebUI/matrix.css \
        Source/WebUI/matrix.js \
        Source/WebUI/live-modulation.js
git commit -m "feat: matrix-view scaffold (SLOTS/MATRIX toggle, stubs, CMake reg)"
```

---

## Task 2: Live macro/morph ring animations in slots view

Establishes the 15 Hz `modulationGetLiveState` poll inside `live-modulation.js`. Wires it to the macro and morph slot rings via a CSS custom property update. LFO and Random slots stay static placeholders. This is a visible UX improvement on its own — independently shippable.

**Files:**
- Modify: `Source/WebUI/live-modulation.js`
- Modify: `Source/WebUI/styles.css` (add ring/lfo-svg/scatter styles)
- Modify: `Source/WebUI/modulation-panel.js` (subscribe macro/morph slots to live-state events)

- [ ] **Step 1: Implement the live-state poll in `live-modulation.js`**

Replace the contents of `Source/WebUI/live-modulation.js` with:

```js
// Source/WebUI/live-modulation.js
//
// Polls modulationGetLiveState at 15 Hz and dispatches a 'kaigen:live-state'
// CustomEvent on document.body. Both the slots view (macro/morph rings) and
// the matrix view (per-cell live-modulating glow) subscribe to this single
// source rather than each running their own poll.

(function () {
  'use strict';

  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    let getLiveState = null;
    try { getLiveState = window.Juce.getNativeFunction('modulationGetLiveState'); }
    catch (e) { console.warn('[live-modulation] binding unavailable', e); return; }
    if (!getLiveState) return;

    const POLL_MS = 1000 / 15;
    let timer = null;

    function tick() {
      try {
        const state = getLiveState();
        document.body.dispatchEvent(new CustomEvent('kaigen:live-state', { detail: state }));
      } catch (e) {
        // Binding gone (editor teardown). Stop polling silently.
        clearInterval(timer);
        timer = null;
      }
    }

    function start() {
      if (timer) return;
      timer = setInterval(tick, POLL_MS);
    }
    function stop() {
      if (!timer) return;
      clearInterval(timer);
      timer = null;
    }

    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') start();
      else stop();
    });

    start();
  }
})();
```

- [ ] **Step 2: Add CSS for the ring + placeholder elements**

Open `Source/WebUI/styles.css`. Find the `.mod-slot-dot` rule (~line 786). Insert after it:

```css
/* Conic-gradient ring driven by --v (0–100). Used by macro + morph slots. */
.mod-slot-ring {
  position: absolute;
  inset: 4px;
  border-radius: 50%;
  background: conic-gradient(currentColor calc(var(--v, 0) * 1%), rgba(255,255,255,0.08) 0);
}
.mod-slot-ring::after {
  content: '';
  position: absolute;
  inset: 4px;
  border-radius: 50%;
  background: #0a0c10;
}
.mod-slot-dot {
  position: relative;            /* anchor for the absolutely-positioned ring */
  overflow: hidden;
}
.mod-slot-macro .mod-slot-ring { color: #5DD3E0; }
.mod-slot-morph .mod-slot-ring { color: #EFEFF2; }

/* LFO mini-waveform placeholder (greyed until PR4 wires live data). */
.mod-slot-lfo-svg {
  width: 24px;
  height: 14px;
  position: absolute;
  inset: 50% 50%;
  transform: translate(-50%, -50%);
  color: #4A90E2;
}

/* Random scatter placeholder (greyed until PR5 wires live data). */
.mod-slot-scatter {
  position: absolute;
  inset: 8px;
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  grid-template-rows: repeat(3, 1fr);
  gap: 1px;
}
.mod-slot-scatter i {
  background: #6B5DC9;
  opacity: 0.3;
  border-radius: 50%;
}
.mod-slot-scatter i:nth-child(1) { opacity: 0.8; }
.mod-slot-scatter i:nth-child(5) { opacity: 0.6; }
.mod-slot-scatter i:nth-child(7) { opacity: 0.9; }
```

The macro/morph slot dot keeps its existing `radial-gradient` background as a base layer; the conic ring is overlaid.

- [ ] **Step 3: Subscribe macro/morph slots to live-state events**

Open `Source/WebUI/modulation-panel.js`. Find the `init()` function (~line 17). At the **end** of `init()` (after the `panel.querySelectorAll('.mod-slot').forEach(...)` block), insert:

```js
    // Live ring updates for macro + morph slots. Each event carries
    // { macros: { macro1: { value }, ... }, morph_amount: <number> } from the
    // C++ modulationGetLiveState binding. Per the spec, only macro and morph
    // get live rings in this PR; LFO + Random rings are PR4/PR5.
    document.body.addEventListener('kaigen:live-state', (ev) => {
      const detail = ev.detail || {};
      const macros = detail.macros || {};
      for (const id of ['macro1', 'macro2', 'macro3', 'macro4']) {
        const slot = panel.querySelector(`.mod-slot[data-slot-id="${id}"] .mod-slot-ring`);
        if (!slot) continue;
        const v = (macros[id] && typeof macros[id].value === 'number') ? macros[id].value : 0;
        slot.style.setProperty('--v', String(Math.max(0, Math.min(1, v)) * 100));
      }
      const morphSlot = panel.querySelector('.mod-slot[data-slot-id="morph"] .mod-slot-ring');
      if (morphSlot) {
        const m = (typeof detail.morph_amount === 'number') ? detail.morph_amount : 0;
        morphSlot.style.setProperty('--v', String(Math.max(0, Math.min(1, m)) * 100));
      }
    });
```

- [ ] **Step 4: Verify `modulationGetLiveState` returns macros + morph_amount**

The current binding in `Source/PluginEditor.cpp` should already include macros 1–4. Open it and find the `addNativeFunction` for `modulationGetLiveState`. Confirm the returned JSON includes a top-level `macros` object keyed by `macro1`–`macro4` with `value`, and a top-level `morph_amount` numeric field. If `morph_amount` is missing, add it from `apvts.getRawParameterValue("morph_amount")->load()` (read-only, RT-safe) — this is a one-line addition to the existing binding's lambda body. **If you have to add it, include the change in this task's commit; do not split into a separate PR.**

- [ ] **Step 5: Build and visually verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, drag macro 1 in slots view (its slot is clickable but you'll need to drag the macro from the drawer for now since the matrix isn't wired yet — alternatively, automate macro 1 from Live or use any DAW's parameter window). Verify the macro 1 slot's ring fills proportionally to the macro value at 15 Hz. Move the morph knob — the morph slot's ring fills to match. LFO and random slots stay static (greyed placeholders).

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/live-modulation.js \
        Source/WebUI/styles.css \
        Source/WebUI/modulation-panel.js \
        Source/PluginEditor.cpp  # if morph_amount was added in step 4
git commit -m "feat: live ring animations on macro + morph slots (15 Hz poll)"
```

---

## Task 3: SLOTS/MATRIX toggle wires visibility flip

Adds the toggle handler in `modulation-panel.js`. Clicking MATRIX hides the slot row + drawer and shows the empty matrix container (and grows the editor); clicking SLOTS reverses. No grid contents yet.

**Files:**
- Modify: `Source/WebUI/modulation-panel.js`
- Modify: `Source/WebUI/styles.css` (slot-row hidden in matrix mode)

- [ ] **Step 1: Add CSS for hiding the slot row in matrix mode**

In `Source/WebUI/styles.css`, find the `.modulation-row` rule (~line 740). After it, add:

```css
.modulation-panel.is-matrix-mode .modulation-row,
.modulation-panel.is-matrix-mode .modulation-row-labels,
.modulation-panel.is-matrix-mode .modulation-drawer {
  display: none;
}
```

- [ ] **Step 2: Add toggle handler in `modulation-panel.js`**

Open `Source/WebUI/modulation-panel.js`. At the top of `init()` after `if (!panel || !drawer) return;`, add:

```js
    // ── SLOTS / MATRIX mode toggle ────────────────────────────────────────
    const matrix = document.getElementById('modulation-matrix');
    const modeBar = panel.querySelector('.modulation-mode-toggle');
    const BASE_HEIGHT     = 820;
    const MATRIX_EXPANDED = BASE_HEIGHT + 320;   // matrix needs more vertical room than drawer
    let setEditorHeight = null;
    try { setEditorHeight = window.Juce.getNativeFunction('setEditorHeight'); }
    catch (e) {}

    let currentMode = 'slots';
    function applyMode(mode) {
      currentMode = mode;
      panel.classList.toggle('is-matrix-mode', mode === 'matrix');
      if (matrix) {
        matrix.classList.toggle('is-open', mode === 'matrix');
        matrix.setAttribute('aria-hidden', mode === 'matrix' ? 'false' : 'true');
      }
      if (modeBar) {
        modeBar.querySelectorAll('.mode-btn').forEach(b => {
          const active = b.getAttribute('data-mode') === mode;
          b.classList.toggle('is-active', active);
          b.setAttribute('aria-selected', active ? 'true' : 'false');
        });
      }
      if (setEditorHeight) {
        try { setEditorHeight(mode === 'matrix' ? MATRIX_EXPANDED : BASE_HEIGHT); }
        catch (e) {}
      }
      // Tell the matrix to (re)render now that it's visible.
      if (mode === 'matrix' && typeof window.kaigenRenderMatrix === 'function') {
        window.kaigenRenderMatrix();
      }
    }
    if (modeBar) {
      modeBar.querySelectorAll('.mode-btn').forEach(btn => {
        btn.addEventListener('click', () => {
          const m = btn.getAttribute('data-mode');
          if (m === 'slots' || m === 'matrix') applyMode(m);
        });
      });
    }
    window.kaigenSetModulationMode = applyMode;
```

- [ ] **Step 3: Add a temporary "matrix renders here" placeholder so the toggle is visually verifiable**

In `Source/WebUI/matrix.js`, replace the body with:

```js
// Source/WebUI/matrix.js — matrix view controller. See plan Task 4+.
(function () {
  'use strict';

  function render() {
    const root = document.getElementById('modulation-matrix');
    if (!root) return;
    root.replaceChildren();
    const ph = document.createElement('div');
    ph.className = 'matrix-placeholder';
    ph.textContent = 'matrix view (skeleton — rows/cells in next tasks)';
    ph.style.cssText = 'padding: 40px; text-align: center; color: rgba(255,255,255,0.35); font: 600 11px/1 sans-serif; letter-spacing: 1.5px;';
    root.appendChild(ph);
  }

  window.kaigenRenderMatrix = render;
})();
```

- [ ] **Step 4: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone. Click MATRIX → slot row + drawer disappear, matrix area shows the placeholder text, plugin window grows by ~320 px. Click SLOTS → slot row returns, matrix hides, window shrinks back. The MATRIX button shows the active style.

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/styles.css Source/WebUI/modulation-panel.js Source/WebUI/matrix.js
git commit -m "feat: SLOTS/MATRIX toggle + window-grow + render hook"
```

---

## Task 4: Render modulator rows (left strip)

Renders the per-engine modulator-row strip: one row per modulator, with a live-animation icon, a name label, and a value readout. Engine A on top, Engine B below, divider between. No cells yet.

**Files:**
- Modify: `Source/WebUI/matrix.js`
- Modify: `Source/WebUI/matrix.css`

- [ ] **Step 1: Add modulator-row rendering to `matrix.js`**

Replace the body of `Source/WebUI/matrix.js` with:

```js
// Source/WebUI/matrix.js — matrix view controller.
(function () {
  'use strict';

  // Modulator definitions. Order within each engine: macros → LFOs → random.
  // LFO + Random rows are placeholders (no live data) until PR4/PR5.
  const MODULATORS = {
    A: [
      { id: 'macro1',  type: 'macro',  label: 'MAC 1' },
      { id: 'macro2',  type: 'macro',  label: 'MAC 2' },
      { id: 'lfo1',    type: 'lfo',    label: 'LFO 1', placeholder: 'PR4' },
      { id: 'lfo2',    type: 'lfo',    label: 'LFO 2', placeholder: 'PR4' },
      { id: 'randomA', type: 'random', label: 'RAND',  placeholder: 'PR5' },
    ],
    B: [
      { id: 'macro3',  type: 'macro',  label: 'MAC 3' },
      { id: 'macro4',  type: 'macro',  label: 'MAC 4' },
      { id: 'lfo3',    type: 'lfo',    label: 'LFO 3', placeholder: 'PR4' },
      { id: 'lfo4',    type: 'lfo',    label: 'LFO 4', placeholder: 'PR4' },
      { id: 'randomB', type: 'random', label: 'RAND',  placeholder: 'PR5' },
    ],
  };

  function makeModRow(mod) {
    const row = document.createElement('div');
    row.className = `mtx-mod mtx-mod-${mod.type}`;
    if (mod.placeholder) row.classList.add('is-placeholder');
    row.dataset.modId = mod.id;

    const anim = document.createElement('div');
    anim.className = 'mtx-mod-anim';
    if (mod.type === 'macro') {
      anim.innerHTML = '<span class="mtx-ring" style="--v: 0;"></span>';
    } else if (mod.type === 'lfo') {
      anim.innerHTML = '<svg viewBox="0 0 24 14" preserveAspectRatio="none"><path d="M0,7 Q3,0 6,7 T12,7 T18,7 T24,7" fill="none" stroke="currentColor" stroke-width="1.2"/></svg>';
    } else if (mod.type === 'random') {
      anim.innerHTML = '<span class="mtx-scatter"><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i></span>';
    }

    const name = document.createElement('div');
    name.className = 'mtx-mod-name';
    name.textContent = mod.label;

    const val = document.createElement('div');
    val.className = 'mtx-mod-val';
    val.textContent = mod.placeholder ? mod.placeholder : '0.00';

    row.appendChild(anim);
    row.appendChild(name);
    row.appendChild(val);
    return row;
  }

  function makeEngineBlock(engineId) {
    const block = document.createElement('section');
    block.className = `mtx-engine mtx-engine-${engineId.toLowerCase()}`;

    const header = document.createElement('div');
    header.className = 'mtx-engine-label';
    header.textContent = `ENGINE ${engineId}`;
    block.appendChild(header);

    const grid = document.createElement('div');
    grid.className = 'mtx-grid';
    block.appendChild(grid);

    for (const mod of MODULATORS[engineId]) {
      const row = document.createElement('div');
      row.className = 'mtx-row';
      row.appendChild(makeModRow(mod));
      // Cell area placeholder — populated in later tasks.
      const cells = document.createElement('div');
      cells.className = 'mtx-cells';
      cells.dataset.engine = engineId;
      cells.dataset.modId = mod.id;
      row.appendChild(cells);
      grid.appendChild(row);
    }
    return block;
  }

  function render() {
    const root = document.getElementById('modulation-matrix');
    if (!root) return;
    root.replaceChildren();
    root.appendChild(makeEngineBlock('A'));
    const divider = document.createElement('div');
    divider.className = 'mtx-engine-divider';
    root.appendChild(divider);
    root.appendChild(makeEngineBlock('B'));
  }

  // Expose so modulation-panel.js can trigger a render on toggle-to-matrix.
  window.kaigenRenderMatrix = render;
})();
```

- [ ] **Step 2: Add modulator-row CSS**

Replace the body of `Source/WebUI/matrix.css` with:

```css
/* Source/WebUI/matrix.css — matrix view styles. */

.modulation-matrix {
  background: #0a0c10;
  border-radius: 6px;
  margin: 0 12px 8px;
  padding: 12px;
  color: #c8d8ea;
  font: 11px/1.3 'Space Grotesk', system-ui, sans-serif;
}

.mtx-engine-label {
  font: 700 10px/1 'Space Grotesk', sans-serif;
  letter-spacing: 3px;
  margin-bottom: 8px;
}
.mtx-engine-a .mtx-engine-label { color: rgba(74, 144, 226, 0.65); }
.mtx-engine-b .mtx-engine-label { color: rgba(93, 211, 224, 0.65); }

.mtx-engine-divider {
  height: 1px;
  background: linear-gradient(90deg, transparent, rgba(255,255,255,0.18), transparent);
  margin: 16px 0 6px;
}

.mtx-grid {
  display: flex;
  flex-direction: column;
  gap: 1px;
  background: rgba(0, 0, 0, 0.4);
}

.mtx-row {
  display: grid;
  grid-template-columns: 130px 1fr;
  gap: 8px;
  align-items: stretch;
}

.mtx-mod {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 10px;
  background: rgba(20, 24, 30, 0.5);
  border-left: 2px solid transparent;
  border-radius: 3px 0 0 3px;
}
.mtx-mod-macro  { border-left-color: #5DD3E0; }
.mtx-mod-lfo    { border-left-color: #4A90E2; }
.mtx-mod-random { border-left-color: #6B5DC9; }
.mtx-mod.is-placeholder { opacity: 0.55; }

.mtx-mod-anim {
  width: 28px;
  height: 28px;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  position: relative;
}

.mtx-ring {
  width: 22px;
  height: 22px;
  border-radius: 50%;
  background: conic-gradient(currentColor calc(var(--v, 0) * 1%), rgba(255,255,255,0.08) 0);
  position: relative;
  color: #5DD3E0;
}
.mtx-ring::after {
  content: '';
  position: absolute;
  inset: 4px;
  border-radius: 50%;
  background: #0a0c10;
}

.mtx-mod-lfo .mtx-mod-anim svg {
  width: 24px;
  height: 14px;
  color: #4A90E2;
}

.mtx-scatter {
  width: 22px;
  height: 22px;
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  grid-template-rows: repeat(3, 1fr);
  gap: 1px;
  color: #9990E0;
}
.mtx-scatter i {
  background: currentColor;
  opacity: 0.3;
  border-radius: 50%;
}
.mtx-scatter i:nth-child(1) { opacity: 0.8; }
.mtx-scatter i:nth-child(5) { opacity: 0.6; }
.mtx-scatter i:nth-child(7) { opacity: 0.9; }

.mtx-mod-name {
  font-weight: 700;
  letter-spacing: 1.5px;
  font-size: 9px;
  line-height: 1;
}
.mtx-mod-macro  .mtx-mod-name { color: #5DD3E0; }
.mtx-mod-lfo    .mtx-mod-name { color: #4A90E2; }
.mtx-mod-random .mtx-mod-name { color: #9990E0; }

.mtx-mod-val {
  font: 600 9px/1 monospace;
  color: rgba(255, 255, 255, 0.55);
  margin-left: auto;
}
.mtx-mod.is-placeholder .mtx-mod-val { font-size: 8px; }

.mtx-cells {
  display: grid;
  grid-template-columns: repeat(32, minmax(28px, 1fr));
  gap: 1px;
  background: rgba(20, 24, 30, 0.4);
  min-height: 30px;
}
```

(Cell column count is hardcoded to 32 here as a placeholder — Task 5 generalizes via category headers.)

- [ ] **Step 3: Wire macro live values into the matrix rings**

Open `Source/WebUI/matrix.js`. Above `window.kaigenRenderMatrix = render;`, add:

```js
  document.body.addEventListener('kaigen:live-state', (ev) => {
    const root = document.getElementById('modulation-matrix');
    if (!root || !root.classList.contains('is-open')) return;  // skip when hidden
    const macros = (ev.detail && ev.detail.macros) || {};
    for (const id of ['macro1', 'macro2', 'macro3', 'macro4']) {
      const ring = root.querySelector(`.mtx-mod[data-mod-id="${id}"] .mtx-ring`);
      const val  = root.querySelector(`.mtx-mod[data-mod-id="${id}"] .mtx-mod-val`);
      const v = (macros[id] && typeof macros[id].value === 'number') ? macros[id].value : 0;
      if (ring) ring.style.setProperty('--v', String(Math.max(0, Math.min(1, v)) * 100));
      if (val)  val.textContent = v.toFixed(2);
    }
  });
```

- [ ] **Step 4: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, click MATRIX. Two engine blocks (A and B) with 5 modulator rows each. Macro rows show their type-color border-left, name label, and a "0.00" value. Drag a macro from the drawer (or DAW) — its ring fills, its readout updates to the current value. LFO and random rows are greyed with "PR4" / "PR5" placeholder labels.

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/matrix.js Source/WebUI/matrix.css
git commit -m "feat: matrix-view modulator-row strip (10 rows + live macro rings)"
```

---

## Task 5: Render destination columns with category groups

Replaces the 32-column placeholder grid with 8 category groups, each header expandable. Default state: all collapsed except GHOST and RECIPE (showing the most-likely-modulated params). Cells are still empty visuals; no routings rendered yet.

**Files:**
- Modify: `Source/WebUI/matrix.js`
- Modify: `Source/WebUI/matrix.css`

- [ ] **Step 1: Define the destinations table at the top of `matrix.js`**

In `Source/WebUI/matrix.js`, immediately after the `MODULATORS` constant, add:

```js
  // Continuous-parameter destinations grouped by category. Choice/bool params
  // (mode, ghost_mode, env_source, midi_trigger_enabled, etc.) are excluded
  // because they're not modulation-eligible. The label is a 5–6 char abbreviation
  // shown in the column header; the tooltip shows the full prefixed param ID.
  const DEST_GROUPS = [
    { id: 'GHOST',    label: 'GHOST',    leaves: [
      ['ghost', 'GHOST'], ['phantom_threshold', 'PHTHR'],
      ['phantom_strength', 'PSTR'], ['output_gain', 'OUT'],
    ]},
    { id: 'RECIPE',   label: 'RECIPE',   leaves: [
      ['recipe_h2', 'H2'], ['recipe_h3', 'H3'], ['recipe_h4', 'H4'], ['recipe_h5', 'H5'],
      ['recipe_h6', 'H6'], ['recipe_h7', 'H7'], ['recipe_h8', 'H8'],
      ['harmonic_saturation', 'HSAT'],
    ]},
    { id: 'SHAPE',    label: 'SHAPE',    leaves: [
      ['synth_step', 'STEP'], ['synth_duty', 'DUTY'], ['synth_skip', 'SKIP'],
    ]},
    { id: 'ENVELOPE', label: 'ENV',      leaves: [
      ['env_attack_ms', 'ATK'], ['env_release_ms', 'REL'],
    ]},
    { id: 'FILTER',   label: 'FILTER',   leaves: [
      ['synth_lpf_hz', 'LPF'], ['synth_hpf_hz', 'HPF'],
    ]},
    { id: 'RESYN',    label: 'RESYN',    leaves: [
      ['synth_wavelet_length', 'WVLEN'], ['synth_gate_threshold', 'GATE'],
      ['synth_h1', 'H1'], ['synth_sub', 'SUB'],
    ]},
    { id: 'PITCH',    label: 'PITCH',    leaves: [
      ['synth_min_samples', 'MINSP'], ['synth_max_samples', 'MAXSP'],
      ['tracking_speed', 'TRACK'], ['punch_amount', 'PUNCH'],
      ['synth_boost_threshold', 'BTHR'], ['synth_boost_amount', 'BAMT'],
    ]},
    { id: 'STEREO',   label: 'STEREO',   leaves: [
      ['binaural_width', 'BIN'], ['stereo_width', 'WIDTH'],
    ]},
  ];

  // UI state: which categories are expanded per engine. Persisted via
  // matrixGetState/matrixSetState bindings (Task 12). Default-expanded:
  // GHOST and RECIPE only.
  const expandedCats = {
    A: new Set(['GHOST', 'RECIPE']),
    B: new Set(['GHOST', 'RECIPE']),
  };
```

- [ ] **Step 2: Replace the cell area builder with a category-grouped grid**

In `matrix.js`, find `function makeEngineBlock(engineId)`. Replace it with:

```js
  function makeColumnHeaderRow(engineId) {
    const headerRow = document.createElement('div');
    headerRow.className = 'mtx-row mtx-header-row';
    const empty = document.createElement('div');
    empty.className = 'mtx-mod mtx-mod-placeholder';
    empty.style.background = 'transparent';
    empty.style.borderLeft = 'none';
    headerRow.appendChild(empty);

    const headerCells = document.createElement('div');
    headerCells.className = 'mtx-cells mtx-header-cells';
    for (const group of DEST_GROUPS) {
      const isOpen = expandedCats[engineId].has(group.id);
      const groupHeader = document.createElement('div');
      groupHeader.className = 'mtx-cat-header' + (isOpen ? ' is-open' : '');
      groupHeader.dataset.engine = engineId;
      groupHeader.dataset.cat = group.id;
      groupHeader.textContent = group.label + (isOpen ? ' ▾' : ' ▸');
      groupHeader.title = isOpen ? `Collapse ${group.label}` : `Expand ${group.label}`;
      groupHeader.addEventListener('click', () => {
        if (expandedCats[engineId].has(group.id)) expandedCats[engineId].delete(group.id);
        else expandedCats[engineId].add(group.id);
        render();
        if (typeof window.kaigenSaveMatrixState === 'function') window.kaigenSaveMatrixState();
      });
      headerCells.appendChild(groupHeader);

      if (isOpen) {
        for (const [leaf, label] of group.leaves) {
          const colHeader = document.createElement('div');
          colHeader.className = 'mtx-col-header';
          colHeader.textContent = label;
          colHeader.title = (engineId === 'A' ? 'a_' : 'b_') + leaf;
          headerCells.appendChild(colHeader);
        }
      }
    }
    headerRow.appendChild(headerCells);
    return headerRow;
  }

  function makeEngineBlock(engineId) {
    const block = document.createElement('section');
    block.className = `mtx-engine mtx-engine-${engineId.toLowerCase()}`;

    const header = document.createElement('div');
    header.className = 'mtx-engine-label';
    header.textContent = `ENGINE ${engineId}`;
    block.appendChild(header);

    const grid = document.createElement('div');
    grid.className = 'mtx-grid';
    block.appendChild(grid);

    grid.appendChild(makeColumnHeaderRow(engineId));

    for (const mod of MODULATORS[engineId]) {
      const row = document.createElement('div');
      row.className = 'mtx-row';
      row.appendChild(makeModRow(mod));

      const cells = document.createElement('div');
      cells.className = 'mtx-cells';
      cells.dataset.engine = engineId;
      cells.dataset.modId = mod.id;
      for (const group of DEST_GROUPS) {
        const isOpen = expandedCats[engineId].has(group.id);
        const groupSpacer = document.createElement('div');
        groupSpacer.className = 'mtx-cat-spacer';
        cells.appendChild(groupSpacer);
        if (isOpen) {
          for (const [leaf, _label] of group.leaves) {
            const cell = document.createElement('div');
            cell.className = 'mtx-cell';
            cell.dataset.engine = engineId;
            cell.dataset.modId = mod.id;
            cell.dataset.paramId = (engineId === 'A' ? 'a_' : 'b_') + leaf;
            cell.title = cell.dataset.paramId;
            cells.appendChild(cell);
          }
        }
      }
      row.appendChild(cells);
      grid.appendChild(row);
    }
    return block;
  }
```

The header row always renders all category headers. Below it, modulator rows put one spacer per category (occupies the same slot as the header) plus one cell per visible leaf when expanded. CSS uses `auto-flow: column; grid-auto-columns: min-content;` so spacers and cells line up.

- [ ] **Step 3: Update CSS for category headers + dynamic column count**

In `Source/WebUI/matrix.css`, replace the `.mtx-cells` rule from Task 4 with:

```css
.mtx-cells {
  display: flex;
  gap: 1px;
  background: rgba(20, 24, 30, 0.4);
  min-height: 30px;
  overflow: hidden;
}

.mtx-header-row .mtx-cells {
  background: transparent;
  min-height: auto;
}

.mtx-cat-header {
  padding: 4px 8px;
  background: rgba(255, 255, 255, 0.04);
  font: 700 8px/1 'Space Grotesk', sans-serif;
  letter-spacing: 1.5px;
  color: rgba(255, 255, 255, 0.55);
  cursor: pointer;
  user-select: none;
  white-space: nowrap;
  flex-shrink: 0;
  border-right: 1px solid rgba(0, 0, 0, 0.4);
}
.mtx-cat-header:hover { color: #fff; background: rgba(255, 255, 255, 0.07); }
.mtx-cat-header.is-open {
  background: rgba(93, 211, 224, 0.10);
  color: rgba(255, 255, 255, 0.85);
}

.mtx-col-header {
  padding: 4px 6px;
  font: 600 8px/1 'Space Grotesk', sans-serif;
  letter-spacing: 1.5px;
  color: rgba(255, 255, 255, 0.45);
  text-align: center;
  white-space: nowrap;
  flex: 1 1 36px;
  min-width: 36px;
}

.mtx-cat-spacer {
  width: calc(8px + 6ch + 18px);   /* approx category-header width; match by feel */
  flex-shrink: 0;
}

.mtx-cell {
  flex: 1 1 36px;
  min-width: 36px;
  height: 30px;
  background: rgba(20, 24, 30, 0.6);
  cursor: pointer;
  position: relative;
}
.mtx-cell:hover {
  background: rgba(40, 46, 56, 0.7);
}
```

The `.mtx-cat-spacer` width is a hand-tuned value — adjust during implementation if column alignment with the category-header spans drifts.

- [ ] **Step 4: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, click MATRIX. Each engine block shows a header row with 8 category labels (`GHOST ▾`, `RECIPE ▾`, `SHAPE ▸`, `ENV ▸`, ...). GHOST and RECIPE are expanded showing their column abbreviations (GHOST, PHTHR, PSTR, OUT, H2, H3, ...). Modulator rows below have empty cells under expanded categories, blank space under collapsed categories. Click `SHAPE ▸` → expands inline showing STEP, DUTY, SKIP columns. Click again → collapses.

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/matrix.js Source/WebUI/matrix.css
git commit -m "feat: matrix-view destination columns + category expand/collapse"
```

---

## Task 6: Render existing routings into cells (read-only)

Pulls routing data from `modulationGetState` and renders active cells with depth, polarity, and modulator color. Re-renders on `modulationStateChanged` events (already dispatched by the C++ bindings after every mutation). Still no add/remove from the matrix — read-only display.

**Files:**
- Modify: `Source/WebUI/matrix.js`
- Modify: `Source/WebUI/matrix.css`

- [ ] **Step 1: Add `getState` cache + cell-fill rendering to `matrix.js`**

At the top of `matrix.js`'s IIFE, after the `expandedCats` declaration, add:

```js
  let getState = null;
  if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
    try { getState = window.Juce.getNativeFunction('modulationGetState'); }
    catch (e) { console.warn('[matrix] modulationGetState unavailable', e); }
  }

  let lastState = { engineA: { routings: [] }, engineB: { routings: [] } };

  function pullState() {
    if (!getState) return;
    try {
      const s = getState();
      if (s && typeof s === 'object') lastState = s;
    } catch (e) { console.warn('[matrix] getState failed', e); }
  }

  function findRouting(engineId, modId, paramId) {
    const eng = engineId === 'A' ? lastState.engineA : lastState.engineB;
    if (!eng || !Array.isArray(eng.routings)) return null;
    return eng.routings.find(r => r.source === modId && r.param === paramId) || null;
  }
```

- [ ] **Step 2: Update cell rendering to apply routing visuals**

Inside `makeEngineBlock`, find the inner cell-creation loop (the `for (const [leaf, _label] of group.leaves)` block). Replace the cell creation with:

```js
        if (isOpen) {
          for (const [leaf, _label] of group.leaves) {
            const paramId = (engineId === 'A' ? 'a_' : 'b_') + leaf;
            const routing = findRouting(engineId, mod.id, paramId);
            const cell = document.createElement('div');
            cell.className = 'mtx-cell mtx-cell-' + mod.type;
            cell.dataset.engine  = engineId;
            cell.dataset.modId   = mod.id;
            cell.dataset.paramId = paramId;
            cell.title = paramId;
            if (routing) {
              const d = Math.max(-1, Math.min(1, routing.depth));
              cell.classList.add('is-routed');
              if (d < 0) cell.classList.add('is-negative');
              cell.style.setProperty('--depth-pct', String(Math.round(d * 100)));
              cell.style.setProperty('--depth-abs', String(Math.abs(d)));
              const num = document.createElement('span');
              num.className = 'mtx-cell-depth';
              num.textContent = (d > 0 ? '+' : '') + Math.round(d * 100);
              cell.appendChild(num);
            }
            cells.appendChild(cell);
          }
        }
```

- [ ] **Step 3: Pull state at render entry + re-render on state-change events**

In `matrix.js`, change the `render()` function to call `pullState()` first. Also subscribe to the `modulationStateChanged` event:

```js
  function render() {
    pullState();
    const root = document.getElementById('modulation-matrix');
    if (!root) return;
    root.replaceChildren();
    root.appendChild(makeEngineBlock('A'));
    const divider = document.createElement('div');
    divider.className = 'mtx-engine-divider';
    root.appendChild(divider);
    root.appendChild(makeEngineBlock('B'));
  }

  // The C++ bindings dispatch this CustomEvent after every Add/Remove/SetDepth.
  document.body.addEventListener('modulationStateChanged', () => {
    if (document.getElementById('modulation-matrix').classList.contains('is-open')) {
      render();
    } else {
      // Pull anyway so the next render-on-toggle has fresh data.
      pullState();
    }
  });
```

(If the C++ side does NOT yet dispatch `modulationStateChanged`, verify in `Source/PluginEditor.cpp`'s native-binding lambdas. The drawer-era `macro-editor.js` relied on this same event; if it works there, it works here. If it's missing, add it: each `addRouting` / `removeRouting` / `setRoutingDepth` lambda should call the existing JS-broadcast helper or directly evaluate `document.body.dispatchEvent(new CustomEvent('modulationStateChanged'))` via `WebView::evaluateJavascript`. Include any binding additions in this task's commit.)

- [ ] **Step 4: Add CSS for routed cells**

In `Source/WebUI/matrix.css`, replace the `.mtx-cell` rule with:

```css
.mtx-cell {
  flex: 1 1 36px;
  min-width: 36px;
  height: 30px;
  background: rgba(20, 24, 30, 0.6);
  cursor: pointer;
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  font: 600 8px/1 monospace;
  color: rgba(255, 255, 255, 0.25);
  transition: background 80ms;
}
.mtx-cell:hover {
  background: rgba(40, 46, 56, 0.7);
  color: rgba(255, 255, 255, 0.5);
}

.mtx-cell.is-routed {
  color: #fff;
}
.mtx-cell.is-routed.mtx-cell-macro {
  background: linear-gradient(90deg,
              rgba(93, 211, 224, calc(0.18 + var(--depth-abs, 0) * 0.27)),
              rgba(93, 211, 224, calc(0.18 + var(--depth-abs, 0) * 0.27)));
  box-shadow: inset 0 0 0 1px rgba(93, 211, 224, 0.55);
}
.mtx-cell.is-routed.mtx-cell-macro.is-negative {
  background: linear-gradient(270deg,
              rgba(93, 211, 224, calc(0.18 + var(--depth-abs, 0) * 0.27)),
              rgba(93, 211, 224, calc(0.18 + var(--depth-abs, 0) * 0.27)));
}
.mtx-cell.is-routed.mtx-cell-lfo {
  background: linear-gradient(90deg,
              rgba(74, 144, 226, calc(0.18 + var(--depth-abs, 0) * 0.27)),
              rgba(74, 144, 226, calc(0.18 + var(--depth-abs, 0) * 0.27)));
  box-shadow: inset 0 0 0 1px rgba(74, 144, 226, 0.55);
}
.mtx-cell.is-routed.mtx-cell-random {
  background: linear-gradient(90deg,
              rgba(107, 93, 201, calc(0.18 + var(--depth-abs, 0) * 0.27)),
              rgba(107, 93, 201, calc(0.18 + var(--depth-abs, 0) * 0.27)));
  box-shadow: inset 0 0 0 1px rgba(107, 93, 201, 0.55);
}

.mtx-cell-depth { z-index: 1; pointer-events: none; }
```

(Bipolar fill direction is encoded by reversing the gradient angle — left-to-right for positive, right-to-left for negative. Saturation is depth-driven via `--depth-abs`.)

- [ ] **Step 5: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, click into the (still-existing) drawer, add a routing — for example, set Macro 1 → Ghost at +60%. Toggle to MATRIX. The cell at row Macro 1, column GHOST shows `+60` with a teal gradient filled left-to-right. Add a negative-depth routing in the drawer (or have the implementer hand-edit one to negative via the drawer's depth slider). Verify the matrix cell shows `-30` (or whatever value) with the gradient flowing right-to-left.

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/matrix.js Source/WebUI/matrix.css
git commit -m "feat: matrix renders routings from modulationGetState (read-only)"
```

---

## Task 7: Click empty cell to add routing

Adds the click handler on empty cells. Calls `modulationAddRouting` with the cell's modulator + param + default depth +0.5. Re-renders on success.

**Files:**
- Modify: `Source/WebUI/matrix.js`

- [ ] **Step 1: Cache the addRouting binding at the top of the IIFE**

In `matrix.js`, near the existing `getState` binding caching, add:

```js
  let addRouting = null;
  try { addRouting = window.Juce.getNativeFunction('modulationAddRouting'); }
  catch (e) { console.warn('[matrix] addRouting unavailable', e); }
```

- [ ] **Step 2: Wire click-to-add inside the cell-creation loop**

In `makeEngineBlock`, find the cell creation block. After the line `cell.title = paramId;`, add:

```js
            cell.addEventListener('click', (ev) => {
              if (cell.classList.contains('is-routed')) return;   // active cells handled by drag/right-click
              if (!addRouting) return;
              try {
                addRouting({
                  source: mod.id,
                  param: paramId,
                  depth: 0.5,
                });
              } catch (e) { console.warn('[matrix] addRouting failed', e); }
              // Re-render via the modulationStateChanged event the binding fires.
            });
```

- [ ] **Step 3: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, switch to MATRIX. Click the empty cell at Macro 1 × GHOST. Cell lights up at `+50`, teal gradient. Click empty cell at Macro 1 × PSTR — also lights up. Switch back to SLOTS, open the macro 1 drawer — both routings are listed there too (proving the C++ side received them). Toggle back to MATRIX and confirm the cells are still rendered.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/matrix.js
git commit -m "feat: matrix click empty cell -> AddRouting at +50%"
```

---

## Task 8: Drag active cell vertically to adjust depth

Adds pointer-event-based drag on active cells. Vertical drag distance maps to depth change; releasing commits via `modulationSetRoutingDepth`. Snap to ±0 within a small dead zone. Other cells stay unaffected during the drag.

**Files:**
- Modify: `Source/WebUI/matrix.js`

- [ ] **Step 1: Cache `setRoutingDepth`**

Near other binding caches in `matrix.js`:

```js
  let setRoutingDepth = null;
  try { setRoutingDepth = window.Juce.getNativeFunction('modulationSetRoutingDepth'); }
  catch (e) { console.warn('[matrix] setRoutingDepth unavailable', e); }
```

- [ ] **Step 2: Add drag handler in the cell-creation block**

In `makeEngineBlock`, replace the cell click handler from Task 7 with this expanded handler that distinguishes click vs drag:

```js
            const startDrag = (downEv) => {
              if (!cell.classList.contains('is-routed')) return;   // empty cells use click (handled below)
              if (downEv.button !== 0) return;                      // only left button
              downEv.preventDefault();
              const startY = downEv.clientY;
              const routing = findRouting(engineId, mod.id, paramId);
              if (!routing) return;
              const startDepth = Math.max(-1, Math.min(1, routing.depth));
              let dragged = false;
              const onMove = (moveEv) => {
                const dy = startY - moveEv.clientY;        // up = positive
                if (Math.abs(dy) > 3) dragged = true;
                let next = startDepth + dy / 100;          // 100 px = full range
                if (Math.abs(next) < 0.03) next = 0;       // dead-zone snap
                next = Math.max(-1, Math.min(1, next));
                cell.style.setProperty('--depth-pct', String(Math.round(next * 100)));
                cell.style.setProperty('--depth-abs', String(Math.abs(next)));
                cell.classList.toggle('is-negative', next < 0);
                const num = cell.querySelector('.mtx-cell-depth');
                if (num) num.textContent = (next > 0 ? '+' : '') + Math.round(next * 100);
              };
              const onUp = (upEv) => {
                window.removeEventListener('pointermove', onMove);
                window.removeEventListener('pointerup', onUp);
                if (!dragged) return;                       // no drag = let click handler fire
                if (!setRoutingDepth) return;
                const finalDepth = parseInt(cell.style.getPropertyValue('--depth-pct'), 10) / 100;
                try {
                  setRoutingDepth({
                    source: mod.id,
                    param: paramId,
                    depth: finalDepth,
                  });
                } catch (e) { console.warn('[matrix] setRoutingDepth failed', e); }
              };
              window.addEventListener('pointermove', onMove);
              window.addEventListener('pointerup',   onUp);
            };

            cell.addEventListener('pointerdown', startDrag);
            cell.addEventListener('click', (ev) => {
              if (cell.classList.contains('is-routed')) return;   // routed cells use drag/right-click
              if (!addRouting) return;
              try {
                addRouting({
                  source: mod.id,
                  param: paramId,
                  depth: 0.5,
                });
              } catch (e) { console.warn('[matrix] addRouting failed', e); }
            });
```

- [ ] **Step 3: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, switch to MATRIX. Add a routing to a cell (Macro 1 × GHOST). Press and drag the cell upward — depth value increases visually as you drag (`+50` → `+72` → `+100`). Drag downward — depth decreases, crosses zero (snapping briefly to `0`), goes negative (`-25`), gradient flips direction. Release → value persists. Toggle to SLOTS, open the macro 1 drawer — depth slider matches the new value.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/matrix.js
git commit -m "feat: matrix drag active cell -> SetRoutingDepth (vertical, dead-zone snap)"
```

---

## Task 9: Right-click cell for quick-set + remove popover

Adds a small popover on right-click of any cell (active or empty). For active cells, it shows quick-set buttons (`-100`, `0`, `+50`, `+100`) and a Remove button. For empty cells, it shows just an "Add at +50%" / "Add at -50%" pair (so the user can add with negative polarity directly).

**Files:**
- Modify: `Source/WebUI/matrix.js`
- Modify: `Source/WebUI/matrix.css`

- [ ] **Step 1: Cache `removeRouting`**

```js
  let removeRouting = null;
  try { removeRouting = window.Juce.getNativeFunction('modulationRemoveRouting'); }
  catch (e) { console.warn('[matrix] removeRouting unavailable', e); }
```

- [ ] **Step 2: Implement a minimal popover helper**

Above `function render()`, add:

```js
  let activePopover = null;
  function closePopover() {
    if (activePopover && activePopover.parentNode) activePopover.remove();
    activePopover = null;
    document.removeEventListener('click', onDocClick, true);
  }
  function onDocClick(ev) {
    if (activePopover && !activePopover.contains(ev.target)) closePopover();
  }
  function openPopover(x, y, items) {
    closePopover();
    const pop = document.createElement('div');
    pop.className = 'mtx-popover';
    pop.style.left = x + 'px';
    pop.style.top  = y + 'px';
    for (const item of items) {
      const btn = document.createElement('button');
      btn.className = 'mtx-popover-btn' + (item.danger ? ' is-danger' : '');
      btn.textContent = item.label;
      btn.addEventListener('click', (ev) => {
        ev.stopPropagation();
        try { item.onClick(); } finally { closePopover(); }
      });
      pop.appendChild(btn);
    }
    document.body.appendChild(pop);
    activePopover = pop;
    setTimeout(() => document.addEventListener('click', onDocClick, true), 0);
  }
```

- [ ] **Step 3: Wire `contextmenu` on cells**

Inside the cell-creation block (after the existing `pointerdown` and `click` handlers), add:

```js
            cell.addEventListener('contextmenu', (ev) => {
              ev.preventDefault();
              const items = cell.classList.contains('is-routed')
                ? [
                    { label: '−100',   onClick: () => setRoutingDepth({ source: mod.id, param: paramId, depth: -1 }) },
                    { label: '0',      onClick: () => setRoutingDepth({ source: mod.id, param: paramId, depth: 0 }) },
                    { label: '+50',    onClick: () => setRoutingDepth({ source: mod.id, param: paramId, depth: 0.5 }) },
                    { label: '+100',   onClick: () => setRoutingDepth({ source: mod.id, param: paramId, depth: 1 }) },
                    { label: 'Remove', danger: true, onClick: () => removeRouting({ source: mod.id, param: paramId }) },
                  ]
                : [
                    { label: 'Add at +50', onClick: () => addRouting({ source: mod.id, param: paramId, depth: 0.5  }) },
                    { label: 'Add at −50', onClick: () => addRouting({ source: mod.id, param: paramId, depth: -0.5 }) },
                  ];
              openPopover(ev.pageX, ev.pageY, items);
            });
```

- [ ] **Step 4: Add popover CSS**

In `Source/WebUI/matrix.css`, append:

```css
.mtx-popover {
  position: absolute;
  background: #15181d;
  border: 1px solid rgba(93, 211, 224, 0.3);
  border-radius: 4px;
  padding: 4px;
  display: flex;
  flex-direction: column;
  gap: 2px;
  z-index: 1000;
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.6);
  font: 600 10px/1 'Space Grotesk', sans-serif;
  letter-spacing: 1px;
}
.mtx-popover-btn {
  background: transparent;
  border: none;
  color: rgba(255, 255, 255, 0.7);
  padding: 6px 12px;
  text-align: left;
  cursor: pointer;
  border-radius: 2px;
}
.mtx-popover-btn:hover {
  background: rgba(93, 211, 224, 0.18);
  color: #5DD3E0;
}
.mtx-popover-btn.is-danger { color: rgba(232, 100, 100, 0.85); }
.mtx-popover-btn.is-danger:hover {
  background: rgba(232, 100, 100, 0.18);
  color: #ff8585;
}
```

- [ ] **Step 5: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, switch to MATRIX. Right-click an empty cell → popover with two add options. Right-click an active cell → popover with quick-set buttons + Remove. Click `+100` → cell jumps to `+100`. Click `Remove` → cell becomes empty. Click anywhere else → popover closes.

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/matrix.js Source/WebUI/matrix.css
git commit -m "feat: matrix right-click popover (quick-set + remove)"
```

---

## Task 10: Inline macro name editor

Clicking a macro modulator-row's name label inline-replaces it with a text input. Enter commits via `modulationSetMacroName`; Escape cancels. LFO/Random rows are non-editable (their click is reserved for PR4/PR5 popovers).

**Files:**
- Modify: `Source/WebUI/matrix.js`

- [ ] **Step 1: Cache `setMacroName`**

```js
  let setMacroName = null;
  try { setMacroName = window.Juce.getNativeFunction('modulationSetMacroName'); }
  catch (e) { console.warn('[matrix] setMacroName unavailable', e); }
```

- [ ] **Step 2: Wire click handler in `makeModRow`**

Replace `function makeModRow(mod)` with:

```js
  function makeModRow(mod) {
    const row = document.createElement('div');
    row.className = `mtx-mod mtx-mod-${mod.type}`;
    if (mod.placeholder) row.classList.add('is-placeholder');
    row.dataset.modId = mod.id;

    const anim = document.createElement('div');
    anim.className = 'mtx-mod-anim';
    if (mod.type === 'macro') {
      anim.innerHTML = '<span class="mtx-ring" style="--v: 0;"></span>';
    } else if (mod.type === 'lfo') {
      anim.innerHTML = '<svg viewBox="0 0 24 14" preserveAspectRatio="none"><path d="M0,7 Q3,0 6,7 T12,7 T18,7 T24,7" fill="none" stroke="currentColor" stroke-width="1.2"/></svg>';
    } else if (mod.type === 'random') {
      anim.innerHTML = '<span class="mtx-scatter"><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i></span>';
    }

    const name = document.createElement('div');
    name.className = 'mtx-mod-name';
    // Read current name from state (modulators[].name) if available, else default label.
    const eng = (mod.id === 'macro1' || mod.id === 'macro2' || mod.id === 'lfo1' || mod.id === 'lfo2' || mod.id === 'randomA') ? 'A' : 'B';
    const stateMods = (eng === 'A' ? lastState.engineA : lastState.engineB).modulators || [];
    const stateMod = stateMods.find(m => m.id === mod.id);
    name.textContent = (stateMod && stateMod.name) ? stateMod.name : mod.label;

    if (mod.type === 'macro' && setMacroName) {
      name.addEventListener('click', () => {
        const input = document.createElement('input');
        input.type = 'text';
        input.value = name.textContent;
        input.className = 'mtx-mod-name-input';
        input.maxLength = 16;
        const commit = () => {
          const v = input.value.trim() || mod.label;
          try { setMacroName({ source: mod.id, name: v }); }
          catch (e) { console.warn('[matrix] setMacroName failed', e); }
        };
        const cancel = () => render();
        input.addEventListener('keydown', (kev) => {
          if (kev.key === 'Enter')  { kev.preventDefault(); commit(); }
          if (kev.key === 'Escape') { kev.preventDefault(); cancel(); }
        });
        input.addEventListener('blur', commit);
        name.replaceWith(input);
        input.focus();
        input.select();
      });
    }

    const val = document.createElement('div');
    val.className = 'mtx-mod-val';
    val.textContent = mod.placeholder ? mod.placeholder : '0.00';

    row.appendChild(anim);
    row.appendChild(name);
    row.appendChild(val);
    return row;
  }
```

- [ ] **Step 3: Add input CSS**

In `Source/WebUI/matrix.css`, append:

```css
.mtx-mod-name-input {
  background: #15181d;
  border: 1px solid #5DD3E0;
  border-radius: 2px;
  color: #5DD3E0;
  font: 700 9px/1 'Space Grotesk', sans-serif;
  letter-spacing: 1.5px;
  padding: 3px 6px;
  width: 70px;
  outline: none;
}
```

- [ ] **Step 4: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, MATRIX mode. Click `MAC 1` label → text input replaces it, prefilled with "MAC 1". Type "BASS DRIVE" → press Enter. Label updates. Toggle SLOTS → drawer header for macro 1 shows "BASS DRIVE". Toggle MATRIX → label persists. Click again, type something, press Escape → reverts to "BASS DRIVE". LFO/Random labels are inert when clicked.

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/matrix.js Source/WebUI/matrix.css
git commit -m "feat: matrix inline macro-name editor (Enter/Escape/blur)"
```

---

## Task 11: Live-modulating glow on active cells

Subscribes the matrix to `kaigen:live-state` events. For each active cell, if `liveValue × depth > 0.05`, apply a CSS class that pulses a per-modulator-color glow. Removed when contribution drops below threshold.

**Files:**
- Modify: `Source/WebUI/matrix.js`
- Modify: `Source/WebUI/matrix.css`

- [ ] **Step 1: Add live-state hook for cells in `matrix.js`**

Replace the existing `document.body.addEventListener('kaigen:live-state', ...)` block with:

```js
  document.body.addEventListener('kaigen:live-state', (ev) => {
    const root = document.getElementById('modulation-matrix');
    if (!root || !root.classList.contains('is-open')) return;
    const macros = (ev.detail && ev.detail.macros) || {};

    // Update modulator-row rings + value readouts.
    for (const id of ['macro1', 'macro2', 'macro3', 'macro4']) {
      const ring = root.querySelector(`.mtx-mod[data-mod-id="${id}"] .mtx-ring`);
      const val  = root.querySelector(`.mtx-mod[data-mod-id="${id}"] .mtx-mod-val`);
      const v = (macros[id] && typeof macros[id].value === 'number') ? macros[id].value : 0;
      if (ring) ring.style.setProperty('--v', String(Math.max(0, Math.min(1, v)) * 100));
      if (val)  val.textContent = v.toFixed(2);
    }

    // Update per-cell live-modulating glow.
    root.querySelectorAll('.mtx-cell.is-routed').forEach(cell => {
      const modId = cell.dataset.modId;
      const live = (macros[modId] && typeof macros[modId].value === 'number') ? macros[modId].value : 0;
      const depth = parseInt(cell.style.getPropertyValue('--depth-pct'), 10) / 100;
      const contribution = Math.abs(live * depth);
      cell.classList.toggle('is-modulating', contribution > 0.05);
      cell.style.setProperty('--mod-pulse', String(contribution));
    });
  });
```

- [ ] **Step 2: Add the modulating-glow CSS**

In `Source/WebUI/matrix.css`, append:

```css
@keyframes mtx-cell-pulse {
  0%, 100% { box-shadow: inset 0 0 0 1px var(--mod-color, rgba(93,211,224,0.55)),
                          0 0 4px rgba(93, 211, 224, calc(var(--mod-pulse, 0) * 0.6)); }
  50%      { box-shadow: inset 0 0 0 1px var(--mod-color, rgba(93,211,224,0.85)),
                          0 0 10px rgba(93, 211, 224, calc(var(--mod-pulse, 0) * 1.0)); }
}
.mtx-cell.is-routed.is-modulating {
  animation: mtx-cell-pulse 700ms ease-in-out infinite;
}
.mtx-cell.is-routed.mtx-cell-macro.is-modulating { --mod-color: rgba(93, 211, 224, 0.85); }
.mtx-cell.is-routed.mtx-cell-lfo.is-modulating   { --mod-color: rgba(74, 144, 226, 0.85); }
.mtx-cell.is-routed.mtx-cell-random.is-modulating { --mod-color: rgba(107, 93, 201, 0.85); }
```

- [ ] **Step 3: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, MATRIX mode. Add a routing Macro 1 × Ghost at +50%. Drag macro 1 in slots view (or DAW automation) to 0.5 — the cell at Macro 1 × Ghost should now pulse a teal glow at moderate intensity. Set macro 1 to 0 — pulse stops. Set macro 1 to 1.0 — pulse intensifies. Add a routing Macro 1 × LPF at +30% — that cell pulses with the same intensity (since it's the same modulator). Add Macro 1 × HPF at +5% (very small) — pulses very faintly or below threshold (no animation when contribution < 0.05).

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/matrix.js Source/WebUI/matrix.css
git commit -m "feat: matrix live-modulating cell glow (live × depth > 0.05)"
```

---

## Task 12: Persist matrix view state

Adds a `MatrixViewState` C++ struct + `<MatrixView>` ValueTree round-trip mirroring `EngineFocus.h` / `SpectrumViewMode.h`. Wires save/load in `PluginProcessor.cpp`. Adds `matrixGetState` / `matrixSetState` native bindings. JS side persists current mode + per-engine expanded categories.

**Files:**
- Create: `Source/MatrixViewState.h`
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginEditor.cpp`
- Modify: `Source/WebUI/modulation-panel.js`
- Modify: `Source/WebUI/matrix.js`
- Create: `tests/MatrixViewStatePersistenceTest.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing C++ persistence test**

Create `tests/MatrixViewStatePersistenceTest.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "../Source/MatrixViewState.h"

using kaigen::phantom::MatrixViewState;
using kaigen::phantom::MatrixMode;
using kaigen::phantom::writeMatrixViewToTree;
using kaigen::phantom::readMatrixViewFromTree;

TEST_CASE("MatrixViewState round-trips through ValueTree", "[matrix-view][persistence]")
{
    MatrixViewState in;
    in.mode = MatrixMode::Matrix;
    in.expandedA = "GHOST,RECIPE,FILTER";
    in.expandedB = "RESYN";

    juce::ValueTree parent("PluginState");
    writeMatrixViewToTree(parent, in);

    auto out = readMatrixViewFromTree(parent);
    REQUIRE(out.mode == MatrixMode::Matrix);
    REQUIRE(out.expandedA == "GHOST,RECIPE,FILTER");
    REQUIRE(out.expandedB == "RESYN");
}

TEST_CASE("MatrixViewState defaults to Slots mode + GHOST,RECIPE expanded when missing", "[matrix-view][persistence]")
{
    juce::ValueTree parent("PluginState");
    auto out = readMatrixViewFromTree(parent);
    REQUIRE(out.mode == MatrixMode::Slots);
    REQUIRE(out.expandedA == "GHOST,RECIPE");
    REQUIRE(out.expandedB == "GHOST,RECIPE");
}
```

Add to `tests/CMakeLists.txt` (find the Catch2 target's source list and append):

```cmake
add_executable(matrix_view_state_test
    MatrixViewStatePersistenceTest.cpp
)
target_link_libraries(matrix_view_state_test PRIVATE Catch2::Catch2WithMain juce::juce_data_structures)
target_include_directories(matrix_view_state_test PRIVATE ${CMAKE_SOURCE_DIR})
catch_discover_tests(matrix_view_state_test)
```

(Adjust to match the existing tests' CMake style — e.g., `EngineFocusTest`. The exact wiring may differ; mirror the existing entry for `EngineFocusTest` line-for-line.)

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build --target matrix_view_state_test --config Debug && ctest --test-dir build -C Debug -R matrix_view_state`
Expected: FAIL — `MatrixViewState.h` doesn't exist.

- [ ] **Step 3: Create `Source/MatrixViewState.h`**

```cpp
// Source/MatrixViewState.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** Bottom modulation panel view mode. Persisted in <MatrixView mode="..."/>. */
enum class MatrixMode : int { Slots = 0, Matrix = 1 };

/** Matrix view UI state. Persists mode + per-engine expanded category list.
 *  Routing data itself is in <ModulationConfig>; this struct only holds
 *  layout/UI concerns. */
struct MatrixViewState
{
    MatrixMode  mode       { MatrixMode::Slots };
    juce::String expandedA { "GHOST,RECIPE" };  // comma-separated category IDs
    juce::String expandedB { "GHOST,RECIPE" };
};

inline void writeMatrixViewToTree(juce::ValueTree& parent, const MatrixViewState& s)
{
    juce::ValueTree node("MatrixView");
    node.setProperty("mode", s.mode == MatrixMode::Matrix ? "Matrix" : "Slots", nullptr);
    node.setProperty("expandedA", s.expandedA, nullptr);
    node.setProperty("expandedB", s.expandedB, nullptr);
    parent.appendChild(node, nullptr);
}

inline MatrixViewState readMatrixViewFromTree(const juce::ValueTree& parent)
{
    MatrixViewState s;
    auto node = parent.getChildWithName("MatrixView");
    if (! node.isValid()) return s;
    s.mode = (node.getProperty("mode").toString() == "Matrix")
             ? MatrixMode::Matrix : MatrixMode::Slots;
    if (node.hasProperty("expandedA")) s.expandedA = node.getProperty("expandedA").toString();
    if (node.hasProperty("expandedB")) s.expandedB = node.getProperty("expandedB").toString();
    return s;
}

} // namespace kaigen::phantom
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target matrix_view_state_test --config Debug && ctest --test-dir build -C Debug -R matrix_view_state`
Expected: PASS — both tests green.

- [ ] **Step 5: Wire `MatrixViewState` into the processor**

Open `Source/PluginProcessor.h`. Add to the includes:

```cpp
#include "MatrixViewState.h"
```

Inside the `PhantomProcessor` class, alongside the existing `EngineFocus engineFocus;` and `SpectrumViewMode spectrumViewMode;` members, add:

```cpp
    kaigen::phantom::MatrixViewState matrixView;
```

And public accessors:

```cpp
    kaigen::phantom::MatrixViewState getMatrixView() const          { return matrixView; }
    void setMatrixView(const kaigen::phantom::MatrixViewState& s)   { matrixView = s; }
```

- [ ] **Step 6: Wire save/load in `PluginProcessor.cpp`**

In `getStateInformation` (around line 491–492), after `writeSpectrumViewModeToTree(...)`, add:

```cpp
    kaigen::phantom::writeMatrixViewToTree(wrapper, matrixView);
```

In `setStateInformation` (around line 541–542), after the `SpectrumView` read, add:

```cpp
    if (wrapper.getChildWithName("MatrixView").isValid())
        matrixView = kaigen::phantom::readMatrixViewFromTree(wrapper);
```

- [ ] **Step 7: Add `matrixGetState` / `matrixSetState` native bindings**

Open `Source/PluginEditor.cpp`. Find the existing `addNativeFunction` block where `modulationGetState`, `engineSetFocus`, etc. are registered. Add two new bindings (place them next to the existing modulation bindings):

```cpp
    addNativeFunction("matrixGetState",
        [this](const juce::var& args, NativeFunctionCompletion completion)
        {
            juce::ignoreUnused(args);
            auto s = static_cast<PhantomProcessor&>(processor).getMatrixView();
            auto obj = new juce::DynamicObject();
            obj->setProperty("mode", s.mode == kaigen::phantom::MatrixMode::Matrix ? "Matrix" : "Slots");
            obj->setProperty("expandedA", s.expandedA);
            obj->setProperty("expandedB", s.expandedB);
            completion(juce::var(obj));
        });

    addNativeFunction("matrixSetState",
        [this](const juce::var& args, NativeFunctionCompletion completion)
        {
            kaigen::phantom::MatrixViewState s;
            if (args.isArray() && args.size() > 0 && args[0].isObject())
            {
                const auto& a = args[0];
                if (a["mode"].toString() == "Matrix") s.mode = kaigen::phantom::MatrixMode::Matrix;
                else                                  s.mode = kaigen::phantom::MatrixMode::Slots;
                if (a.hasProperty("expandedA")) s.expandedA = a["expandedA"].toString();
                if (a.hasProperty("expandedB")) s.expandedB = a["expandedB"].toString();
            }
            static_cast<PhantomProcessor&>(processor).setMatrixView(s);
            completion(juce::var(true));
        });
```

(Match the existing binding-registration pattern — the actual signature of `addNativeFunction` and how `NativeFunctionCompletion` is invoked may differ slightly. If unsure, copy the body of `modulationGetState` as a template since it has the same shape.)

- [ ] **Step 8: Wire JS side to load/save**

In `Source/WebUI/modulation-panel.js`, near the top of `init()` after the `applyMode` definition, add:

```js
    // Restore persisted mode + expanded categories on init.
    if (window.Juce && typeof window.Juce.getNativeFunction === 'function') {
      try {
        const matrixGet = window.Juce.getNativeFunction('matrixGetState');
        if (matrixGet) {
          matrixGet().then(s => {
            if (!s) return;
            if (s.expandedA && window.kaigenSetMatrixExpanded) {
              window.kaigenSetMatrixExpanded('A', s.expandedA);
            }
            if (s.expandedB && window.kaigenSetMatrixExpanded) {
              window.kaigenSetMatrixExpanded('B', s.expandedB);
            }
            if (s.mode === 'Matrix') applyMode('matrix');
          }).catch(() => {});
        }
      } catch (e) {}
    }
```

(If `getNativeFunction` returns a sync function in this codebase rather than a Promise, drop the `.then` chain and use the value directly. Verify by checking how `modulationGetState` is consumed in `macro-editor.js` — match that pattern.)

After the existing `applyMode` definition body, before the final `};`, add a `saveState` call:

```js
    function saveMatrixState() {
      if (!window.Juce) return;
      try {
        const matrixSet = window.Juce.getNativeFunction('matrixSetState');
        if (!matrixSet) return;
        const expA = (typeof window.kaigenGetMatrixExpanded === 'function') ? window.kaigenGetMatrixExpanded('A') : '';
        const expB = (typeof window.kaigenGetMatrixExpanded === 'function') ? window.kaigenGetMatrixExpanded('B') : '';
        matrixSet({
          mode: currentMode === 'matrix' ? 'Matrix' : 'Slots',
          expandedA: expA,
          expandedB: expB,
        });
      } catch (e) {}
    }
    window.kaigenSaveMatrixState = saveMatrixState;
    // Save whenever mode changes.
    const origApplyMode = applyMode;
    applyMode = function(mode) { origApplyMode(mode); saveMatrixState(); };
```

In `Source/WebUI/matrix.js`, add the `kaigenGetMatrixExpanded` / `kaigenSetMatrixExpanded` helpers at the bottom of the IIFE near `window.kaigenRenderMatrix`:

```js
  window.kaigenGetMatrixExpanded = (engineId) =>
    Array.from(expandedCats[engineId]).join(',');
  window.kaigenSetMatrixExpanded = (engineId, csv) => {
    expandedCats[engineId] = new Set(
      (csv || '').split(',').map(s => s.trim()).filter(Boolean)
    );
  };
```

- [ ] **Step 9: Build and verify end-to-end**

Run: `build.bat`
Expected: build succeeds. Open Standalone. MATRIX mode → expand SHAPE. Close the Standalone. Reopen → MATRIX mode is restored, SHAPE is still expanded. (If running inside Live, save the project, close, reopen.)

- [ ] **Step 10: Commit**

```bash
git add Source/MatrixViewState.h \
        Source/PluginProcessor.h \
        Source/PluginProcessor.cpp \
        Source/PluginEditor.cpp \
        Source/WebUI/modulation-panel.js \
        Source/WebUI/matrix.js \
        tests/MatrixViewStatePersistenceTest.cpp \
        tests/CMakeLists.txt
git commit -m "feat: persist matrix view (mode + per-engine expanded cats) in plugin state"
```

---

## Task 13: Header counter + slot-click to matrix-mode handoff

Updates the header counter ("N ROUTINGS · M MACROS ACTIVE") on every state change. Wires slot-click in slots view to switch to matrix mode (when clicking a macro slot) and briefly highlight that modulator's row.

**Files:**
- Modify: `Source/WebUI/modulation-panel.js`
- Modify: `Source/WebUI/matrix.js`
- Modify: `Source/WebUI/matrix.css`

- [ ] **Step 1: Counter update logic**

In `Source/WebUI/modulation-panel.js`, near the bottom of `init()`, add:

```js
    function updateCounter() {
      const counter = document.getElementById('modulation-counter');
      if (!counter) return;
      let totalRoutings = 0;
      let activeMacros = 0;
      try {
        const getState = window.Juce.getNativeFunction('modulationGetState');
        const s = getState();
        if (s) {
          for (const engId of ['engineA', 'engineB']) {
            const eng = s[engId];
            if (eng && Array.isArray(eng.routings)) {
              totalRoutings += eng.routings.length;
              const seen = new Set();
              for (const r of eng.routings) {
                if (r.source.startsWith('macro') && !seen.has(r.source)) {
                  activeMacros++;
                  seen.add(r.source);
                }
              }
            }
          }
        }
      } catch (e) {}
      counter.textContent = `${totalRoutings} ROUTING${totalRoutings === 1 ? '' : 'S'}` +
                            (activeMacros > 0 ? ` · ${activeMacros} MACRO${activeMacros === 1 ? '' : 'S'} ACTIVE` : '');
    }
    document.body.addEventListener('modulationStateChanged', updateCounter);
    updateCounter();
```

- [ ] **Step 2: Slot click → matrix-mode + highlight row**

In `Source/WebUI/modulation-panel.js`, find the existing `panel.querySelectorAll('.mod-slot').forEach(...)` block. Replace its inner `btn.addEventListener('click', ...)` with:

```js
      btn.addEventListener('click', () => {
        const id   = btn.getAttribute('data-slot-id');
        const type = btn.getAttribute('data-slot-type');
        // Engine-focus auto-switch (was in openSlot before; preserved here).
        if (type === 'macro') {
          const wantTab = (id === 'macro3' || id === 'macro4') ? 'B' : 'A';
          if (window.__kaigenActiveTab !== wantTab) {
            window.__kaigenActiveTab = wantTab;
            try {
              const f = window.Juce.getNativeFunction('engineSetFocus');
              if (f) f({ activeTab: wantTab, linkOn: !!window.__kaigenLinkOn });
            } catch (e) {}
            const tabA = document.getElementById('engine-tab-a');
            const tabB = document.getElementById('engine-tab-b');
            if (tabA && tabB) {
              tabA.classList.toggle('is-active', wantTab === 'A');
              tabA.setAttribute('aria-selected', wantTab === 'A' ? 'true' : 'false');
              tabB.classList.toggle('is-active', wantTab === 'B');
              tabB.setAttribute('aria-selected', wantTab === 'B' ? 'true' : 'false');
            }
            if (window.Juce && typeof window.Juce.broadcastKaigenTabChanged === 'function') {
              window.Juce.broadcastKaigenTabChanged();
            }
          }
          // Switch to MATRIX mode and ask the matrix to highlight this row.
          applyMode('matrix');
          if (typeof window.kaigenHighlightMatrixRow === 'function') {
            window.kaigenHighlightMatrixRow(id);
          }
        }
        // LFO/Random slots in slots view stay inert in this PR.
      });
```

- [ ] **Step 3: Implement `kaigenHighlightMatrixRow` in `matrix.js`**

In `matrix.js`, near the bottom of the IIFE (above `window.kaigenRenderMatrix = render;`), add:

```js
  window.kaigenHighlightMatrixRow = (modId) => {
    const root = document.getElementById('modulation-matrix');
    if (!root) return;
    const target = root.querySelector(`.mtx-mod[data-mod-id="${modId}"]`);
    if (!target) return;
    target.classList.add('is-highlighted');
    target.scrollIntoView({ block: 'center', behavior: 'smooth' });
    setTimeout(() => target.classList.remove('is-highlighted'), 1500);
  };
```

- [ ] **Step 4: Add highlight CSS**

In `Source/WebUI/matrix.css`, append:

```css
@keyframes mtx-row-highlight {
  0%   { box-shadow: inset 0 0 0 2px rgba(93, 211, 224, 0); }
  20%  { box-shadow: inset 0 0 0 2px rgba(93, 211, 224, 0.85); }
  100% { box-shadow: inset 0 0 0 2px rgba(93, 211, 224, 0); }
}
.mtx-mod.is-highlighted {
  animation: mtx-row-highlight 1.5s ease-out;
}
```

- [ ] **Step 5: Build and verify**

Run: `build.bat`
Expected: build succeeds. Open Standalone, SLOTS mode. Add a routing somewhere (use right-click in MATRIX or temporarily in the still-existing drawer). Header shows "1 ROUTING · 1 MACRO ACTIVE". Add another routing on a different macro → "2 ROUTINGS · 2 MACROS ACTIVE". Remove → counter decrements. Click MAC 2 slot in slots view → matrix mode opens, MAC 2 row briefly highlights with a teal pulse, scrolls into view if necessary.

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/modulation-panel.js Source/WebUI/matrix.js Source/WebUI/matrix.css
git commit -m "feat: header counter + slot-click -> matrix-mode + row highlight"
```

---

## Task 14: Delete drawer code

With the matrix fully functional, removes the drawer + per-macro editor. Deletes `macro-editor.js`, the drawer container in `index.html`, the drawer mechanics in `modulation-panel.js`, drawer CSS in `styles.css`, and the CMake binary-data entry.

**Files:**
- Delete: `Source/WebUI/macro-editor.js`
- Modify: `Source/WebUI/index.html` (remove `#modulation-drawer`, remove `<script src="/macro-editor.js">`)
- Modify: `Source/WebUI/styles.css` (remove drawer + macro-editor styles, ~lines 729-738 + 843-end-of-section)
- Modify: `Source/WebUI/modulation-panel.js` (remove the drawer-mechanics half, the `growEditor` / `shrinkEditor` for the drawer specifically — keep the matrix-mode height handling)
- Modify: `CMakeLists.txt` (remove `Source/WebUI/macro-editor.js`)

- [ ] **Step 1: Delete `macro-editor.js`**

```bash
rm Source/WebUI/macro-editor.js
```

- [ ] **Step 2: Remove the drawer container from `index.html`**

Open `Source/WebUI/index.html`. Delete:

```html
      <div id="modulation-drawer" class="modulation-drawer" aria-hidden="true"></div>
```

Delete the script tag:

```html
<script src="/macro-editor.js"></script>
```

- [ ] **Step 3: Strip drawer styles from `styles.css`**

Remove the `.modulation-drawer` rule (~lines 729–738). Remove the entire "MACRO EDITOR (drawer content)" section starting at the `/* ═══ MACRO EDITOR (drawer content) ═══ */` comment (~line 843) through to the next major section delimiter. Verify no other selectors reference `.modulation-drawer` or `.macro-editor-*`.

- [ ] **Step 4: Reduce `modulation-panel.js`**

Open `Source/WebUI/modulation-panel.js`. Remove every reference to `drawer` and the drawer mechanics:

- Delete the `const drawer = document.getElementById('modulation-drawer');` line.
- Delete the `if (!panel || !drawer) return;` (replace with `if (!panel) return;`).
- Delete the `BASE_HEIGHT`/`DRAWER_EXPANDED`/`DRAWER_TRANSITION_MS` constants and the `growEditor` / `shrinkEditor` functions (the matrix-mode height handling already exists separately in `applyMode`).
- Delete the `closeDrawer` function entirely.
- Delete the `openSlot` function (the drawer-opening logic). The slot-click handler from Task 13 has fully replaced it.
- Delete the `window.kaigenCloseModulationDrawer = closeDrawer;` export.

After edits, the only slot-click logic in this file is the Task 13 handler that switches to matrix mode + highlights the row. Keep all the toggle/counter logic intact.

- [ ] **Step 5: Remove `macro-editor.js` from CMakeLists.txt**

In `CMakeLists.txt`, the `juce_add_binary_data` block — remove the `Source/WebUI/macro-editor.js` line.

- [ ] **Step 6: Build and verify nothing references the deleted files**

Run: `build.bat`
Expected: build succeeds. No "kaigenRenderMacroEditor undefined" or "modulation-drawer not found" warnings in the Standalone console.

- [ ] **Step 7: Verify SLOTS mode behavior is preserved**

Open Standalone, SLOTS mode. Click MAC 1 → switches to MATRIX, MAC 1 row highlighted. Click MAC 2 → MAC 2 highlighted. Click MAC 3 → engine focus switches to B, MAC 3 highlighted. The drawer never appears.

- [ ] **Step 8: Commit**

```bash
git rm Source/WebUI/macro-editor.js
git add CMakeLists.txt \
        Source/WebUI/index.html \
        Source/WebUI/styles.css \
        Source/WebUI/modulation-panel.js
git commit -m "feat: remove drawer + per-macro editor (superseded by matrix view)"
```

---

## Task 15: Manual smoke + final review

Walks the smoke checklist from the spec end-to-end. Catches any regression in the audio path (the user's bug-2 concern from the original PR3b). Adds a top-of-doc note to the original PR3b plan pointing to the revamp.

**Files:**
- Modify: `docs/superpowers/plans/2026-05-05-pr3b-macro-editor.md` (1-line note)

- [ ] **Step 1: Smoke checklist**

In Live (or Reaper, or any DAW with macro automation):

1. Insert Kaigen Phantom on a bass track. Switch to MATRIX mode. Verify both engine blocks render with all 8 categories collapsed except GHOST + RECIPE.
2. Click empty Engine A / Macro 1 / GHOST cell. Cell shows `+50` with teal gradient.
3. Drag the cell upward — value crosses to `+100`. Drag down through `0` — gradient flips, cell shows `-25` then `-100`.
4. Right-click the cell → popover. Click `0` → cell value snaps to `0` (still active, since `0` is a valid depth and the routing exists). Click `Remove` → cell becomes empty.
5. Add Macro 1 × Ghost at +50 again. **Audibly verify the audio path:** play bass through the plugin with Ghost set to a low base value (e.g., 0.2). Automate macro 1 from 0 to 1 — the bass character should change as macro 1 sweeps (Ghost reaching ~0.7 at full macro). If the audio doesn't change, this is the latent bug-2 from the original smoke and needs investigation in `DualEngineHost::syncEngineFromPrefix` — but per spec, the C++ chain is unchanged from PR3a so any failure here points to a binding regression.
6. Repeat the audio test with Engine B / Macro 3 / Ghost — verify engine separation (macro 3 should NOT affect engine A's Ghost).
7. Toggle to SLOTS view → macro 1 ring fills proportional to current value. Drag macro 1 from 0 to 1 in SLOTS → ring fills smoothly at 15 Hz. Morph slot ring tracks morph_amount.
8. In SLOTS, click MAC 4 → MATRIX opens, MAC 4 row highlighted, scrolls into view if needed.
9. Add 5 routings spread across different modulators + destinations. Header counter: "5 ROUTINGS · 3 MACROS ACTIVE" (or whatever applies).
10. Save the project / preset. Close and reopen → MATRIX mode is restored, expanded categories preserved, all 5 routings load with correct depths and bipolar fill directions.
11. Watch the editor's CPU profile for 30 sec while the macros are being automated — should stay near baseline (15 Hz × 4 macros × native bridge round-trip is the load floor; if it spikes above ~5% something is wrong with the polling).

- [ ] **Step 2: Note any regressions in the original PR3b plan**

Open `docs/superpowers/plans/2026-05-05-pr3b-macro-editor.md`. At the very top (immediately after the heading), insert:

```markdown
> **Superseded by PR3b-rev (2026-05-06):** The drawer + per-macro editor described in Tasks 4–6 of this plan was replaced by a Vital/Roar-style matrix view. See `docs/superpowers/specs/2026-05-06-pr3b-rev-matrix-view-design.md` and `docs/superpowers/plans/2026-05-06-pr3b-rev-matrix-view.md`. Tasks 1–3 (C++ framework + native bindings) of this plan are still in effect.
```

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/plans/2026-05-05-pr3b-macro-editor.md
git commit -m "docs: mark drawer-era PR3b plan superseded by matrix view"
```

- [ ] **Step 4: Final spec-coverage self-check**

Re-read `docs/superpowers/specs/2026-05-06-pr3b-rev-matrix-view-design.md` end-to-end. For each "What's new" / "Visual design" / "Data flow" / "Persistence" / "Animations" / "Error handling" item, confirm a Task implements it. Flag any gap to the user before declaring this PR ready for review.

---

## Self-review notes

### Spec coverage

- **Two sibling views (SLOTS / MATRIX) toggled by segmented control** → Task 1 (markup) + Task 3 (handler).
- **SLOTS view live macro/morph rings** → Task 2.
- **Matrix view modulator-row strip with name + animation + value readout** → Task 4 + Task 10 (name editor) + Task 11 (live).
- **Destination columns grouped into 8 expandable categories** → Task 5.
- **Cells with bipolar gradient fill + numeric depth + modulator color** → Task 6.
- **Click empty cell → AddRouting at +50%** → Task 7.
- **Drag active cell vertically → SetRoutingDepth** → Task 8.
- **Right-click cell popover (quick-set + remove)** → Task 9.
- **Inline macro name editor on row label click** → Task 10.
- **Live-modulating cell glow** → Task 11.
- **Persistence (`<MatrixView>` mode + per-engine expanded cats)** → Task 12.
- **Header counter + slot-click → matrix-mode handoff with row highlight** → Task 13.
- **Delete drawer + per-macro editor + supporting code** → Task 14.
- **Audible verification of audio path (bug-2 concern)** → Task 15 step 1 #5.

### Placeholder scan

No "TBD" / "TODO" / "fill in details". Every step has a code block when code is required. Every command has an expected outcome.

### Type consistency

- `MatrixViewState` is consistent across header (Task 12 step 3), test (Task 12 step 1), and processor (Task 12 step 5).
- `MatrixMode::Slots` / `MatrixMode::Matrix` enum used consistently.
- JS-side mode strings (`'slots'` / `'matrix'`) and persisted strings (`'Slots'` / `'Matrix'`) are mapped consistently in the binding (Task 12 step 7) and in `applyMode` (Task 3 step 2).
- `expandedCats` Set semantics match the comma-separated string format used at the binding boundary (Task 12 step 8 helpers).
- Native binding names (`modulationGetState`, `modulationAddRouting`, `modulationRemoveRouting`, `modulationSetRoutingDepth`, `modulationSetMacroName`, `modulationGetLiveState`, `matrixGetState`, `matrixSetState`) are spelled identically everywhere referenced.
- Modulator IDs (`macro1`–`macro4`, `lfo1`–`lfo4`, `randomA`, `randomB`) match `MODULATORS.A` and `MODULATORS.B` in `matrix.js` and the slot `data-slot-id` attributes in `index.html`.
- Param ID prefix logic (`a_` for engine A, `b_` for engine B) is applied identically in cell creation (Task 5/6) and in the binding payloads (Task 7/8/9).
