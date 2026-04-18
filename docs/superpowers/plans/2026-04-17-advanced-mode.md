# Advanced Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an "Advanced mode" toggle that vertically expands the plugin from 820px → 1020px, revealing a 200px-tall circuit-board panel between the knob rows and the oscilloscope. The panel hosts 16 advanced DSP controls via a new compact `<phantom-mini-knob>` component and an animated Canvas2D circuit-board background.

**Architecture:** UI-only change in the WebView, plus one new APVTS boolean (`advanced_open`) for state persistence and one new JUCE native function (`setEditorHeight`) to resize the editor from JS. Basic-mode knob rendering is LOCKED — the advanced knobs are a separate custom element in a separate file.

**Tech Stack:** JUCE 8.0.4 WebView2, HTML/CSS/JS, SVG shadow DOM, Canvas2D, CMake.

**Spec reference:** `docs/superpowers/specs/2026-04-17-advanced-mode-design.md`

---

## File Structure

| File | Responsibility | Type |
|---|---|---|
| `Source/Parameters.h` | Add `ADVANCED_OPEN` bool param | Modify |
| `Source/PluginEditor.h` | Declare helper for editor resize native function | Modify |
| `Source/PluginEditor.cpp` | Register `setEditorHeight` native function | Modify |
| `Source/WebUI/knob-mini.js` | `<phantom-mini-knob>` — 36px dark-theme rotary | Create |
| `Source/WebUI/circuit-board.js` | Canvas2D static traces + animated pulses + LEDs | Create |
| `Source/WebUI/index.html` | Load new scripts; add advanced-panel DOM, header button, seam latch | Modify |
| `Source/WebUI/styles.css` | Advanced-panel layout + transition, dark control styling, seam latch | Modify |
| `Source/WebUI/phantom.js` | Wire mini-knobs to relays; toggle advanced mode; persist state | Modify |
| `tests/ParameterTests.cpp` | Update parameter-count assertion to include new param | Modify |

---

## Build / Verify Reference

Used in most tasks:

**Build VST3:**
```
cmake --build build --config Release --target KaigenPhantom_VST3
```
Runs the CMake build. VST3 auto-installs to `%LOCALAPPDATA%\Programs\Common\VST3\`. Copy to Ableton test path with:
```
cp -r "build/KaigenPhantom_artefacts/Release/VST3/Kaigen Phantom.vst3" "/c/Users/kaislate/Downloads/KAIGEN/"
```

**Run tests:**
```
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe
```

**Inspect WebView in DevTools:** Remote debug on port 9222 (while VST3 is open in Ableton). Use Chrome → `http://localhost:9222`.

---

## Task 1: Add `advanced_open` APVTS parameter

**Files:**
- Modify: `Source/Parameters.h` (ParamID block, ID registry, createParameterLayout)
- Modify: `tests/ParameterTests.cpp:64` (expected param count)

**Context:** The advanced-mode toggle state persists through plugin-state save/load via this parameter. Audio thread never reads it; it's purely UI state surfaced through APVTS so the plugin's saved state captures it.

- [ ] **Step 1: Add the parameter ID constant**

In `Source/Parameters.h`, inside `namespace ParamID`, add a new section near the bottom (before the closing brace):

```cpp
    // ── Advanced mode toggle (UI state; not automated) ────────────────────
    /** True if the advanced controls panel is open. UI-only; DSP never reads. */
    inline constexpr auto ADVANCED_OPEN = "advanced_open";
```

- [ ] **Step 2: Add ID to `getAllParameterIDs()`**

In the same file, add `ParamID::ADVANCED_OPEN,` at the end of the list inside `getAllParameterIDs()` (before the closing `}`).

- [ ] **Step 3: Add parameter to `createParameterLayout()`**

In the same file, at the end of `createParameterLayout` (just before the `return { params.begin(), params.end() };` line), add:

```cpp
    // ── Advanced mode toggle ──────────────────────────────────────────────
    params.push_back(std::make_unique<AudioParameterBool>(
        ParamID::ADVANCED_OPEN, "Advanced Open", false));
```

- [ ] **Step 4: Update the parameter-count test**

In `tests/ParameterTests.cpp`, find the line (around line 64):
```cpp
    REQUIRE( ids.size() == 38u );
```
Change to the current total. Before this task the codebase had 40 params (per an earlier test run), this task adds one, new total is **41**. So replace with:
```cpp
    REQUIRE( ids.size() == 41u );
```
If the number is different when the task runs, update it to whatever the test actually reports.

- [ ] **Step 5: Build tests and run**

Run:
```
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: 50/50 test cases pass.

- [ ] **Step 6: Commit**

```
git add Source/Parameters.h tests/ParameterTests.cpp
git commit -m "feat(params): add advanced_open UI-state parameter"
```

---

## Task 2: Create `<phantom-mini-knob>` skeleton + SVG render

**Files:**
- Create: `Source/WebUI/knob-mini.js`

**Context:** A new compact dark-theme knob custom element, used only in the advanced panel. 36px diameter. Basic-mode knob.js is untouched.

- [ ] **Step 1: Create the file with the complete component**

Create `Source/WebUI/knob-mini.js` with this content:

```js
// phantom-mini-knob — compact dark-mode rotary for the advanced panel.
// Basic-mode knobs are in knob.js (separate component).

const MK_TAU = Math.PI * 2;
const MK_DEG = Math.PI / 180;
const MK_ARC_START = 135;
const MK_ARC_SWEEP = 270;
const MK_ARC_END = MK_ARC_START + MK_ARC_SWEEP;

function mkPolarToXY(cx, cy, r, deg) {
  const rad = deg * MK_DEG;
  return { x: cx + r * Math.cos(rad), y: cy + r * Math.sin(rad) };
}

