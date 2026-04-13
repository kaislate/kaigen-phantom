# Frontend & Quick Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix 9 issues across the WebView frontend and one C++ host-API call — covering F1–F4 and S1–S5 from the spec.

**Architecture:** All frontend changes are pure JS/CSS/HTML in `Source/WebUI/`. The only C++ change is a one-liner in `PluginProcessor.h`. No new parameters except S2 (adds `%` label to two existing params and changes their stored range, requiring a PluginProcessor change).

**Tech Stack:** JUCE 7, WebBrowserComponent, vanilla JS (no modules), CSS custom properties

---

## Task 1 — F1: Dynamic Tail Length

**Files:**
- Modify: `Source/PluginProcessor.h:28`

The hardcoded `return 0.5` means hosts cut output at 500 ms regardless of the Release knob (which goes to 5000 ms). Fix reads the live Release parameter value.

- [ ] **Step 1: Open PluginProcessor.h and find the current declaration**

Read `Source/PluginProcessor.h` line 28. Confirm it says:
```cpp
double getTailLengthSeconds() const override { return 0.5; }
```

- [ ] **Step 2: Replace with dynamic value**

Replace that line with:
```cpp
double getTailLengthSeconds() const override
{
    const float releaseMs = apvts.getRawParameterValue(ParamID::ENV_RELEASE_MS)->load();
    return (double)(releaseMs / 1000.0f) + 0.1;
}
```

- [ ] **Step 3: Build and verify no errors**

Run:
```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" "C:\Documents\NEw project\Kaigen Phantom\build\Kaigen Phantom.sln" /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```
Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 4: Commit**

```bash
git add "Source/PluginProcessor.h"
git commit -m "fix: dynamic tail length from env_release_ms instead of hardcoded 0.5s"
```

---

## Task 2 — F3: Bypass Button CSS + F4: Remove Dead deconfliction_mode Code

**Files:**
- Modify: `Source/WebUI/styles.css`
- Modify: `Source/WebUI/phantom.js`

F3 and F4 are both tiny and touch different areas of the same two files — group them into one commit for efficiency.

### F3: Bypass visual state

The JS side is already implemented (`phantom.js` lines 22-27 add/remove `.active` class on `#bypass-btn`). Only the CSS rule for `.hdr-btn.active` is missing.

- [ ] **Step 1: Add the active-state rule to styles.css**

Open `Source/WebUI/styles.css`. Find the `.hdr-btn` rule. After it, add:
```css
.hdr-btn.active {
  color: rgba(255,255,255,0.90);
  border-color: rgba(255,178,38,0.60);
  box-shadow: 0 0 6px rgba(255,178,38,0.25);
}
```

### F4: Remove deconfliction_mode dead code

`phantom.js` section 6 (lines 213–227) calls `getComboBoxState("deconfliction_mode")`. This parameter does not exist in `Parameters.h`. Remove the entire section.

- [ ] **Step 2: Remove the dead section from phantom.js**

In `Source/WebUI/phantom.js`, delete from line 209 (`// =====...6. Wire deconfliction mode select`) through line 227 (the closing `}` of the `if (deconSelect)` block), inclusive. The section looks like:

```javascript
// =============================================================================
// 6. Wire deconfliction mode select
// =============================================================================

const deconModeState = getComboBoxState("deconfliction_mode");
const deconSelect = document.getElementById("decon-mode");

if (deconSelect) {
  deconSelect.addEventListener("change", () => {
    deconModeState.setChoiceIndex(deconSelect.selectedIndex);
  });

  function updateDeconUI() {
    deconSelect.selectedIndex = deconModeState.getChoiceIndex();
  }

  deconModeState.valueChangedEvent.addListener(updateDeconUI);
  updateDeconUI();
}
```

- [ ] **Step 3: Build and verify**

Run MSBuild (same command as Task 1 Step 3). Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 4: Manual verification**

Load plugin in DAW. Click ⏻ button. It should light up with amber border/glow. Click again — glow disappears. No console errors in WebView devtools.

- [ ] **Step 5: Commit**

```bash
git add "Source/WebUI/styles.css" "Source/WebUI/phantom.js"
git commit -m "fix: bypass button active CSS state; remove dead deconfliction_mode JS code"
```

---

## Task 3 — F2: Gate Threshold Lines on Oscilloscope

**Files:**
- Modify: `Source/WebUI/oscilloscope.js`

Draw two horizontal dashed amber lines on the oscilloscope canvas when Gate > 0 and mode = RESYN. Lines sit at ±gateThreshold amplitude from center, matching the waveform's scaleY.

- [ ] **Step 1: Read the current draw() function**

