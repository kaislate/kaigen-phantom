# Kaigen Phantom — Fixes & Improvements Spec

**Date:** 2026-04-13  
**Scope:** Fix-Now (4), Short-Term (5), Medium-Term (4)  
**Exploratory items (MIDI lock, Waveshaper removal, program management, Gate positive-peak symmetry) deferred.**

---

## Tier 1 — Fix Now

### F1. Dynamic Tail Length

**Problem:** `getTailLengthSeconds()` returns a hardcoded `0.5`. The envelope follower release can be up to 5000 ms. Hosts that respect tail length will cut the plugin's output after 500 ms regardless of the release setting, producing audible cutoffs on slow-release notes.

**Fix:** Return the current `env_release_ms` value converted to seconds, with a small safety margin:
```cpp
double getTailLengthSeconds() const override
{
    const float releaseMs = apvts.getRawParameterValue(ParamID::ENV_RELEASE_MS)->load();
    return (double)(releaseMs / 1000.0f) + 0.1;  // 100 ms safety margin
}
```

**Files:** `Source/PluginProcessor.cpp`

---

### F2. Gate Threshold Lines on Oscilloscope

**Problem:** The Gate knob has no visual correlation to the signal. Miya draws horizontal threshold lines on the waveform display so you can see exactly which crossings will be blocked. Without this, Gate is a blind control.

**Design:**
- When Gate > 0 and oscilloscope is visible, draw two horizontal lines on the IN layer at `±gateThreshold × scaleY` from the centre line
- Lines: `rgba(255, 178, 38, 0.5)` (amber — same hue as the SYNTH layer, half opacity, dashed)
- When Gate = 0 (default), lines are not drawn — no visual change for users who never touch Gate
- Lines update in real-time as the knob is turned (polled each draw frame from the relay)
- When in Effect mode, Gate is inactive — lines should NOT draw even if Gate > 0 (they would be misleading)

**Implementation:** In `oscilloscope.js` `draw()`, after drawing the zero line and before drawing waveforms:
```javascript
const gateState   = window.Juce?.getSliderState?.('synth_gate_threshold');
const modeState   = window.Juce?.getSliderState?.('mode');
const gateThr     = gateState  ? gateState.getValue()  : 0;
const isResyn     = modeState  ? modeState.getValue() >= 0.5 : false;

if (isResyn && gateThr > 0 && hasData) {
    const lineAbove = mid - gateThr * scaleY;
    const lineBelow = mid + gateThr * scaleY;
    ctx.save();
    ctx.strokeStyle = 'rgba(255,178,38,0.50)';
    ctx.lineWidth   = 1;
    ctx.setLineDash([4, 4]);
    ctx.beginPath(); ctx.moveTo(0, lineAbove); ctx.lineTo(w, lineAbove); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(0, lineBelow); ctx.lineTo(w, lineBelow); ctx.stroke();
    ctx.restore();
}
```

`scaleY = h * 0.38` — matches the existing `drawWave` scaleY so the lines sit correctly against the drawn waveform.

**Files:** `Source/WebUI/oscilloscope.js`

---

### F3. Bypass Button Visual State

**Problem:** The bypass button (⏻) in the header sends the bypass parameter but has no visual feedback. You cannot tell whether the plugin is bypassed without listening.

**Design:**
- When `bypass` parameter is true, add class `active` to `#bypass-btn`
- CSS `.hdr-btn.active`: bright white text, amber border or slight amber glow to match the rest of the active-state language in the UI

**CSS addition** (in `styles.css`):
```css
.hdr-btn.active {
  color: rgba(255,255,255,0.90);
  border-color: rgba(255,178,38,0.60);
  box-shadow: 0 0 6px rgba(255,178,38,0.25);
}
```

**JS change** (in `phantom.js`, in the bypass toggle handler): on any bypass state change, toggle `.active` class on the button to reflect current state.

**Files:** `Source/WebUI/phantom.js`, `Source/WebUI/styles.css`

---

### F4. Remove `deconfliction_mode` JS Reference

**Problem:** `phantom.js` calls `getComboBoxState("deconfliction_mode")`. This parameter does not exist in `Parameters.h`. The code silently does nothing, but it will log warnings or cause confusion in future debugging.

**Fix:** Remove the `getComboBoxState("deconfliction_mode")` call and any associated handler setup from `phantom.js`.