function mkDescribeArc(cx, cy, r, s, e) {
  const a = mkPolarToXY(cx, cy, r, s);
  const b = mkPolarToXY(cx, cy, r, e);
  const large = (e - s) > 180 ? 1 : 0;
  return `M ${a.x} ${a.y} A ${r} ${r} 0 ${large} 1 ${b.x} ${b.y}`;
}

const MK_TEMPLATE = document.createElement('template');
MK_TEMPLATE.innerHTML = `
<style>
:host {
  display: inline-flex;
  flex-direction: column;
  align-items: center;
  width: 44px;
  height: 60px;
  cursor: ns-resize;
  user-select: none;
  -webkit-user-select: none;
  position: relative;
}
svg { width: 36px; height: 36px; display: block; }
.mini-label {
  margin-top: 3px;
  font-family: 'Kalam', 'Segoe Script', cursive;
  font-size: 8px;
  font-weight: 400;
  letter-spacing: 1px;
  color: #FFFFFF;
  text-align: center;
  line-height: 1;
  text-transform: lowercase;
}
.mini-readout {
  position: absolute;
  top: -16px;
  left: 50%;
  transform: translateX(-50%);
  padding: 1px 4px;
  background: rgba(0,0,0,0.72);
  color: #FFFFFF;
  font-family: 'Courier New', monospace;
  font-size: 9px;
  font-weight: 700;
  border-radius: 2px;
  opacity: 0;
  pointer-events: none;
  transition: opacity 150ms ease;
  white-space: nowrap;
}
.mini-readout.visible { opacity: 1; transition: opacity 80ms ease; }
</style>
<svg viewBox="0 0 36 36"></svg>
<div class="mini-label"></div>
<div class="mini-readout"></div>
`;

class PhantomMiniKnob extends HTMLElement {
  static get observedAttributes() {
    return ['value', 'display-value', 'label', 'default-value', 'data-param'];
  }
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    this.shadowRoot.appendChild(MK_TEMPLATE.content.cloneNode(true));
    this._svg = this.shadowRoot.querySelector('svg');
    this._labelEl = this.shadowRoot.querySelector('.mini-label');
    this._readoutEl = this.shadowRoot.querySelector('.mini-readout');
    this._value = 0;
    this._displayValue = '';
    this._dragging = false;
    this._lastY = 0;
    this._onPointerDown = this._onPointerDown.bind(this);
    this._onPointerMove = this._onPointerMove.bind(this);
    this._onPointerUp = this._onPointerUp.bind(this);
    this._onDblClick = this._onDblClick.bind(this);
  }
  connectedCallback() {
    if (!this.hasAttribute('value')) {
      const def = parseFloat(this.getAttribute('default-value')) || 0;
      this._value = Math.max(0, Math.min(1, def));
    }
    this._labelEl.textContent = (this.getAttribute('label') || '').toLowerCase();
    this._render();
    this.addEventListener('pointerdown', this._onPointerDown);
    this.addEventListener('dblclick', this._onDblClick);
  }
  disconnectedCallback() {
    this.removeEventListener('pointerdown', this._onPointerDown);
    this.removeEventListener('dblclick', this._onDblClick);
    document.removeEventListener('pointermove', this._onPointerMove);
    document.removeEventListener('pointerup', this._onPointerUp);
  }
  attributeChangedCallback(name, _old, val) {
    if (name === 'value') {
      this._value = Math.max(0, Math.min(1, parseFloat(val) || 0));
      this._render();
    } else if (name === 'display-value') {
      this._displayValue = val || '';
      this._render();
    } else if (name === 'label') {
      if (this._labelEl) this._labelEl.textContent = (val || '').toLowerCase();
    }
  }
  get value() { return this._value; }
  set value(v) {
    this._value = Math.max(0, Math.min(1, parseFloat(v) || 0));
    this._render();
  }
  get displayValue() { return this._displayValue; }
  set displayValue(s) {
    this._displayValue = s || '';
    this._render();
  }
  get name() { return this.dataset.param; }
  _onPointerDown(e) {
    if (e.button !== 0) return;
    this._dragging = true;
    this._lastY = e.clientY;
    this.setPointerCapture(e.pointerId);
    document.body.style.pointerEvents = 'none';
    document.addEventListener('pointermove', this._onPointerMove);
    document.addEventListener('pointerup', this._onPointerUp);
    this._readoutEl.classList.add('visible');
    e.preventDefault();
  }
  _onPointerMove(e) {
    if (!this._dragging) return;
    const dy = e.clientY - this._lastY;
    this._lastY = e.clientY;
    const nv = Math.max(0, Math.min(1, this._value + dy * -0.005));
    if (nv !== this._value) {
      this._value = nv;
      this._render();
      this.dispatchEvent(new CustomEvent('knob-change', {
        bubbles: true,
        detail: { name: this.dataset.param, value: this._value }
      }));
    }
  }
  _onPointerUp(e) {
    this._dragging = false;
    document.body.style.pointerEvents = '';
    document.removeEventListener('pointermove', this._onPointerMove);
    document.removeEventListener('pointerup', this._onPointerUp);
    try { this.releasePointerCapture(e.pointerId); } catch (_) {}
    this._readoutEl.classList.remove('visible');
  }
  _onDblClick() {
    const def = parseFloat(this.getAttribute('default-value')) || 0;
    this._value = Math.max(0, Math.min(1, def));
    this._render();
    this.dispatchEvent(new CustomEvent('knob-change', {
      bubbles: true,
      detail: { name: this.dataset.param, value: this._value }
    }));
  }
  _render() {
    const cx = 18, cy = 18, outerR = 16, arcR = 14;
    const valEndDeg = MK_ARC_START + MK_ARC_SWEEP * this._value;
    const trackPath = mkDescribeArc(cx, cy, arcR, MK_ARC_START, MK_ARC_END - 0.01);
    const valPath = this._value > 0.001
      ? mkDescribeArc(cx, cy, arcR, MK_ARC_START, valEndDeg)
      : '';
    // Indicator tick: short radial line from the arc inward by 4px at valEndDeg.
    const tipOuter = mkPolarToXY(cx, cy, arcR - 1, valEndDeg);
    const tipInner = mkPolarToXY(cx, cy, arcR - 6, valEndDeg);
    this._svg.innerHTML = `
      <circle cx="${cx}" cy="${cy}" r="${outerR}" fill="#1A1B1C"/>
      <circle cx="${cx}" cy="${cy}" r="${outerR}" fill="none"
        stroke="rgba(255,255,255,0.15)" stroke-width="0.5"/>
      <path d="${trackPath}" fill="none"
        stroke="rgba(255,255,255,0.08)" stroke-width="2" stroke-linecap="round"/>
      ${valPath ? `<path d="${valPath}" fill="none"
        stroke="#FFFFFF" stroke-width="2.2" stroke-linecap="round"/>` : ''}
      <line x1="${tipOuter.x.toFixed(2)}" y1="${tipOuter.y.toFixed(2)}"
        x2="${tipInner.x.toFixed(2)}" y2="${tipInner.y.toFixed(2)}"
        stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round"/>
    `;
    this._readoutEl.textContent = this._displayValue || this._value.toFixed(2);
  }
}