Open `Source/WebUI/oscilloscope.js` and find the `draw()` function (starts around line 90). Confirm:
- `const mid = h * 0.5` is used by `drawWave`
- `scaleY = h * 0.38` is inside `drawWave` but NOT declared in `draw()` scope

Note: `scaleY` must be declared locally in `draw()` for the gate lines — `drawWave` uses it internally but doesn't expose it.

- [ ] **Step 2: Add gate lines to draw()**

In `draw()`, find the block after the early return `if (!hasData) return;` and before the `const lin = {` line. Insert gate-line drawing code there. The final structure of `draw()` in the relevant area should look like:

```javascript
function draw() {
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const w = canvas.width;
    const h = canvas.height;
    if (w === 0 || h === 0) return;

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, w, h);

    // Zero line
    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(0, h * 0.5); ctx.lineTo(w, h * 0.5); ctx.stroke();

    // Vertical time-grid lines
    ctx.strokeStyle = 'rgba(255,255,255,0.03)';
    for (let i = 1; i < 8; i++) {
        const x = Math.round(w * i / 8) + 0.5;
        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
    }

    if (!hasData) return;

    // Gate threshold lines (RESYN mode only, Gate > 0)
    const gateState = window.Juce?.getSliderState?.('synth_gate_threshold');
    const modeState = window.Juce?.getSliderState?.('mode');
    const gateThr   = gateState ? gateState.getValue()  : 0;
    const isResyn   = modeState ? modeState.getValue() >= 0.5 : false;

    if (isResyn && gateThr > 0) {
        const mid    = h * 0.5;
        const scaleY = h * 0.38;
        const lineAbove = mid - gateThr * scaleY;
        const lineBelow = mid + gateThr * scaleY;
        ctx.save();
        ctx.strokeStyle = 'rgba(255,178,38,0.50)';
        ctx.lineWidth   = 1;
        ctx.setLineDash([4, 4]);
        ctx.beginPath(); ctx.moveTo(0, lineAbove); ctx.lineTo(w, lineAbove); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(0, lineBelow); ctx.lineTo(w, lineBelow); ctx.stroke();
        ctx.setLineDash([]);
        ctx.restore();
    }

    const lin = {
    // ... rest unchanged
```

- [ ] **Step 3: Build and verify**

Run MSBuild. Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 4: Manual verification**

Load plugin. Toggle to oscilloscope (click ≡ button). Set mode to RESYN. Turn Gate knob up. Two dashed amber horizontal lines should appear, symmetric around center, moving closer/farther as Gate is adjusted. Switch to Effect mode — lines should disappear. Set Gate back to 0 — lines disappear.

- [ ] **Step 5: Commit**

```bash
git add "Source/WebUI/oscilloscope.js"
git commit -m "feat: draw gate threshold lines on oscilloscope in RESYN mode (F2)"
```

---

## Task 4 — S1: Remove Voice-Split from UI

**Files:**
- Modify: `Source/WebUI/index.html`

Remove the `Voice-Split` option from the binaural mode select. The backend `BinauralStage` code is preserved for future implementation.

- [ ] **Step 1: Remove the option from index.html**

In `Source/WebUI/index.html`, find the binaural mode select (around line 152):
```html
<select id="binaural-mode-select" class="param-select" data-param="binaural_mode">
  <option value="0">Off</option>
  <option value="1">Spread</option>
  <option value="2">Voice-Split</option>
</select>
```

Remove the `<option value="2">Voice-Split</option>` line. Leave Off and Spread.

- [ ] **Step 2: Build and verify**

Run MSBuild. Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 3: Manual verification**

Open Settings overlay. Binaural mode dropdown should show only "Off" and "Spread". No Voice-Split option.

- [ ] **Step 4: Commit**

```bash
git add "Source/WebUI/index.html"
git commit -m "fix: remove Voice-Split from binaural UI — backend preserved for future (S1)"
```

---

## Task 5 — S2: Knob Value Formatting for wavelet_length and gate_threshold

**Files:**
- Modify: `Source/Parameters.h`
- Modify: `Source/PluginProcessor.cpp`

Most parameters already display correctly via `formatDisplayValue()` in `phantom.js` — they have proper labels (`"Hz"`, `"dB"`, `"ms"`, `"%"`) and matching ranges. The two exceptions:
- `synth_wavelet_length`: stored 0.05–1.0, no label → shows raw decimal
- `synth_gate_threshold`: stored 0.0–1.0, no label → shows "0%–100%" via fallback (works) but inconsistent

Fix: change both to 0–100 range with `"%"` label. Update `syncParamsToEngine` to divide by 100.

- [ ] **Step 1: Update synth_wavelet_length in Parameters.h**