**Files:** `Source/WebUI/phantom.js`

---

## Tier 2 — Short Term

### S1. Remove Voice-Split from UI (Preserve Backend)

**Problem:** Binaural mode "Voice-Split" is selectable in Settings but the `BinauralStage::process()` branch is a no-op (`/* deferred */`). A visible control that does nothing erodes trust.

**Decision:** Remove `Voice-Split` from the `<select>` dropdown in the Settings overlay. The `BinauralStage` code stays — the backend is retained for future implementation.

**Approach:**
- Remove `<option value="2">Voice-Split</option>` from `index.html`
- The `binaural_mode` parameter range remains 0–2 in `Parameters.h` for preset compatibility (old sessions with `binaural_mode=2` will silently use no effect, same as now)
- When Voice-Split is implemented, the option is simply re-added

**Files:** `Source/WebUI/index.html`

---

### S2. Knob Value Formatting

**Problem:** Several knobs display raw normalized values rather than semantic units, making them hard to read. Miya and every other well-designed plugin display units the user understands.

**Affected parameters and their target displays:**

| Parameter | Raw range | Display format | Example |
|---|---|---|---|
| `synth_wavelet_length` | 0.05 – 1.0 | `X%` (×100, round) | `75%` |
| `synth_gate_threshold` | 0.0 – 1.0 | `X%` (×100, round) | `40%` |
| `synth_duty` | 0.05 – 0.95 | `X%` (×100, round) | `50%` |
| `output_gain` | 0.0 – 1.0 (maps to -24..+12 dB) | `±XdB` | `+3dB` / `-6dB` |
| `phantom_threshold` | 20 – 250 Hz (stored raw Hz) | `XXHz` | `80Hz` |
| `synth_skip` | 1 – 8 (stored as 1–8 integer) | integer `1`–`8` | `3` |
| `env_attack_ms` | 0.1 – 2000 ms | `Xms` (1 decimal if < 10) | `1.0ms` / `200ms` |
| `env_release_ms` | 5 – 5000 ms | `Xms` | `50ms` |

**Output gain conversion:** The `output_gain` parameter uses `NormalisableRange<float>(-24.0f, 12.0f)` (or similar — verify the actual range in `Parameters.h`). Map with `Decibels::gainToDecibels` or directly from the stored dB value.

**Implementation:** In `phantom.js`, each `WebSliderRelay` has a value-changed callback that sets `_displayValue` on the corresponding `phantom-knob` element. Add formatting functions per parameter and apply them in those callbacks. The `phantom-knob` component uses `_displayValue` when set, falling back to the raw value otherwise.

```javascript
// Formatting helpers
function fmtPercent(v)  { return Math.round(v * 100) + '%'; }
function fmtHz(v)       { return Math.round(v) + 'Hz'; }
function fmtMs(v)       { return v < 10 ? v.toFixed(1) + 'ms' : Math.round(v) + 'ms'; }
function fmtDb(v)       { const db = /* convert from stored range */; 
                           return (db >= 0 ? '+' : '') + db.toFixed(1) + 'dB'; }
function fmtSkip(v)     { return String(Math.round(v)); }
```

**Files:** `Source/WebUI/phantom.js`

---

### S3. Crossover Frequency Line on Spectrum

**Problem:** `phantom_threshold` determines which frequencies get processed but nothing in the spectrum display marks where that boundary is. It's impossible to judge the control visually.

**Design:**
- Draw a vertical line on the spectrum canvas at the `phantom_threshold` frequency
- Style: `rgba(255,178,38,0.35)` (faint amber) — consistent with SYNTH layer color, clearly distinct from the gray/white spectrum layers
- Label: small amber text `"XOVER"` or just the frequency value (`"80Hz"`) just above the line at the top of the canvas
- Updates in real-time as `phantom_threshold` is changed (polled each draw frame, same pattern as oscilloscope gate lines)
- The x-position maps from Hz to canvas x using the same log-scale formula already used to position spectrum bins

**x-position calculation:**
```javascript
const logMin  = Math.log10(30);
const logMax  = Math.log10(16000);
const xoverHz = /* read from relay */;
if (xoverHz > 30 && xoverHz < 16000) {
    const xFrac = (Math.log10(xoverHz) - logMin) / (logMax - logMin);
    const xPos  = Math.round(xFrac * w);
    // draw line at xPos
}
```

