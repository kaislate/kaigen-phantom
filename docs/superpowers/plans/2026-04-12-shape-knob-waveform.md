# Shape Knob Waveform Display Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render a live sine→square waveform inside the Shape knob's OLED well, with a small numeric value below it, using the exact DSP math replicated in JS.

**Architecture:** Two pure frontend changes — add three JS math helpers to `knob.js` and a waveform rendering branch in `_render()`, then change one attribute in `index.html`. No DSP, no CSS, no build changes.

**Tech Stack:** Vanilla JS, SVG, custom elements (PhantomKnob web component)

---

## File Map

- Modify: `Source/WebUI/knob.js` — add `warpPhase`, `shapedWave`, `buildWaveformPoints` helpers; add waveform OLED branch in `_render()`
- Modify: `Source/WebUI/index.html` — change Shape knob to `size="large"` and add `data-oled="waveform"`

---

## Key Facts for the Implementer

**knob.js structure (line references match the file at time of spec):**
- Lines 1–21: module-scope constants and helpers (`polarToXY`, `describeArc`)
- Line 23: `TEMPLATE` definition starts
- Lines 178–266: `_render()` method
- `_render()` uses `this._value` (normalised 0–1) and `this.getAttribute('data-oled')` is not yet checked anywhere

**Large knob geometry** (what Shape will be after this plan):
- `sz=100`, `cx=cy=50`, `oledR=37` (volcanoR - inset = 50 - 13)
- OLED circle: centre (50,50), radius 37 → spans y=[13,87] at x=50

**Waveform layout within OLED:**
- x range: `[cx-(oledR-5), cx+(oledR-5)]` = `[18, 82]`
- waveform zero-line y: `cy - oledR*0.15` ≈ 44.4 (upper portion)
- waveform amplitude: `oledR*0.45` ≈ 16.65 → peaks at y≈27.8, troughs at y≈61.1 (both inside circle)
- number y: `cy + oledR*0.55` ≈ 70.4 (lower zone, comfortably inside circle)
- number font-size: 9px (vs normal 12px for large)
- No inner SVG label in waveform mode — the `.label-below` div already shows "SHAPE"

**DSP math (exact replica from `ZeroCrossingSynth.cpp` lines 77–106):**
- `step` = `this._value` (0–1), duty fixed at 0.5 for visual preview
- `drive = 1 + step * 19`; output = `tanh(sin(warpedPhase) * drive) / tanh(drive)`

**`_render()` existing innerHTML structure** — the section to replace is the value text + label text block at the end of the template string (lines 249–265):
```
      <!-- Value text (triple glow stack) -->
      <text ...>${displayText}</text>  ×3

      <!-- Label text -->
      <text ...>${label.toUpperCase()}</text>
```

---

### Task 1: Add math helpers to knob.js

**Files:**
- Modify: `Source/WebUI/knob.js` (after line 21, before `const TEMPLATE`)

- [ ] **Step 1: Insert the three helper functions**

Add the following block immediately after the `describeArc` function (after the closing brace on line 21), before `const TEMPLATE = ...`:

```javascript
// ── Waveform helpers (exact replica of DSP warpPhase / shapedWave) ──────────
function warpPhase(phase, duty) {
  const d = Math.min(0.95, Math.max(0.05, duty));
  if (phase < TAU * d)
    return phase / (2 * d);
  else
    return Math.PI + (phase - TAU * d) / (2 * (1 - d));
}

function shapedWave(wp, step) {
  const s = Math.sin(wp);
  if (step <= 0) return s;
  const drive = 1 + step * 19;
  const tanhD = Math.tanh(drive);
  return Math.tanh(s * drive) / tanhD;
}

function buildWaveformPoints(step, cx, cy, oledR) {
  const nPts = 64;
  const xL = cx - (oledR - 5);
  const xR = cx + (oledR - 5);
  const yMid = cy - oledR * 0.15;
  const yAmp = oledR * 0.45;
  const pts = [];
  for (let i = 0; i <= nPts; i++) {
    const t = (i / nPts) * TAU;
    const wp = warpPhase(t, 0.5);
    const y  = shapedWave(wp, step);
    pts.push(`${(xL + (i / nPts) * (xR - xL)).toFixed(1)},${(yMid - y * yAmp).toFixed(1)}`);
  }
  return pts.join(' ');
}
```

- [ ] **Step 2: Verify no syntax errors**

Open the WebUI in the JUCE plugin (or a local browser serving the Source/WebUI directory). Open DevTools → Console. There should be zero errors. All existing knobs should render normally.

- [ ] **Step 3: Commit**

```bash
git add "Source/WebUI/knob.js"
git commit -m "feat: add warpPhase/shapedWave/buildWaveformPoints helpers to knob.js"
```

---

### Task 2: Add waveform OLED branch in `_render()`

**Files:**
- Modify: `Source/WebUI/knob.js` — `_render()` method (lines 178–266)

- [ ] **Step 1: Compute `isWaveform` and build `oledContent`**

In `_render()`, immediately after the line `const displayText = this._displayValue || this._value.toFixed(2);` (line 189), insert:

```javascript
    const isWaveform = this.getAttribute('data-oled') === 'waveform';
    const numY = cy + oledR * 0.55;
    const oledContent = isWaveform
      ? `
      <!-- Waveform polyline -->
      <polyline points="${buildWaveformPoints(this._value, cx, cy, oledR)}"
        fill="none" stroke="#fff" stroke-opacity="0.85" stroke-width="1.5" stroke-linecap="round"/>
      <!-- Number (triple glow stack) -->
      <text x="${cx}" y="${numY}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="9"
        fill="#fff" opacity="0.3">${displayText}</text>
      <text x="${cx}" y="${numY}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="9"
        fill="#fff" opacity="0.6">${displayText}</text>
      <text x="${cx}" y="${numY}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="9"
        fill="#fff" opacity="1">${displayText}</text>`
      : `
      <!-- Value text (triple glow stack) -->
      <text x="${cx}" y="${cy - 1}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="${fontSize}"
        fill="#fff" opacity="0.3">${displayText}</text>
      <text x="${cx}" y="${cy - 1}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="${fontSize}"
        fill="#fff" opacity="0.6">${displayText}</text>
      <text x="${cx}" y="${cy - 1}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="${fontSize}"
        fill="#fff" opacity="1">${displayText}</text>

      <!-- Label text -->
      <text x="${cx}" y="${cy + fontSize / 2 + 5}" text-anchor="middle" dominant-baseline="central"
        font-family="'Space Grotesk', sans-serif" font-size="5" font-weight="500"
        letter-spacing="2" fill="rgba(255,255,255,0.45)" text-transform="uppercase"
        style="text-transform:uppercase">${label.toUpperCase()}</text>`;
```

- [ ] **Step 2: Replace the value text + label block in svg.innerHTML with `${oledContent}`**

Find the section inside the `svg.innerHTML = \`...\`` template string that currently reads:

```
      <!-- Value text (triple glow stack) -->
      <text x="${cx}" y="${cy - 1}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="${fontSize}"
        fill="#fff" opacity="0.3">${displayText}</text>
      <text x="${cx}" y="${cy - 1}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="${fontSize}"
        fill="#fff" opacity="0.6">${displayText}</text>
      <text x="${cx}" y="${cy - 1}" text-anchor="middle" dominant-baseline="central"
        font-family="'Courier New', monospace" font-weight="bold" font-size="${fontSize}"
        fill="#fff" opacity="1">${displayText}</text>

      <!-- Label text -->
      <text x="${cx}" y="${cy + fontSize / 2 + 5}" text-anchor="middle" dominant-baseline="central"
        font-family="'Space Grotesk', sans-serif" font-size="5" font-weight="500"
        letter-spacing="2" fill="rgba(255,255,255,0.45)" text-transform="uppercase"
        style="text-transform:uppercase">${label.toUpperCase()}</text>
```

Replace that entire block with:

```
      ${oledContent}
```

- [ ] **Step 3: Verify in browser — all existing knobs unchanged**

Open the plugin UI. All non-Shape knobs (Saturation, Strength, Push, Skip, etc.) must still show numeric value + inner label as before. DevTools Console must show zero errors.

- [ ] **Step 4: Commit**

```bash
git add "Source/WebUI/knob.js"
git commit -m "feat: add waveform OLED rendering branch to PhantomKnob"
```

---

### Task 3: Activate waveform on the Shape knob in index.html

**Files:**
- Modify: `Source/WebUI/index.html` (line 72 in current file)

- [ ] **Step 1: Update Shape knob element**

Find the current Shape knob element (line 72):

```html
              <phantom-knob data-param="synth_step" size="medium" label="Shape" default-value="0"></phantom-knob>
```

Replace with:

```html
              <phantom-knob data-param="synth_step" size="large" label="Shape" default-value="0" data-oled="waveform"></phantom-knob>
```

- [ ] **Step 2: Visual verification — Shape knob at value 0 (sine)**

Open the plugin UI. The Shape knob should:
- Be visibly larger than Strength / Push / Skip (those are medium, Shape is now large)
- Show a smooth sine wave in the OLED well
- Show "0" (or "0.00") as a small number in the lower zone of the OLED
- Show "SHAPE" label below the knob (from `.label-below` div)

- [ ] **Step 3: Visual verification — drag Shape knob to maximum**

Drag the Shape knob to the top (value 100). The OLED should:
- Show a visibly flat-topped / square-ish waveform
- Show "100" (or "1.00") in the number zone
- Update continuously and smoothly while dragging

- [ ] **Step 4: Visual verification — intermediate value**

Set Shape to approximately 50%. The waveform should be visibly between sine and square — a rounded, slightly clipped shape.

- [ ] **Step 5: Verify all other knobs unaffected**

Saturation (large), Strength, Push, Skip — all should display their numeric values normally with no visual change.

- [ ] **Step 6: Verify layout**

The Harmonic Engine panel should not overflow or clip. Two large knobs (Saturation + Shape) should sit comfortably in the row alongside three medium knobs.

- [ ] **Step 7: Commit**

```bash
git add "Source/WebUI/index.html"
git commit -m "feat: Shape knob upgraded to large with live waveform OLED display"
```