Find the `synth_wavelet_length` parameter definition (around line 192):
```cpp
params.push_back(std::make_unique<APF>(
    ParamID::SYNTH_WAVELET_LENGTH, "Wavelet Length",
    NormalisableRange<float>(0.05f, 1.0f), 1.0f));
```

Replace with:
```cpp
params.push_back(std::make_unique<APF>(
    ParamID::SYNTH_WAVELET_LENGTH, "Wavelet Length",
    NormalisableRange<float>(5.0f, 100.0f), 100.0f,
    AudioParameterFloatAttributes().withLabel("%")));
```

- [ ] **Step 2: Update synth_gate_threshold in Parameters.h**

Find the `synth_gate_threshold` parameter definition (around line 195):
```cpp
params.push_back(std::make_unique<APF>(
    ParamID::SYNTH_GATE_THRESHOLD, "Gate Threshold",
    NormalisableRange<float>(0.0f, 1.0f), 0.0f));
```

Replace with:
```cpp
params.push_back(std::make_unique<APF>(
    ParamID::SYNTH_GATE_THRESHOLD, "Gate Threshold",
    NormalisableRange<float>(0.0f, 100.0f), 0.0f,
    AudioParameterFloatAttributes().withLabel("%")));
```

- [ ] **Step 3: Update syncParamsToEngine in PluginProcessor.cpp**

Find `engine.setWaveletLength` and `engine.setGateThreshold` calls (around lines 64-65):
```cpp
engine.setWaveletLength(apvts.getRawParameterValue(ParamID::SYNTH_WAVELET_LENGTH)->load());
engine.setGateThreshold(apvts.getRawParameterValue(ParamID::SYNTH_GATE_THRESHOLD)->load());
```

Replace with:
```cpp
engine.setWaveletLength(apvts.getRawParameterValue(ParamID::SYNTH_WAVELET_LENGTH)->load() / 100.0f);
engine.setGateThreshold(apvts.getRawParameterValue(ParamID::SYNTH_GATE_THRESHOLD)->load() / 100.0f);
```

- [ ] **Step 4: Build and verify**

Run MSBuild. Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 5: Manual verification**

Load plugin. Length knob should display "100%" at max, "50%" at mid, "5%" at min. Gate knob should display "0%" to "100%". Both knob tooltips should be semantic. Turning Length to 50% should halve the audible wavelet duration in RESYN mode.

- [ ] **Step 6: Commit**

```bash
git add "Source/Parameters.h" "Source/PluginProcessor.cpp"
git commit -m "fix: Length and Gate knobs display as % — change range to 0-100, divide by 100 in engine (S2)"
```

---

## Task 6 — S3: Crossover Frequency Line on Spectrum

**Files:**
- Modify: `Source/WebUI/spectrum.js`

Draw a vertical amber line at the `phantom_threshold` crossover frequency on the spectrum canvas. Updates each frame — same polling pattern as the spectrum itself.

- [ ] **Step 1: Read drawSpectrum() in spectrum.js**

Open `Source/WebUI/spectrum.js`. Find `drawSpectrum()` (starts at line 143). Note:
- `freqToX(freq, w)` already exists (line 49) — use it for the crossover x position
- The function clears and draws grid, then input fill, then output fill, then peak hold

- [ ] **Step 2: Add crossover line at the end of drawSpectrum()**

At the end of `drawSpectrum()`, after the peak-hold stroke (after `ctx.stroke()` on the peakPts, around line 200), add:

```javascript
    // ── Crossover frequency line ───────────────────────────────────────
    const xoverState = window.Juce?.getSliderState?.('phantom_threshold');
    if (xoverState) {
        const xoverHz = xoverState.getScaledValue();
        if (xoverHz > 20 && xoverHz < 20000) {
            const xPos = Math.round(freqToX(xoverHz, w));
            const labelFontPx = Math.max(8, Math.round(h * 0.10));
            ctx.save();
            ctx.strokeStyle = 'rgba(255,178,38,0.35)';
            ctx.lineWidth   = 1;
            ctx.setLineDash([3, 4]);
            ctx.beginPath();
            ctx.moveTo(xPos + 0.5, 0);
            ctx.lineTo(xPos + 0.5, h);
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.fillStyle  = 'rgba(255,178,38,0.55)';
            ctx.font       = labelFontPx + 'px monospace';
            ctx.textAlign  = 'left';
            ctx.textBaseline = 'top';
            const labelText = Math.round(xoverHz) + 'Hz';
            ctx.fillText(labelText, xPos + 3, 2);
            ctx.restore();
        }
    }
```

- [ ] **Step 3: Build and verify**

Run MSBuild. Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 4: Manual verification**