customElements.define('phantom-mini-knob', PhantomMiniKnob);
```

- [ ] **Step 2: Verify file created and syntactically valid**

Run:
```
node --check "Source/WebUI/knob-mini.js"
```

Expected: no output (means no syntax errors). If `node` isn't available, skip and rely on the build step catching issues.

- [ ] **Step 3: Commit**

```
git add Source/WebUI/knob-mini.js
git commit -m "feat(ui): add phantom-mini-knob component for advanced mode"
```

---

## Task 3: Load `knob-mini.js` in `index.html`

**Files:**
- Modify: `Source/WebUI/index.html` (add `<script>` tag for the new file)

- [ ] **Step 1: Add the script tag**

In `Source/WebUI/index.html`, find the `<script src="/knob.js"></script>` line (near the bottom). Immediately after it, add:

```html
<script src="/knob-mini.js"></script>
```

- [ ] **Step 2: Build + verify**

Run:
```
cmake --build build --config Release --target KaigenPhantom_VST3
```

Build should succeed. No visual change yet (the element isn't used in the layout).

- [ ] **Step 3: Commit**

```
git add Source/WebUI/index.html
git commit -m "feat(ui): load knob-mini.js in plugin HTML"
```

---

## Task 4: Create `circuit-board.js` — static layer only

**Files:**
- Create: `Source/WebUI/circuit-board.js`

**Context:** First pass of the circuit-board visual draws only the static PCB traces and connection dots. Animated pulses come in Task 5.

- [ ] **Step 1: Create the file with static-only rendering**

Create `Source/WebUI/circuit-board.js`:

```js
// Circuit-board visual for the advanced panel. Exposes window.PhantomCircuitBoard
// with start()/stop() methods. start() kicks off rAF animation; stop() pauses it.
// Task 4: static draw only. Task 5 adds pulses + LEDs.

