# Preset Recipe Spectrum Preview — Design

**Date:** 2026-04-21
**Status:** Approved for implementation planning
**Scope:** Bedtime-sized feature. Single session.

---

## 1. Summary

Render a small monochrome spectrum curve inline at the top of the preset browser's preview card. The curve is a smooth Gaussian-peak-sum over the preset's seven harmonic weights (H2–H8) on a log-frequency axis, with a dashed vertical marker at the preset's own crossover frequency labeled in Hz. Shown whenever the user hovers (or has selected) a preset row in the browser.

The goal is to give the user a quick visual fingerprint of what a preset generates before loading it — closing the gap with commercial preset browsers like Arturia's and Serum's.

## 2. Motivation

The current preview card shows `name / type / designer / pack / description` only. Users must load a preset to hear or see its harmonic character. A small at-a-glance spectrum visualization turns browsing into a guided experience: odd-harmonic presets look spiky and bright, even-harmonic presets look denser and warmer, low-crossover presets sit further left than high-crossover ones.

The aesthetic — a single ink-dark curve on the neumorphic preview panel with a dashed crossover marker — was chosen to echo conventions already present in the main spectrum analyzer (which has the same crossover-frequency line) while fitting the preview card's light translucent surface.

## 3. Data Source

### 3.1 What lives in a preset

Every `.fxp` preset is an XML dump of the APVTS state tree. It already contains:

- `recipe_h2 … recipe_h8` — seven harmonic amplitude weights, normalized 0–1
- `phantom_threshold` — crossover frequency in Hz (parameter range 20–250 Hz; the UI knob is labeled *Crossover*)

No schema change to the preset file format is needed.

### 3.2 Extraction at scan time

`PresetManager::scanPresetsFromDisk()` currently reads each preset's `<Metadata>` child tree (name/type/designer/description) via `readMetadataFromFile()`. We extend this to also read the eight relevant parameter values (seven harmonics + crossover) from the APVTS state tree.

New struct on `PresetInfo`:

```cpp
struct PreviewData
{
    float h[7]       {};   // recipe_h2..h8
    float crossover  = 80.0f;
};

struct PresetInfo
{
    juce::File         file;
    PresetMetadata     metadata;
    PreviewData        preview;   // NEW
};
```

Values are read once during the scan and cached on `PresetInfo`, so hovering a row triggers zero disk I/O.

### 3.3 Serialization to JS

The `getAllPresets` native function (in `PluginEditor.cpp`) currently returns a JSON array of preset entries. Each entry gains a `preview` key:

```json
{
  "name": "Bright Lead",
  "type": "Synth",
  "designer": "Kaigen",
  "description": "...",
  "pack": "Factory",
  "isFavorite": false,
  "preview": {
    "h":        [0.45, 0.75, 0.30, 0.65, 0.25, 0.55, 0.20],
    "crossover": 80.0
  }
}
```

## 4. Rendering

### 4.1 New file

`Source/WebUI/preset-spectrum.js` — a single module exporting:

```js
window.PresetSpectrum = {
  render(svgEl, preview)  // populate svgEl with the spectrum viz
};
```