Load plugin. Spectrum should show a faint dashed amber vertical line. Turn the Crossover knob — line should move left/right in real-time tracking the frequency. Label text (e.g. "120Hz") should appear at the top of the line.

- [ ] **Step 5: Commit**

```bash
git add "Source/WebUI/spectrum.js"
git commit -m "feat: crossover frequency indicator line on spectrum display (S3)"
```

---

## Task 7 — S4: Recipe Wheel Snaps Preset to Custom on Spoke Drag

**Files:**
- Modify: `Source/WebUI/phantom.js`

When a spoke is dragged, `spoke-change` fires. The preset strip still shows the old preset name. Fix: set `recipe_preset` to index 6 (Custom) when `spoke-change` fires.

- [ ] **Step 1: Find the spoke-change handler in phantom.js**

Open `Source/WebUI/phantom.js`. Find the `spoke-change` listener (around line 123):
```javascript
document.addEventListener('spoke-change', e => {
  const { index, value } = e.detail;
  const id = harmonicParamIds[index];
  if (!id) return;
  const state = getSliderState(id);
  if (!state) return;
  state.sliderDragStarted();
  state.setNormalisedValue(value);
  state.sliderDragEnded();
});
```

- [ ] **Step 2: Add preset snap to the handler**

Replace the `spoke-change` listener with:
```javascript
document.addEventListener('spoke-change', e => {
  const { index, value } = e.detail;
  const id = harmonicParamIds[index];
  if (!id) return;
  const state = getSliderState(id);
  if (!state) return;
  // Snap to Custom preset (index 6) when user manually drags a spoke
  presetState.setChoiceIndex(6);
  state.sliderDragStarted();
  state.setNormalisedValue(value);
  state.sliderDragEnded();
});
```

Note: `presetState` is declared earlier in `phantom.js` (around line 165) and is in scope here because both are inside the same IIFE.

- [ ] **Step 3: Build and verify**

Run MSBuild. Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 4: Manual verification**

Load plugin. Click "Warm" preset — preset strip highlights "Warm". Now drag a spoke on the recipe wheel. The preset strip should immediately switch highlight to "Custom". Click "Stable" — returns to Stable. Drag spoke again — jumps to Custom.

- [ ] **Step 5: Commit**

```bash
git add "Source/WebUI/phantom.js"
git commit -m "feat: recipe wheel spoke drag snaps preset to Custom (S4)"
```

---

## Task 8 — S5: Dim Length and Gate Knobs in Effect Mode

**Files:**
- Modify: `Source/WebUI/phantom.js`
- Modify: `Source/WebUI/styles.css`

Length and Gate are RESYN-only controls. In Effect mode they do nothing but show no visual indication. Add CSS dimming + pointer-events:none when mode = Effect (index 0).

- [ ] **Step 1: Add CSS rule to styles.css**

In `Source/WebUI/styles.css`, add:
```css
phantom-knob.mode-inactive {
  opacity: 0.38;
  pointer-events: none;
  transition: opacity 0.15s ease;
}
```

- [ ] **Step 2: Update applyMode() in phantom.js to toggle dimming**

In `Source/WebUI/phantom.js`, find the `applyMode` function (around line 140):
```javascript
function applyMode(idx) {
  document
    .querySelectorAll(".mb[data-mode]")
    .forEach((b) =>
      b.classList.toggle("active", parseInt(b.dataset.mode) === idx)
    );
}
```

Replace with:
```javascript
function applyMode(idx) {
  document
    .querySelectorAll(".mb[data-mode]")
    .forEach((b) =>
      b.classList.toggle("active", parseInt(b.dataset.mode) === idx)
    );
  // Dim RESYN-only controls when in Effect mode (idx=0)
  const resynOnly = ['synth_wavelet_length', 'synth_gate_threshold'];
  const isEffect  = idx === 0;
  resynOnly.forEach(param => {
    const el = document.querySelector(`phantom-knob[data-param="${param}"]`);
    if (el) el.classList.toggle('mode-inactive', isEffect);
  });
}
```

- [ ] **Step 3: Build and verify**

Run MSBuild. Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 4: Manual verification**

Load plugin. Set mode to Effect — Length and Gate knobs should be visibly dimmed (opacity ~38%). Mouse over them — cursor shouldn't change (pointer-events:none). Click RESYN mode — knobs restore to full opacity and are interactive again. Mode change should be immediate with a brief fade transition.

- [ ] **Step 5: Commit**

```bash
git add "Source/WebUI/phantom.js" "Source/WebUI/styles.css"
git commit -m "feat: dim Length and Gate knobs in Effect mode — RESYN-only controls (S5)"
```