(function() {
  let canvas = null, ctx = null;
  let width = 0, height = 0;
  let traces = [];     // [{ points: [[x,y],[x,y],...] }, ...]
  let joints = [];     // [{ x, y }]
  let rafId = 0;
  let running = false;

  function resize() {
    if (!canvas) return;
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    width = rect.width;
    height = rect.height;
    canvas.width = Math.round(width * dpr);
    canvas.height = Math.round(height * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    buildTraces();
    drawStatic();
  }

  function buildTraces() {
    // A hand-authored trace layout. Percent-based so it scales with canvas size.
    // Each trace is a list of [x%, y%] points; drawn as polyline.
    const defs = [
      [[2, 50], [15, 50], [15, 20], [30, 20]],
      [[30, 20], [45, 20], [45, 75], [60, 75]],
      [[60, 75], [75, 75], [75, 40], [98, 40]],
      [[2, 80], [20, 80], [20, 90], [50, 90]],
      [[50, 90], [80, 90], [80, 55], [98, 55]],
      [[10, 10], [25, 10], [25, 35], [40, 35]],
      [[55, 30], [70, 30], [70, 10], [90, 10]],
      [[8, 60], [8, 35], [18, 35]],
    ];
    traces = defs.map(points => ({
      points: points.map(([px, py]) => [px * width / 100, py * height / 100])
    }));
    // Joints are the endpoints of each trace segment.
    const jset = new Map();
    for (const tr of traces) {
      for (const [x, y] of tr.points) {
        const key = `${Math.round(x)}_${Math.round(y)}`;
        if (!jset.has(key)) jset.set(key, { x, y });
      }
    }
    joints = [...jset.values()];
  }

  function drawStatic() {
    ctx.clearRect(0, 0, width, height);

    // Background.
    ctx.fillStyle = '#0A0B0C';
    ctx.fillRect(0, 0, width, height);

    // Grid of faint dots.
    ctx.fillStyle = 'rgba(255,255,255,0.08)';
    for (let x = 15; x < width; x += 30) {
      for (let y = 15; y < height; y += 30) {
        ctx.fillRect(x, y, 1, 1);
      }
    }

    // Traces.
    ctx.strokeStyle = 'rgba(255,255,255,0.18)';
    ctx.lineWidth = 1;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    for (const tr of traces) {
      ctx.beginPath();
      ctx.moveTo(tr.points[0][0], tr.points[0][1]);
      for (let i = 1; i < tr.points.length; i++) {
        ctx.lineTo(tr.points[i][0], tr.points[i][1]);
      }
      ctx.stroke();
    }

    // Solder pads at joints.
    ctx.fillStyle = 'rgba(255,255,255,0.20)';
    for (const j of joints) {
      ctx.beginPath();
      ctx.arc(j.x, j.y, 2, 0, Math.PI * 2);
      ctx.fill();
    }
  }

  function tick() {
    // Task 4: nothing animated yet.
    if (!running) return;
    rafId = requestAnimationFrame(tick);
  }

  window.PhantomCircuitBoard = {
    start(canvasEl) {
      if (running) return;
      canvas = canvasEl;
      ctx = canvas.getContext('2d');
      resize();
      window.addEventListener('resize', resize);
      running = true;
      rafId = requestAnimationFrame(tick);
    },
    stop() {
      running = false;
      if (rafId) cancelAnimationFrame(rafId);
      window.removeEventListener('resize', resize);
    }
  };
})();
```

- [ ] **Step 2: Commit**

```
git add Source/WebUI/circuit-board.js
git commit -m "feat(ui): circuit-board static layer — traces + joints"
```

---

## Task 5: Add animated pulses + LED flashes to circuit-board

**Files:**
- Modify: `Source/WebUI/circuit-board.js`

- [ ] **Step 1: Replace the `tick()` function and add pulse/LED state**

Open `Source/WebUI/circuit-board.js`. Find the `function tick()` block and the surrounding state region. Replace the state block (`let traces = []; let joints = []; ...`) and the `tick` function with the expanded version:

Find this block (currently near the top of the IIFE):

```js
  let canvas = null, ctx = null;
  let width = 0, height = 0;
  let traces = [];     // [{ points: [[x,y],[x,y],...] }, ...]
  let joints = [];     // [{ x, y }]
  let rafId = 0;
  let running = false;
```

Add three more state variables after `running`:

```js
  let canvas = null, ctx = null;
  let width = 0, height = 0;
  let traces = [];     // [{ points: [[x,y],[x,y],...], length: number }, ...]
  let joints = [];     // [{ x, y, flashUntil: number }]
  let pulses = [];     // [{ traceIdx, t: 0..1, speed, onArrive: (joint)=>void }]
  let rafId = 0;
  let lastFrameMs = 0;
  let nextSpawnMs = 0;
  let running = false;
  const MAX_PULSES = 5;
```

Find the `buildTraces()` function and replace it entirely with this version (adds pre-computed trace length for constant-speed pulse animation):

```js
  function buildTraces() {
    const defs = [
      [[2, 50], [15, 50], [15, 20], [30, 20]],
      [[30, 20], [45, 20], [45, 75], [60, 75]],
      [[60, 75], [75, 75], [75, 40], [98, 40]],
      [[2, 80], [20, 80], [20, 90], [50, 90]],
      [[50, 90], [80, 90], [80, 55], [98, 55]],
      [[10, 10], [25, 10], [25, 35], [40, 35]],
      [[55, 30], [70, 30], [70, 10], [90, 10]],
      [[8, 60], [8, 35], [18, 35]],
    ];
    traces = defs.map(points => {
      const p = points.map(([px, py]) => [px * width / 100, py * height / 100]);
      let len = 0;
      for (let i = 1; i < p.length; i++) {
        const dx = p[i][0] - p[i-1][0];
        const dy = p[i][1] - p[i-1][1];
        len += Math.hypot(dx, dy);
      }
      return { points: p, length: len };
    });
    const jset = new Map();
    for (const tr of traces) {
      for (const [x, y] of tr.points) {
        const key = `${Math.round(x)}_${Math.round(y)}`;
        if (!jset.has(key)) jset.set(key, { x, y, flashUntil: 0 });
      }
    }
    joints = [...jset.values()];
  }
```

Add this helper function above `tick()`:

```js
  function pointOnTrace(tr, t) {
    const target = tr.length * Math.max(0, Math.min(1, t));
    let travelled = 0;
    for (let i = 1; i < tr.points.length; i++) {
      const a = tr.points[i-1], b = tr.points[i];
      const seg = Math.hypot(b[0]-a[0], b[1]-a[1]);
      if (travelled + seg >= target) {
        const local = (target - travelled) / seg;
        return [a[0] + (b[0]-a[0]) * local, a[1] + (b[1]-a[1]) * local];
      }
      travelled += seg;
    }
    const last = tr.points[tr.points.length - 1];
    return [last[0], last[1]];
  }

  function spawnPulse() {
    if (pulses.length >= MAX_PULSES || traces.length === 0) return;
    const traceIdx = Math.floor(Math.random() * traces.length);
    // 120 px/sec → speed as fraction of trace length per millisecond.
    const speed = 120 / Math.max(1, traces[traceIdx].length) / 1000;
    pulses.push({ traceIdx, t: 0, speed });
  }

  function flashNearestJoint(x, y) {
    let best = null, bestD = Infinity;
    for (const j of joints) {
      const d = Math.hypot(j.x - x, j.y - y);
      if (d < bestD) { bestD = d; best = j; }
    }
    if (best) best.flashUntil = performance.now() + 300;
  }

  function drawAnimated(nowMs) {
    // Re-draw static layer first (clears previous animation frame).
    drawStatic();

    // Draw LEDs — flash if recently hit.
    for (const j of joints) {
      const remaining = j.flashUntil - nowMs;
      const alpha = remaining > 0
        ? 0.20 + 0.80 * (remaining / 300)
        : 0.20;
      ctx.fillStyle = `rgba(255,255,255,${alpha})`;
      ctx.beginPath();
      ctx.arc(j.x, j.y, 3, 0, Math.PI * 2);
      ctx.fill();
      if (remaining > 0) {
        ctx.shadowColor = 'rgba(255,255,255,0.8)';
        ctx.shadowBlur = 6;
        ctx.beginPath();
        ctx.arc(j.x, j.y, 3, 0, Math.PI * 2);
        ctx.fill();
        ctx.shadowBlur = 0;
      }
    }

    // Draw pulses.
    for (const p of pulses) {
      const tr = traces[p.traceIdx];
      const [x, y] = pointOnTrace(tr, p.t);
      ctx.shadowColor = 'rgba(255,255,255,0.9)';
      ctx.shadowBlur = 8;
      ctx.fillStyle = '#FFFFFF';
      ctx.beginPath();
      ctx.arc(x, y, 2.5, 0, Math.PI * 2);
      ctx.fill();
      ctx.shadowBlur = 0;
    }
  }
```

Replace `tick()` with:

```js
  function tick(nowMs) {
    if (!running) return;
    const dt = lastFrameMs ? (nowMs - lastFrameMs) : 16;
    lastFrameMs = nowMs;

    // Spawn a new pulse on the schedule.
    if (nowMs >= nextSpawnMs) {
      spawnPulse();
      nextSpawnMs = nowMs + 800 + Math.random() * 700;
    }

    // Advance existing pulses; remove completed ones, flashing at their endpoint.
    for (let i = pulses.length - 1; i >= 0; i--) {
      const p = pulses[i];
      p.t += p.speed * dt;
      if (p.t >= 1) {
        const tr = traces[p.traceIdx];
        const end = tr.points[tr.points.length - 1];
        flashNearestJoint(end[0], end[1]);
        pulses.splice(i, 1);
      }
    }

    drawAnimated(nowMs);
    rafId = requestAnimationFrame(tick);
  }
```

Update `start()` to reset animation state:

Find:
```js
    start(canvasEl) {
      if (running) return;
      canvas = canvasEl;
      ctx = canvas.getContext('2d');
      resize();
      window.addEventListener('resize', resize);
      running = true;
      rafId = requestAnimationFrame(tick);
    },
```

Replace with:
```js
    start(canvasEl) {
      if (running) return;
      canvas = canvasEl;
      ctx = canvas.getContext('2d');
      resize();
      window.addEventListener('resize', resize);
      running = true;
      pulses = [];
      lastFrameMs = 0;
      nextSpawnMs = performance.now() + 300;
      rafId = requestAnimationFrame(tick);
    },
```

- [ ] **Step 2: Commit**

```
git add Source/WebUI/circuit-board.js
git commit -m "feat(ui): circuit-board pulses + LED flashes"
```

---

## Task 6: Advanced-panel DOM in `index.html`

**Files:**
- Modify: `Source/WebUI/index.html` (add the advanced panel + header button + seam latch)

**Context:** Wire every advanced parameter from spec §2 to a `<phantom-mini-knob>` with the correct `data-param` and `default-value`. The defaults mirror the parameter values declared in `Source/Parameters.h`.

- [ ] **Step 1: Add the header advanced button**

In `Source/WebUI/index.html`, find the line:
```html
<button class="hdr-btn" id="settings-btn" title="Settings">&#x2699;</button>
```

Immediately after it, add:

```html
<button class="hdr-btn" id="advanced-btn" title="Advanced controls">
  <svg viewBox="0 0 14 14" width="14" height="14">
    <path d="M2 5 L7 10 L12 5" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>
  </svg>
</button>
```

- [ ] **Step 2: Load the circuit-board script**

Find the line `<script src="/oscilloscope.js"></script>` near the bottom of `index.html`. Immediately after it, add:

```html
<script src="/circuit-board.js"></script>
```

- [ ] **Step 3: Add the advanced panel DOM**

In `index.html`, find the closing `</div>` of the last `ctrl-row` block (the one containing the Output panel). Immediately AFTER that `</div>` and BEFORE the `<div class="gap-h"></div>` that precedes the viz-section, insert the advanced panel. To find the insertion point precisely: look for `<!-- Visualization: meters span full height, osc+spectrum stacked in center -->`. The advanced panel goes BEFORE the `<div class="gap-h"></div>` immediately above that comment.

Insert:

```html
        <!-- Advanced panel — revealed by #advanced-btn / #seam-latch -->
        <div class="advanced-panel" id="advancedPanel">
          <canvas id="circuitCanvas" class="circuit-canvas"></canvas>
          <div class="adv-content">
            <div class="adv-section adv-phantom">
              <div class="adv-section-label">Phantom</div>
              <div class="adv-knob-row">
                <phantom-mini-knob data-param="phantom_threshold" label="Crossover" default-value="0.462"></phantom-mini-knob>
              </div>
            </div>
            <div class="preset-sep adv-sep"></div>
            <div class="adv-section adv-wavelet">
              <div class="adv-section-label">Wavelet</div>
              <div class="adv-knob-row">
                <phantom-mini-knob data-param="synth_duty" label="Push" default-value="0.5"></phantom-mini-knob>
                <phantom-mini-knob data-param="synth_h1" label="H1" default-value="0.5"></phantom-mini-knob>
                <phantom-mini-knob data-param="synth_sub" label="Sub" default-value="0"></phantom-mini-knob>
                <phantom-mini-knob data-param="synth_wavelet_length" label="Length" default-value="1"></phantom-mini-knob>
                <phantom-mini-knob data-param="synth_gate_threshold" label="Gate" default-value="0"></phantom-mini-knob>
                <phantom-mini-knob data-param="synth_min_samples" label="Min" default-value="0"></phantom-mini-knob>
                <phantom-mini-knob data-param="synth_max_samples" label="Max" default-value="0.9"></phantom-mini-knob>
                <phantom-mini-knob data-param="tracking_speed" label="Track" default-value="0.5"></phantom-mini-knob>
              </div>
            </div>
            <div class="preset-sep adv-sep"></div>
            <div class="adv-section adv-punch-env">
              <div class="adv-section-label">Punch &amp; Envelope</div>
              <div class="adv-knob-row">
                <phantom-mini-knob data-param="punch_amount" label="Amount" default-value="1"></phantom-mini-knob>
                <phantom-mini-knob data-param="synth_boost_threshold" label="Threshold" default-value="0"></phantom-mini-knob>
                <phantom-mini-knob data-param="synth_boost_amount" label="Boost" default-value="0"></phantom-mini-knob>
                <phantom-mini-knob data-param="env_attack_ms" label="Attack" default-value="0.05"></phantom-mini-knob>
                <phantom-mini-knob data-param="env_release_ms" label="Release" default-value="0.1"></phantom-mini-knob>
              </div>
            </div>
            <div class="preset-sep adv-sep"></div>
            <div class="adv-section adv-binaural">
              <div class="adv-section-label">Binaural</div>
              <div class="adv-knob-row">
                <select id="binaural-mode-select-adv" class="param-select adv-select" data-param="binaural_mode">
                  <option value="0">Off</option>
                  <option value="1">Spread</option>
                  <option value="2">Voice-Split</option>
                </select>
                <phantom-mini-knob data-param="binaural_width" label="Width" default-value="0.5"></phantom-mini-knob>
              </div>
            </div>
          </div>
        </div>
```

- [ ] **Step 4: Add the seam latch**

In the same file, find `<div class="viz-section">`. IMMEDIATELY INSIDE it, as the FIRST child (before `<canvas id="meterIn" ...>`), insert:

```html
          <div class="seam-latch" id="seamLatch" title="Advanced controls">
            <span class="seam-dot"></span>
            <span class="seam-dot"></span>
          </div>
```

- [ ] **Step 5: Build + verify**

Run:
```
cmake --build build --config Release --target KaigenPhantom_VST3
```

Build should succeed. Open the plugin in Ableton. The advanced panel won't be styled yet — but the DOM should exist (`Inspect` should reveal the elements). The header gets a new button with a chevron, the viz-section has a small latch at its top. Layout may be broken temporarily because the advanced panel is unstyled — fixed in the next task.

- [ ] **Step 6: Commit**

```
git add Source/WebUI/index.html
git commit -m "feat(ui): advanced-panel DOM + header button + seam latch"
```

---

## Task 7: Advanced-panel CSS — layout, dark theme, transitions

**Files:**
- Modify: `Source/WebUI/styles.css` (append new block)

- [ ] **Step 1: Append the advanced-panel stylesheet**

At the END of `Source/WebUI/styles.css` (after the existing `.settings-panel .param-select option` rule), append:

```css
/* ═══ ADVANCED PANEL ═══ */
.advanced-panel{
  grid-column:1/-1;
  max-height:0;opacity:0;
  overflow:hidden;
  position:relative;
  background:#0A0B0C;
  transform:scaleY(0);
  transform-origin:center;
  transition:max-height 400ms ease-out, opacity 400ms ease-out, transform 400ms ease-out;
  margin:0;
}
.wrap.advanced-open .advanced-panel{
  max-height:200px;
  opacity:1;
  transform:scaleY(1);
}
.circuit-canvas{
  position:absolute;inset:0;width:100%;height:100%;
  display:block;z-index:0;pointer-events:none;
  opacity:0;transition:opacity 400ms ease;
}
.wrap.advanced-open .circuit-canvas{opacity:1;}

.adv-content{
  position:relative;z-index:2;
  height:200px;
  display:flex;flex-direction:row;align-items:stretch;
  padding:0 20px;gap:0;
}
.adv-section{
  display:flex;flex-direction:column;align-items:center;justify-content:center;
  padding:0 12px;gap:6px;
}
.adv-section-label{
  font-size:9px;font-weight:600;letter-spacing:2px;text-transform:uppercase;
  color:rgba(255,255,255,0.32);
  text-shadow:0 1px 0 rgba(0,0,0,0.45);
}
.adv-knob-row{
  display:flex;flex-direction:row;align-items:center;gap:6px;
}
.adv-sep{
  width:2px;height:80%;align-self:center;margin:0 6px;
  background:linear-gradient(90deg,
    rgba(255,255,255,0.22) 0%,
    rgba(255,255,255,0.22) 50%,
    rgba(0,0,0,0.65) 50%,
    rgba(0,0,0,0.65) 100%);
}
.adv-select{
  background:rgba(255,255,255,0.06);color:rgba(255,255,255,0.85);
  border:none;border-radius:3px;padding:2px 6px;font-size:10px;
  font-family:'Courier New',monospace;
}

/* ═══ HEADER ADVANCED BUTTON ═══ */
#advanced-btn svg{transition:transform 250ms ease;}
#advanced-btn.active svg{transform:rotate(180deg);}

/* ═══ SEAM LATCH ═══ */
.viz-section{position:relative;}
.seam-latch{
  position:absolute;top:-10px;left:50%;transform:translateX(-50%);
  width:80px;height:16px;border-radius:8px;
  background:rgba(0,0,0,0.08);
  box-shadow:inset 1.5px 1.5px 4px rgba(0,0,0,0.14),
    inset -1.5px -1.5px 3px rgba(255,255,255,0.48),
    0 1px 3px rgba(0,0,0,0.07);
  display:flex;align-items:center;justify-content:center;gap:10px;
  cursor:pointer;z-index:20;
  transition:background 200ms ease;
}
.seam-dot{
  width:3px;height:3px;border-radius:50%;
  background:rgba(0,0,0,0.32);transition:background 200ms, box-shadow 200ms;
}
.seam-latch:hover .seam-dot{
  background:rgba(255,255,255,0.88);
  box-shadow:0 0 4px rgba(255,255,255,0.6);
}
```

- [ ] **Step 2: Move the advanced panel outside of the right-panel flex**

The advanced panel needs to span the full plugin width and sit between `.right-panel` content and the `.viz-section` . Re-check the DOM structure in `index.html`: the advanced panel is currently inside `.right-panel` (between the knob ctrl-rows and the viz-section). That's correct — no structural change needed.

- [ ] **Step 3: Build + visual verify**

Run:
```
cmake --build build --config Release --target KaigenPhantom_VST3
cp -r "build/KaigenPhantom_artefacts/Release/VST3/Kaigen Phantom.vst3" "/c/Users/kaislate/Downloads/KAIGEN/"
```

Load in Ableton. Expected:
- Plugin still 1600×820.
- Advanced panel is collapsed (invisible — `max-height:0`).
- New chevron button appears in the header.
- Seam latch visible at top of the oscilloscope.
- No behavior on click yet (wired in the next task).

- [ ] **Step 4: Commit**

```
git add Source/WebUI/styles.css
git commit -m "feat(ui): advanced-panel CSS — collapse/expand + dark theme + seam latch"
```

---

## Task 8: Add `setEditorHeight` JUCE native function

**Files:**
- Modify: `Source/PluginEditor.cpp` (add to `buildWebViewOptions` lambda chain)

**Context:** JavaScript calls this to resize the editor from 820 → 1020 or back. JUCE posts a message-thread callback that calls `setSize()` on `self` (the editor), which triggers a DAW repaint + resize.

- [ ] **Step 1: Add the native function**

In `Source/PluginEditor.cpp`, find the `.withNativeFunction("getOscilloscopeData", ...)` block (around line 168). Immediately after its closing `)` (and before the `.withResourceProvider(...)` line at ~189), add a new `.withNativeFunction` call:

```cpp
        .withNativeFunction("setEditorHeight",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                const int height = args.size() > 0 ? (int) args[0] : 820;
                const int clamped = juce::jlimit(400, 2000, height);
                juce::MessageManager::callAsync([weakSelf = juce::Component::SafePointer<PhantomEditor>(&self), clamped]
                {
                    if (auto* p = weakSelf.getComponent())
                        p->setSize(1600, clamped);
                });
                complete(juce::var(true));
            })
