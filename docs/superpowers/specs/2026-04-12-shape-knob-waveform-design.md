# Shape Knob Waveform Display — Design Spec

## Goal

Replace the numeric-only OLED well in the Shape knob with a live sine→square waveform that morphs as the knob turns, with a small numeric value below the waveform.

---

## Scope

Frontend only. No DSP changes. Two files touched: `knob.js`, `index.html`.

---

## Knob Size

Shape knob is promoted from `size="medium"` (72×72px) to `size="large"` (100×100px, oledR=37). This gives 74px of OLED diameter to render a readable waveform. The Harmonic Engine panel flex ratio (1.2) accommodates a second large knob without layout changes.

---

## OLED Content

The OLED well is split vertically:

- **Upper 75%:** one-cycle waveform polyline (64 sample points, [0, 2π])
- **Lower 25%:** numeric value, 9px, centered, same white glow style as existing value text

Waveform color: `#fff` with `0.85` opacity — consistent with existing SVG text style.

Duty is fixed at 0.5 for the preview (Push affects harmonic balance, not the waveform's shape identity visually).

---

## Math (exact DSP replica in JS)

```js
function warpPhase(phase, duty) {
    const d = Math.min(0.95, Math.max(0.05, duty));
    if (phase < Math.PI * 2 * d)
        return phase / (2 * d);
    else
        return Math.PI + (phase - Math.PI * 2 * d) / (2 * (1 - d));
}

function shapedWave(wp, step) {
    const s = Math.sin(wp);
    if (step <= 0) return s;
    const drive = 1 + step * 19;
    const tanhD = Math.tanh(drive);
    return Math.tanh(s * drive) / tanhD;
}
```

`step` = normalised knob value in [0, 1] (parameter range 0–100, divide by 100).

---

## Trigger Mechanism

A new HTML attribute `data-oled="waveform"` on the `<phantom-knob>` element activates waveform mode. `_render()` in `knob.js` checks for this attribute and branches to waveform rendering instead of the standard value text.

`_render()` is already called on every value change, so reactivity is free — no extra event listeners needed.

---

## Polyline Generation

```js
// Inside _render(), waveform branch:
const step = this._norm;          // 0..1
const nPts = 64;
const xL = cx - (oledR - 5);
const xR = cx + (oledR - 5);
const yMid = cy - oledR * 0.12;  // shift center up to leave room for number
const yAmp = oledR * 0.52;

const pts = [];
for (let i = 0; i <= nPts; i++) {
    const t = (i / nPts) * Math.PI * 2;
    const wp = warpPhase(t, 0.5);
    const y  = shapedWave(wp, step);
    const px = xL + (i / nPts) * (xR - xL);
    const py = yMid - y * yAmp;
    pts.push(`${px.toFixed(1)},${py.toFixed(1)}`);
}
// SVG: <polyline points="..." stroke="#fff" stroke-opacity="0.85" stroke-width="1.5" fill="none"/>
```

Number below:

```js
const numY = cy + oledR * 0.62;
// SVG: triple-stacked <text> at (cx, numY), font-size 9, same glow structure as existing value text
// Display: Math.round(this._norm * 100) + "%"  — or just the raw integer
```

---

## HTML Change

```html
<!-- Before -->
<phantom-knob data-param="synth_step" size="medium" label="Shape" default-value="0"></phantom-knob>

<!-- After -->
<phantom-knob data-param="synth_step" size="large" label="Shape" default-value="0" data-oled="waveform"></phantom-knob>
```

---

## Testing

1. Shape knob renders at large size in the Harmonic Engine row
2. At value 0: smooth sine wave visible in OLED
3. At value 100: flat-topped square wave visible
4. At value 50: visibly between sine and square
5. Number reads correct percentage at all values
6. All other knobs unaffected (waveform mode only activates on `data-oled="waveform"`)
7. No layout overflow in Harmonic Engine panel

---

## Out of Scope

- Animating the Push knob — Push uses standard numeric OLED
- Any DSP changes