**Files:** `Source/WebUI/spectrum.js`

---

### S4. Recipe Wheel Custom Preset Snap

**Problem:** Dragging a spoke on the recipe wheel updates H2-H8 amplitudes but does not update the preset selector. The preset strip still shows the old preset name (e.g., "Stable") even though the values no longer match it. Users have no visual confirmation that they are in a custom state.

**Fix:**
- When a spoke-drag interaction is initiated (mouse/touch down on a spoke), immediately set `recipe_preset` to 6 (Custom) before the H2-H8 values change
- The preset strip will then highlight "Custom" as the spoke is dragged
- Preset changes triggered by clicking the preset strip do NOT trigger this (they write the correct preset index themselves)

**Files:** `Source/WebUI/recipe-wheel.js` (or wherever spoke interaction events originate — likely `phantom.js` or `recipe-wheel.js`)

---

### S5. RESYN-Only Controls Dimming

**Problem:** Length and Gate knobs are in the Harmonic Engine row alongside always-active controls (Saturation, Strength, Shape, Push, Skip). In Effect mode, Length and Gate do nothing but show no indication of this.

**Design:**
- When mode = Effect (0), apply a `.mode-inactive` CSS class to the Length and Gate knob elements
- When mode = RESYN (1), remove the class
- `.mode-inactive` style: `opacity: 0.38` and `pointer-events: none` (not interactive when inactive)
- The class is toggled in real-time via the mode relay's value-changed callback in `phantom.js`
- The label below the knob also dims (it's inside the web component's shadow DOM; the outer element opacity covers it)

**CSS:**
```css
phantom-knob.mode-inactive {
  opacity: 0.38;
  pointer-events: none;
}
```

**Files:** `Source/WebUI/phantom.js`, `Source/WebUI/styles.css`

---

## Tier 3 — Medium Term

### M1. Per-Harmonic Anti-Aliasing

**Problem:** H8 aliases when the fundamental exceeds ~2.75 kHz (Nyquist/8 at 44.1 kHz). The crossover limits processed bass to 500 Hz by default, but `phantom_threshold` is user-adjustable up to 250 Hz and H8 at 250 Hz is 2000 Hz — still well below Nyquist. However at non-standard sample rates (48 kHz, 96 kHz) and with aggressive settings this becomes audible.

**Fix:** In the harmonic sum loop of both `WaveletSynth::process()` and `ZeroCrossingSynth::process()`, multiply each harmonic's contribution by a Nyquist fade factor:

```cpp
// Fade harmonics starting 1 octave below Nyquist (e.g. begins at 0.5× Nyquist)
static const float nyquist      = (float)(sampleRate * 0.5);
static const float fadeStartHz  = nyquist * 0.5f;   // fade starts at Nyquist/2
const float harmonicHz = (float)(i + 2) / estimatedPeriod * (float)sampleRate;
const float aaMul      = juce::jlimit(0.0f, 1.0f,
                           (nyquist - harmonicHz) / (nyquist - fadeStartHz));
y += amps[(size_t)i] * aaMul * shapedWave(warpPhase(hp, duty), step);
```

This is inaudible under normal use (fades start at ~11 kHz with 44.1 kHz SR) and only engages as harmonics approach Nyquist.

**Files:** `Source/Engines/WaveletSynth.cpp`, `Source/Engines/ZeroCrossingSynth.cpp`

---

### M2. Harmonic Sum Soft-Clip in RESYN Mode

**Problem:** WaveletSynth outputs H1 at amplitude 1.0 unconditionally, then adds H2-H8 at user recipe levels. With Dense preset (`[0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7]`), the theoretical peak is ±5.9 before envelope scaling. This passes through the LPF/HPF into `phantomOut * inLvl * phantomStrength` — at high Strength settings this clips the output.

**Fix:** After the harmonic sum and before the length gate in `WaveletSynth::process()`, apply tanh soft saturation:

```cpp
// Soft-clip the harmonic sum to prevent hard clipping on dense recipes.
// tanh(y / 1.5) / tanh(1/1.5) normalises peak-1 signals to ≈1 while
// gently compressing larger amplitudes. Transparent at low recipe levels.
static const float kClipRef  = 1.5f;
static const float kClipNorm = std::tanh(1.0f / kClipRef);
y = std::tanh(y / kClipRef) / kClipNorm * kClipRef;
```