Written as an IIFE matching the pattern used by `preset-system.js` (no ES module imports — they fail in JUCE's WebView2 resource provider).

### 4.2 Curve math

For a given preset, the displayed curve is the sum of seven Gaussian peaks, one per harmonic:

```
S(f) = Σᵢ h[i] · exp( -((log₁₀ f − log₁₀ fᵢ) / σ)² )
```

Where:
- `fᵢ = (i + 2) · fundamentalHz` for i in 0..6  (i.e. 2×, 3×, …, 8× the implicit fundamental)
- `fundamentalHz = preview.crossover` — the crossover frequency is used as the visualization's implied fundamental. This is a display convention, not a DSP-accurate claim: the plugin's actual waveshaper output depends on whatever bass content the user is playing. Using the crossover keeps every preset's curve anchored to its own parameter set, so a preset with crossover=60Hz shows its harmonics left of one with crossover=200Hz.
- `σ = 0.04` — peak width in log-frequency space; gives visually distinct peaks without making the curve jagged
- `h[i]` is the recipe weight (already 0–1)

The curve is sampled at ~48 evenly log-spaced points between 30 Hz and `max(1000, 10 × crossover)` Hz. Each sample yields an (x, y) pair:
- `x = freqToX(f, width)` — same log mapping used by `spectrum.js` (`Math.log10(f/low) / Math.log10(high/low) × width`)
- `y = height × (1 − clamp01(S(f)))`

Path is drawn with quadratic-bezier smoothing between midpoints, matching `spectrum.js:strokeCurve`. The same path is used for the fill (baseline-closed) and the stroke.

### 4.3 SVG composition

Single `<svg viewBox="0 0 W 56" preserveAspectRatio="none">` containing, back to front:

1. **Three horizontal guide lines** at y = 14, 28, 42 (quartiles of the 56-px viewBox). Stroke `rgba(0,0,0,0.05)`, width 1.
2. **Dashed crossover marker** — vertical line at `freqToX(preview.crossover, W)` from y=2 to y=50. Stroke `rgba(0,0,0,0.30)`, width 0.6, `stroke-dasharray="2 2"`. Small label (e.g. `80Hz`) just to the right of the line at y=9, font-size 5.5, `rgba(0,0,0,0.40)`, monospace.
3. **Spectrum curve** — filled path with a linear gradient from `rgba(0,0,0,0.22)` at the top to `rgba(0,0,0,0.02)` at the baseline, stroked `rgba(0,0,0,0.72)` width 1.2.
4. **Three axis labels** at the bottom edge (y=52, font-size 6, `rgba(0,0,0,0.38)`, monospace). The labels are chosen from the fixed set `[30, 100, 300, 1k, 3k, 10k]` — pick the three whose positions divide the displayed range into roughly equal thirds. For the default 30–1000 Hz range, that's `30 / 300 / 1k`.

All colors are expressed in the same black-ink vocabulary as the preview card so the viz disappears into the neumorphic surface rather than fighting it.

### 4.4 Dimensions

- Width: 100% of preview panel (the existing `#browser-preview` is `flex: 1`, roughly 260–280px depending on layout)
- Height: 56-unit viewBox, rendered at 56 CSS pixels; 8px margin above and 10px margin below inside the preview card
- Total footprint above the preset metadata: ~74px

## 5. Integration Points

### 5.1 C++

**`Source/PresetManager.h`**
- Add `struct PreviewData` with `float h[7]` and `float crossover`
- Add `PreviewData preview;` field to `PresetInfo`

**`Source/PresetManager.cpp`**
- Extend scan logic: after parsing the state tree for metadata, also locate the parameter values for `recipe_h2..h8` and `phantom_threshold` in the APVTS portion of the tree and populate `PreviewData`. For legacy presets missing any value, fall back to 0 (harmonics) or 80 Hz (crossover).

**`Source/PluginEditor.cpp`** (`getAllPresets` lambda at line ~334)
- Serialize `info.preview` into each JSON entry as shown in §3.3

### 5.2 JavaScript

**`Source/WebUI/preset-spectrum.js`** — new file, implements `window.PresetSpectrum.render`

**`Source/WebUI/preset-system.js`** (`updatePreview` at line ~669)
- Prepend an `<svg class="preview-spectrum" viewBox="0 0 280 56" preserveAspectRatio="none" style="width:100%;height:56px;display:block;margin-bottom:10px;"></svg>` element to the preview's innerHTML, before the title
- After setting innerHTML, look up the svg via `preview.querySelector('.preview-spectrum')` and call `window.PresetSpectrum.render(svg, entry.preview)` if `entry.preview` is present and `window.PresetSpectrum` is defined

**`Source/WebUI/index.html`**
- Add `<script src="preset-spectrum.js"></script>` before `preset-system.js`

### 5.3 Build

**`CMakeLists.txt`**
- Add `preset-spectrum.js` to the `juce_add_binary_data` sources list so BinaryData contains it

**`Source/PluginEditor.cpp`** (resource provider)
- The URL `/preset-spectrum.js` needs to resolve. The existing resource lookup strips dashes per BinaryData naming conventions (`preset-spectrum.js` → `presetspectrum_js`), which should already be handled by the existing `.replace("-", "")` logic. Verify once during implementation.

## 6. Edge Cases

| Case | Behavior |
|------|----------|
| Preset missing `recipe_h2..h8` in state (legacy) | Read as 0. Curve renders as a flat baseline. Still visually valid. |
| Preset missing `phantom_threshold` | Fall back to 120 Hz — matches the APVTS live default in `Parameters.h`. Crossover marker draws at 120 Hz. |
| All harmonic weights are 0 | Flat baseline. Marker still shows. Communicates "no harmonics" accurately. |
| Very low crossover (20 Hz) | Fundamental is off-screen left; harmonics push left; axis still renders. No clipping issues. |
| Very high crossover (250 Hz) | Upper harmonics (8× = 2kHz) push past 1kHz label. Axis max scales to `max(1000, 10×crossover)` to keep all peaks visible. |
| Preview entry missing entirely (shouldn't happen) | Feature gracefully degrades — `render()` no-ops when `preview` is undefined, card shows as it does today |

## 7. Out of Scope

Explicitly deferred to future sessions:

- **Amplitude scaling by `phantom_strength`** — presets with low strength would render smaller; useful signal but adds complexity
- **Harmonic saturation influence** — saturation adds intermodulation; modeling it requires running actual DSP offline
- **Envelope follower influence** — time-domain behavior, not captured in a static preview
- **Binaural / stereo-width** — no stereo axis in the preview
- **Comparison overlay** — currently-loaded preset vs. hovered preset as two curves
- **Click-to-load from the preview** — rows already do this; the preview is informational
- **Ink-aesthetic redesign of the main spectrum panel** — user expressed interest; save for a dedicated session
- **Pre-rendered frequency-response PNG baked into preset files** — the "further out" option from the thoughts doc; much more work

## 8. Testing Strategy

Manual verification in Ableton Live 12:
1. Open the preset browser. Hover each built-in/Factory preset. Verify curve renders, crossover marker is at the expected Hz value.
2. Save a new preset with a distinctive recipe (e.g. only H3 at 1.0, rest 0). Reopen browser. Curve should be a single narrow peak at 3× the crossover.
3. Load a legacy preset from before this change (if any exist) — verify curve renders as flat baseline and marker at 80 Hz (no crash).
4. Adjust crossover in a preset, save, re-hover — verify marker tracks.
5. Resize the plugin window narrower and wider — verify the SVG rescales cleanly via `preserveAspectRatio="none"`.

No new automated tests. This is visual polish; existing preset-system integration tests are unaffected.

## 9. Future Ideas (not in this work)

- **Apply the same ink aesthetic to the main spectrum panel.** User raised this watching the mockups. Would unify visual language across preview and full analyzer. Dedicated session.
- **Tint curve by preset `type`.** A subtle color cast (e.g. amber for Bass, blue for Synth) would aid scanning the browser. Would break strict monochrome but useful if proven in testing.
- **Show currently-loaded preset's curve as a faint ghost behind the hovered one.** Direct before/after comparison while browsing. Requires maintaining two `PreviewData` references in JS state.
- **Scale amplitude by `phantom_strength`.** Single-line addition in the curve formula; adds signal if users find it helpful.

---

## Implementation Summary

| File | Change |
|------|--------|
| `Source/PresetManager.h` | Add `PreviewData` struct, field on `PresetInfo` |
| `Source/PresetManager.cpp` | Extract H2–H8 + crossover during scan |
| `Source/PluginEditor.cpp` | Include `preview` in `getAllPresets` JSON |
| `Source/WebUI/preset-spectrum.js` | NEW. Curve math + SVG rendering. |
| `Source/WebUI/preset-system.js` | Call `PresetSpectrum.render()` from `updatePreview` |
| `Source/WebUI/index.html` | Load `preset-spectrum.js` |
| `CMakeLists.txt` | Add file to BinaryData |

Estimated one focused session end-to-end.