```

- [ ] **Step 2: Build + verify**

Run:
```
cmake --build build --config Release --target KaigenPhantom_VST3
```

Build should succeed. No behaviour change in UI yet — the function isn't called.

- [ ] **Step 3: Commit**

```
git add Source/PluginEditor.cpp
git commit -m "feat(editor): expose setEditorHeight native function to JS"
```

---

## Task 9: Wire advanced-mode toggle in `phantom.js`

**Files:**
- Modify: `Source/WebUI/phantom.js` (add toggle logic, register mini-knobs with relays, persist state)

- [ ] **Step 1: Register mini-knob params alongside regular knobs**

In `Source/WebUI/phantom.js`, find the block:

```js
document.querySelectorAll("phantom-knob[data-param]").forEach((el) => {
```

Change the selector so it matches both components by updating to:

```js
document.querySelectorAll("phantom-knob[data-param], phantom-mini-knob[data-param]").forEach((el) => {
```

The rest of the loop body works unchanged because `phantom-mini-knob` has the same API (`el.value`, `el.displayValue`, `knob-change` event).

- [ ] **Step 2: Append the toggle-advanced logic at the end of phantom.js**

Open `Source/WebUI/phantom.js`. Go to the very end of the file. Append:

```js
// =============================================================================
// Advanced mode toggle — coordinates class flip, circuit-board rAF, editor resize
// =============================================================================

const ADV_OPEN_PARAM = 'advanced_open';

function readAdvancedOpen() {
  const state = getSliderState ? getSliderState(ADV_OPEN_PARAM) : null;
  if (!state) return false;
  return state.getNormalisedValue() > 0.5;
}

function writeAdvancedOpen(isOpen) {
  const state = getSliderState ? getSliderState(ADV_OPEN_PARAM) : null;
  if (!state) return;
  state.sliderDragStarted();
  state.setNormalisedValue(isOpen ? 1.0 : 0.0);
  state.sliderDragEnded();
}

function setEditorHeightViaNative(h) {
  if (window.juce && typeof window.juce.getNativeFunction === 'function') {
    try {
      window.juce.getNativeFunction('setEditorHeight')(h);
    } catch (e) {
      console.warn('setEditorHeight native not available yet', e);
    }
  }
}

let circuitCanvasEl = null;
function getCircuitCanvas() {
  if (!circuitCanvasEl) circuitCanvasEl = document.getElementById('circuitCanvas');
  return circuitCanvasEl;
}

function applyAdvancedMode(isOpen, { persist = true } = {}) {
  const wrap = document.querySelector('.wrap');
  const btn = document.getElementById('advanced-btn');
  if (!wrap) return;

  if (isOpen) {
    wrap.classList.add('advanced-open');
    if (btn) btn.classList.add('active');
    setEditorHeightViaNative(1020);
    const canvas = getCircuitCanvas();
    if (canvas && window.PhantomCircuitBoard) {
      window.PhantomCircuitBoard.start(canvas);
    }
  } else {
    wrap.classList.remove('advanced-open');
    if (btn) btn.classList.remove('active');
    if (window.PhantomCircuitBoard) window.PhantomCircuitBoard.stop();
    // Resize the editor AFTER the CSS transition completes so the window only
    // shrinks once the panel is hidden.
    setTimeout(() => setEditorHeightViaNative(820), 400);
  }

  if (persist) writeAdvancedOpen(isOpen);
}

function toggleAdvanced() {
  const wrap = document.querySelector('.wrap');
  if (!wrap) return;
  const isOpen = wrap.classList.contains('advanced-open');
  applyAdvancedMode(!isOpen);
}

// Wire click affordances.
const advancedBtn = document.getElementById('advanced-btn');
if (advancedBtn) advancedBtn.addEventListener('click', toggleAdvanced);

const seamLatchEl = document.getElementById('seamLatch');
if (seamLatchEl) seamLatchEl.addEventListener('click', toggleAdvanced);

// Wire binaural-mode-select-adv to the same param as the settings-overlay select.
const binauralModeSelectAdv = document.getElementById('binaural-mode-select-adv');
if (binauralModeSelectAdv) {
  const state = getSliderState('binaural_mode');
  if (state) {
    binauralModeSelectAdv.addEventListener('change', (e) => {
      state.sliderDragStarted();
      state.setNormalisedValue(parseInt(e.target.value, 10) / 2.0);
      state.sliderDragEnded();
    });
    state.valueChangedEvent.addListener(() => {
      binauralModeSelectAdv.value = String(Math.round(state.getNormalisedValue() * 2));
    });
    binauralModeSelectAdv.value = String(Math.round(state.getNormalisedValue() * 2));
  }
}

// Initialise state from APVTS. If advanced_open was true when the plugin was
// saved, restore that on load.
(function initAdvancedFromState() {
  const state = getSliderState ? getSliderState(ADV_OPEN_PARAM) : null;
  if (!state) return;
  const applyFromState = () => applyAdvancedMode(readAdvancedOpen(), { persist: false });
  state.valueChangedEvent.addListener(applyFromState);
  // Defer by a tick so the DOM + Canvas are ready.
  setTimeout(applyFromState, 50);
})();
```

- [ ] **Step 3: Build + visual verify**

Run:
```
cmake --build build --config Release --target KaigenPhantom_VST3
cp -r "build/KaigenPhantom_artefacts/Release/VST3/Kaigen Phantom.vst3" "/c/Users/kaislate/Downloads/KAIGEN/"
```

Load in Ableton (fully restart to bust VST3 cache). Test:
1. Click header advanced button → plugin should grow to 1020px tall, circuit-board panel visible with animated pulses, all 16 mini-knobs visible.
2. Click it again → plugin collapses back to 820px.
3. Click seam latch → same toggle behavior.
4. Drag any advanced knob (e.g., Push) → parameter responds, value persists.
5. Save plugin state with advanced open, reload → advanced should be open again.

- [ ] **Step 4: Commit**

```
git add Source/WebUI/phantom.js
git commit -m "feat(ui): wire advanced mode toggle + mini-knob relays + state"
```

---

## Task 10: Acceptance walkthrough

**Files:**
- No changes unless a regression is found.

- [ ] **Step 1: Verify spec §7 acceptance criteria**

Load in Ableton. For each criterion:

1. `advanced_open = false` default → plugin 1600×820, advanced panel collapsed. ✓ visible state.
2. Click either affordance with `advanced_open = false` → after ~450ms plugin is 1600×1020, 16 controls visible with circuit-board animation.
3. Click again → plugin collapses back to 1600×820, animation stops.
4. All 16 advanced controls wired (saw dragging any moves the APVTS param).
5. Saving state with `advanced_open = true` + reload → restores open state.
6. Basic-mode knobs visually/behaviorally unchanged. Compare side-by-side with commit `72dc288` (last basic-mode knob change).
7. `<phantom-mini-knob>` supports drag / double-click reset.
8. All existing tests still pass: run `./build/tests/Release/KaigenPhantomTests.exe` → 50/50 pass.

- [ ] **Step 2: If any criterion fails, fix and commit**

Each fix is its own `fix(ui): ...` commit. Typical issues to watch for:
- If panel content overlaps the canvas z-index: verify `.adv-content { z-index: 2 }` is higher than `.circuit-canvas { z-index: 0 }`.
- If the canvas is blank on open: check DevTools console for errors in `circuit-board.js`; verify `canvas.getContext('2d')` returned non-null and `resize()` set width/height > 0.
- If the resize doesn't happen: DevTools console should show `setEditorHeight` being called. If not, the native function isn't registered — re-check Task 8.
- If the seam latch blocks clicks on the meter canvas beneath: reposition top offset or reduce latch width.

- [ ] **Step 3: Done**

No trailing commit needed if nothing regressed.

---

## Self-Review

**Spec coverage check (every §):**
- §1 Architecture — Task 1 (param), Task 9 (class toggle, persistence), Task 8 (setEditorHeight).
- §2 Layout — Task 6 (DOM), Task 7 (CSS).
- §3 Mini-knob — Task 2 (component), Task 3 (load), Task 9 (relay wiring).
- §4 Circuit board — Task 4 (static), Task 5 (animated).
- §5 Break-apart transition — Task 7 (CSS transitions), Task 9 (JS coordination + delayed shrink).
- §6 Toggle UI — Task 6 (DOM), Task 7 (CSS), Task 9 (click wiring).
- §7 Acceptance — Task 10.
- §8 Out of scope — nothing to implement, correct.

**Placeholder scan:** No TBDs, no vague "handle error", no "similar to Task N". Every step has complete code or a precise grep+edit instruction.

**Type/name consistency:**
- `toggleAdvanced` defined in Task 9, called by buttons wired in Task 9. ✓
- `applyAdvancedMode` defined in Task 9 with `{ persist = true }` option; called by `initAdvancedFromState` with `{ persist: false }`. ✓
- `PhantomCircuitBoard.start(canvas)` / `.stop()` exposed in Tasks 4/5, called from Task 9. ✓
- Param `advanced_open` defined Task 1, read/written Task 9 via `getSliderState`. ✓
- `phantom-mini-knob` tag defined Task 2, used in DOM Task 6, registered in phantom.js Task 9. ✓
- File path `/circuit-board.js` loaded in Task 6 matches path created in Task 4. ✓