This is a gentle soft-knee: a sum of 1.0 passes through unchanged, 3.0 is compressed to ~2.5, 6.0 to ~3.0. The dense recipe becomes usable at high settings rather than distorting.

**Note:** ZCS never outputs H1 (fundamental absent by design), so its maximum sum is `Σ amps[i]` over H2-H8 with max 7.0. Apply the same fix to ZCS for consistency.

**Files:** `Source/Engines/WaveletSynth.cpp`, `Source/Engines/ZeroCrossingSynth.cpp`

---

### M3. Sidechain Envelope Input

**Problem:** The plugin declares a stereo sidechain input bus that is never read. The most natural use case: drive the phantom harmonic amplitude from an external signal (kick drum, sequencer pulse) for rhythmic bass enhancement — a staple technique.

**Design:**

**Mode:** A new dropdown in the Settings overlay: `"Envelope Source: Input / Sidechain"`. Default: Input (current behaviour unchanged). When Sidechain is selected, the envelope follower reads from the sidechain bus instead of the main input.

**Parameter:** `env_source` — int, 0=Input, 1=Sidechain.

**Signal flow change in `PhantomEngine::process(buffer)`:**
```cpp
// For envelope calculation, use sidechain if enabled and present
const float* envSrc = (envSource == 1 && sidechainBuffer != nullptr)
    ? sidechainBuffer->getReadPointer(0)
    : low;   // default: bass band from main input
const float inLvl = env.process(envSrc[i]);
```

**PluginProcessor change:** In `processBlock()`, read the sidechain bus (index 1) if present and pass it to `PhantomEngine` before calling `engine.process(buffer)`.

**UI:** Add `"Envelope Source"` row to the Settings panel below BINAURAL, with a two-option select (`Input` / `Sidechain`). Only the label and select — no extra knobs.

**New parameter:** `env_source` in `Parameters.h` and `createParameterLayout()`.

**Files:** `Source/Parameters.h`, `Source/PluginProcessor.cpp`, `Source/Engines/PhantomEngine.h/.cpp`, `Source/WebUI/index.html`

---

### M4. Skip Count Glitch Fix

**Problem:** Changing `synth_skip` mid-note resets `crossingsAccum` and `accumulatedSamples` to zero immediately. This drops the period accumulator and causes a measurement gap (silence from the accumulator perspective) until the next `skipCount` crossings complete — audible as a momentary pitch flutter on live automation.

**Fix:** On skip change, preserve `estimatedPeriod` and scale `accumulatedSamples` to the new skip count so the accumulator is in a reasonable starting position:

```cpp
void WaveletSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(1, 8, n);
    if (newSkip != skipCount)
    {
        // Scale accumulated samples proportionally so the EMA has a warm start.
        // e.g. going from skip=1 to skip=2: double the accumulation so the
        // first new measurement lands near the current estimate.
        if (accumulatedSamples > 0.0f)
            accumulatedSamples *= (float)newSkip / (float)skipCount;
        skipCount      = newSkip;
        crossingsAccum = 0;
        // estimatedPeriod is intentionally NOT reset
    }
}
```

Apply the same fix to `ZeroCrossingSynth::setSkipCount()`.

**Files:** `Source/Engines/WaveletSynth.cpp`, `Source/Engines/ZeroCrossingSynth.cpp`

---

## Out of Scope (Deferred to Exploratory)

- MIDI note-lock mode
- Waveshaper.h/.cpp removal
- Program/preset management system
- Gate positive-peak symmetry
- Voice-Split binaural implementation
- Per-harmonic phase offsets

---

## File Change Summary

| File | Tiers that touch it |
|---|---|
| `Source/PluginProcessor.cpp` | F1, M3 |
| `Source/Parameters.h` | M3 |
| `Source/Engines/PhantomEngine.h/.cpp` | M3 |
| `Source/Engines/WaveletSynth.cpp` | M1, M2, M4 |
| `Source/Engines/ZeroCrossingSynth.cpp` | M1, M2, M4 |
| `Source/WebUI/phantom.js` | F3, F4, S2, S4, S5 |
| `Source/WebUI/oscilloscope.js` | F2 |
| `Source/WebUI/spectrum.js` | S3 |
| `Source/WebUI/styles.css` | F3, S5 |
| `Source/WebUI/index.html` | S1, M3 |
| `Source/WebUI/recipe-wheel.js` | S4 |
