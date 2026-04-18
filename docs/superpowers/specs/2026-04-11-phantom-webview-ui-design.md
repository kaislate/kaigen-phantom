# Kaigen Phantom — WebView UI Design Spec

**Date:** 2026-04-11
**Status:** Approved for implementation
**Replaces:** `2026-04-11-phantom-ui-design.md` (JUCE 2D approach — abandoned)
**Mockup reference:** `.superpowers/brainstorm/37941-1775788189/content/mockup-v21.html`

## Overview

The Phantom plugin UI is rendered entirely in an embedded Chromium WebView (JUCE 8's `WebBrowserComponent` with WebView2 backend on Windows). The HTML/CSS/JS from the mockup becomes the production UI — Three.js holographic wheel with bloom, CSS neumorphic panels, Canvas2D spectrum analyzer. All 35 APVTS parameters bind via JUCE's built-in relay system. Real-time data (spectrum, pitch, levels) flows through custom native functions.

## Window

- **Size:** 920 x 620 pixels (fixed, non-resizable)
- **No footer bar** — bypass and settings live in the header

## Layout

```
┌──────────────────────────────────────────────────┐
│ [logo] PHANTOM    [Effect|Instr] [⏻] [⚙] KAIGEN │  46px header
├─────────────────────┬────────────────────────────┤
│                     │ Harmonic Engine    │ Output │
│                     │ Sat(lg) Str(md)    │ Gain   │  ~120px
│   RECIPE WHEEL      ├───────────────────┬────────┤
│   ~340px diameter   │ Ghost             │Stereo  │
│   Three.js +        │ Amt(lg) Thr(md)   │Wid(lg) │  ~120px
│   energy flow       │ [Rpl|Add]         │        │
│   particles         ├───────────────┬───┴────────┤
│                     │ Pitch Tracker │ Sidechain   │
│                     │ Sens  Glide   │ Amt Atk Rel │  ~110px
├─────────────────────┼───────────────┴────────────┤
│ [Warm][Aggr][Hol]   │ ┃                        ┃ │
│ [Dense][Custom]     │ ┃  SPECTRUM ANALYZER      ┃ │  fills rest
│                     │ ┃  (bar + line modes)     ┃ │  (~220px)
│ fund: A2 · 110Hz   │IN┃                        ┃OUT
└─────────────────────┴────────────────────────────┘
```

## Architecture

### C++ Side (Thin Shell)

```
PluginEditor
├── WebBrowserComponent (fills 920x620)
│   ├── ResourceProvider (serves WebUI/ files from BinaryData)
│   ├── 35 parameter relays (auto-sync HTML ↔ APVTS)
│   └── 3 native functions (spectrum, peaks, pitch)
│
PluginProcessor (existing DSP + real-time data)
├── std::atomic<float> currentPitch
├── std::atomic<float> peakInL, peakInR, peakOutL, peakOutR
├── std::array<float, 80> spectrumData
├── std::atomic<bool> spectrumReady
└── FFT computation in processBlock()
```

### WebView Side (Full UI)

```
index.html
├── styles.css         (neumorphic panels, knobs, glow, layout)
├── phantom.js         (JUCE bridge, parameter binding, mode switching, data polling)
├── recipe-wheel.js    (Three.js holographic wheel + energy flow)
├── spectrum.js        (Canvas2D analyzer, bar + line modes)
├── knob.js            (custom rotary knob web component)
└── three.module.js    (bundled Three.js, no CDN)
```

## Header (46px)

- Stippled dot texture background (CSS `radial-gradient` repeating pattern)
- Bottom edge glow seam (CSS `radial-gradient`)
- **Logo:** PNG image, 34px tall, with CSS `filter: drop-shadow` glow
- **PHANTOM:** Etched metallic text (CSS `background-clip: text` with metallic gradient, `filter: drop-shadow` for etch effect)
- **Mode toggle:** Effect / Instrument, two radio-style buttons. Bound to `mode` relay. Clicking toggles visibility of Pitch Tracker vs Deconfliction panel via JS DOM manipulation.
- **Bypass toggle:** Power icon button. Bound to a bypass parameter (or handled via JUCE's built-in bypass). Glows white when plugin is active, dim when bypassed.
- **Settings gear:** Opens/closes the settings overlay panel.
- **KAIGEN:** Etched metallic text, right-aligned.

## Recipe Wheel (Top-Left, ~340x360px)

The visual centerpiece. Rendered with Three.js + UnrealBloomPass inside a `<canvas>` element, with CSS-styled overlays for text and labels.

### Three.js Scene (v7 ethereal style merged with v21 features)

**Holographic Rings:**
- 6 concentric rings at radii 28-126px
- Thin strokes (0.3-0.8px), low opacity (0.035-0.13)
- Rotate at different speeds: {0.015, -0.02, 0.01, -0.025, 0.012, -0.008}
- White with subtle iridescent color shifts per ring (ddeeff, ffeedd, eeffee, eeddff)

**Harmonic Tank Pathways (7 spokes, H2-H8):**
- Dim track line from center to perimeter (opacity 0.05, width 5px)
- Bright fill line proportional to harmonic amplitude (gradient 55%→8% opacity, width 3.5px)
- Glowing cap dot at fill endpoint (opacity proportional to amplitude)

**Harmonic Nodes (v7 dual-element):**
- Large soft glow halo at each spoke endpoint (size 6+amp*12, opacity amp*0.14)
- Small bright core dot (opacity 0.4+amp*0.45)

**Energy Flow Particles (new):**
- Particles spawn at the center and flow outward along each spoke
- Speed and brightness proportional to that harmonic's amplitude
- High-amplitude harmonics have fast, bright streams; low-amplitude have sparse, dim trickles
- Implemented as a `THREE.Points` system with per-particle velocity along spoke direction
- ~20 particles per spoke (140 total), recycled when they reach the perimeter

**Scan Line:**
- Single line from center to radius 122px, rotating at 0.18 rad/frame
- Very low opacity (0.05)

**Center Glow:**
- Soft radial glow at center (22px radius, 0.07 opacity)
- Bright core (12px, 0.14 opacity)

**Bloom:**
- UnrealBloomPass: strength 3.5, radius 0.85, threshold 0.006

### CSS Overlay (on top of Three.js canvas)

**Neumorphic Mount:**
- 340px circular border with CSS neumorphic shadows
- Top-left highlight: `-6px -6px 16px rgba(255,255,255,0.025)`
- Bottom-right shadow: `6px 6px 20px rgba(0,0,0,0.7)`
- Radial gradient fill matching mockup v21

**Center OLED Display (96px diameter):**
- Pure black circle with triple-ring lip/ridge (same as knobs)
- Line 1: "FUND" — 6px, dim
- Line 2: Note name + Hz (e.g., "A2 · 110Hz") — 14px bold, bright white multi-layer glow
- Line 3: Current preset name — 7px, white
- Updated at 30fps from `getPitchInfo()` native function

**H2-H8 Labels:**
- Positioned at perimeter at spoke angles
- 8px bold monospace, `rgba(255,255,255,0.45)` with subtle glow shadow

### Below the Wheel

**Preset Strip:**
- 5 clickable labels: Warm, Aggr, Hollow, Dense, Custom
- Active label: pure white with multi-layer text-shadow glow + dot underneath
- Inactive: very dim (0.07 opacity)
- Clicking sets `recipe_preset` relay value

**Fundamental Readout:**
- Small OLED-style display showing detected fundamental
- Updates from `getPitchInfo()` native function

## Right Panels (Neumorphic Knob Controls)

### Visual Style (CSS)

All panels use the mockup v21 neumorphic style:

**Panel background:**
- `background: rgba(8,8,16,0.92)`
- `border-radius: 8px`
- Inset shadows: `inset 4px 4px 16px rgba(0,0,0,0.6)`, `inset -2px -2px 6px rgba(255,255,255,0.01)`
- Outer neumorphic: `-5px -5px 14px rgba(255,255,255,0.022)`, `5px 5px 18px rgba(0,0,0,0.75)`
- Top-left light spill via `::before` pseudo-element

**Knob rendering (custom `<phantom-knob>` web component):**

Two sizes — Large (100px) and Medium (72px). Each knob is an interactive SVG/Canvas element:

1. Volcano surface: radial gradient with directional light (bright at 32°/28° from top-left)
2. OLED well: black circle inset from edge (13px for Large, 9px for Medium)
3. Lip/ridge: three concentric SVG circles (bright → dark gap → outer subtle)
4. Arc track: full-sweep dim arc (rgba 255,255,255,0.07, 3.5px stroke)
5. Arc value: proportional bright white arc (2.8px stroke) + glow halo (6px, 30% opacity)
6. Value text: 3-pass glow rendering (wide dim → medium → sharp bright)
7. Label text: small caps below value

**Knob interaction:**
- Click+drag vertically to change value
- Double-click to reset to default
- Value changes call the corresponding JUCE relay's `setNormalisedValue()`
- Relay change events update the knob display

### Panel Layout (Row 1, ~120px tall)

**Harmonic Engine (flex: 1.1):**
- Saturation knob (Large, default 0%) — bound to `harmonic_saturation` relay
- Strength knob (Medium, default 80%) — bound to `phantom_strength` relay

**Output (flex: 0.55):**
- Gain knob (Large, default 0dB) — bound to `output_gain` relay

### Panel Layout (Row 2, ~120px tall)

**Ghost (flex: 0.9):**
- Amount knob (Large, default 100%) — bound to `ghost` relay
- Threshold knob (Medium, default 80Hz) — bound to `phantom_threshold` relay
- Replace/Add toggle — bound to `ghost_mode` relay

**Stereo (flex: 0.6):**
- Width knob (Large, default 100%) — bound to `stereo_width` relay

### Panel Layout (Row 3, ~110px tall, mode-switched)

**Effect mode visible:**
- Pitch Tracker: Sensitivity (Medium, 70%) + Glide (Medium, 20ms) — `tracking_sensitivity`, `tracking_glide`
- Sidechain Duck: Amount (Medium, 0%) + Attack (Medium, 5ms) + Release (Medium, 80ms) — `sidechain_duck_amount/attack/release`

**Instrument mode visible:**
- Deconfliction: Mode selector (6 options) + MaxVoices (Medium, 4) + StaggerDelay (Medium, 8ms) — `deconfliction_mode`, `max_voices`, `stagger_delay`
- Sidechain Duck: same as Effect mode

Mode switching is pure JS — `display:none` toggle on the two panels when the `mode` relay value changes.

### Glow Seams Between Panels

- 3px dividers between major sections
- CSS: 1px center line with `linear-gradient` (transparent → 16% white → transparent)
- Behind it: 8px blurred gradient at 4% white for bloom effect

## Spectrum Analyzer (Bottom-Right)

**Canvas2D element, ~220px tall, fills remaining width.**

### Data

- 80 frequency bins, log-spaced 30Hz–16kHz
- Updated at ~30fps by polling `getSpectrumData()` native function
- Smoothed with factor 0.3 per frame

### Two Render Modes (toggled by small button in corner)

**Bar mode:**
- 80 vertical bars with 1px gaps
- Fill: vertical gradient (50% white at bottom → 4% at top)
- Peak cap: 1px bright line with glow
- Fundamental band drawn in gray (15% opacity) behind white harmonic bars

**Line mode:**
- Smooth path connecting bin tops
- Fill below: gradient from 20% white at bottom to transparent
- Stroke: 1.5px white at 60% opacity

### Grid & Labels

- Horizontal lines at -48/-36/-24/-12/0 dB (2% white opacity)
- Frequency labels: 30, 60, 125, 250, 500, 1k, 2k, 4k, 8k, 16k (5px monospace, 8% white)
- dB labels on right edge

### I/O Meters

- 14px wide vertical bars flanking the spectrum
- White phosphor fill gradient, height = peak level
- Labels: "IN" / "OUT" at bottom
- Fed by `getPeakLevels()` native function

## Settings Overlay

Triggered by gear icon in header. Semi-transparent dark panel that slides over the right side of the UI.

**Contains secondary parameters:**

| Parameter | Control | Relay |
|---|---|---|
| Binaural Mode | 3-option selector (Off/Spread/Voice-Split) | `binaural_mode` ComboBoxRelay |
| Binaural Width | Medium knob (default 50%) | `binaural_width` SliderRelay |
| Recipe Rotation | Medium knob (default 0°) | `recipe_rotation` SliderRelay |
| Recipe Phase H2-H8 | 7 x Medium knob (default 0°) | `recipe_phase_h2..h8` SliderRelays |

Close button or clicking outside dismisses the overlay.

## Data Flow

### Parameter Binding (35 params via JUCE Relay System)

C++ creates one relay per parameter:

```cpp
// In PluginEditor — one for each of the 35 parameters
WebSliderRelay         ghostRelay              { "ghost" };
WebSliderRelay         phantomThresholdRelay   { "phantom_threshold" };
WebComboBoxRelay       modeRelay               { "mode" };
WebComboBoxRelay       ghostModeRelay          { "ghost_mode" };
WebComboBoxRelay       recipePresetRelay       { "recipe_preset" };
// ... all 35
```

JS connects via JUCE's provided module:

```js
import { getSliderState, getComboBoxState } from "./juce/index.js";

const ghost = getSliderState("ghost");
ghost.addEventListener("change", () => updateKnob("ghost-knob", ghost));

// Knob drag handler:
knobElement.addEventListener("input", (e) => {
    ghost.setNormalisedValue(e.detail.normValue);
});
```

### Real-Time Data (3 native functions)

```cpp
// Registered in PluginEditor constructor via Options::withNativeFunction

"getSpectrumData" → returns Array<var> of 80 floats from spectrumData[]
"getPeakLevels"   → returns Object {inL, inR, outL, outR} from atomic peaks
"getPitchInfo"    → returns Object {hz, note, preset} from currentPitch + APVTS
```

```js
// Called in requestAnimationFrame loop at ~30fps
const getSpectrum = getNativeFunction("getSpectrumData");
const getPeaks = getNativeFunction("getPeakLevels");
const getPitch = getNativeFunction("getPitchInfo");

function animationLoop() {
    requestAnimationFrame(animationLoop);
    getSpectrum().then(bins => spectrumAnalyzer.update(bins));
    getPeaks().then(p => { inputMeter.update(p.inL, p.inR); outputMeter.update(p.outL, p.outR); });
    getPitch().then(p => recipeWheel.updatePitch(p.hz, p.note, p.preset));
}
```

### Preset Selection

1. User clicks preset label → JS sets `recipe_preset` relay value
2. C++ `ParameterListener` on `RECIPE_PRESET` fires
3. C++ reads preset amplitude table (kWarmAmps, etc.) and writes to RECIPE_H2..H8
4. Those 7 relay change events fire in JS
5. Recipe wheel tank fills update automatically
6. Energy flow particle speeds update to match new amplitudes

### Resource Serving

All web files compiled into `BinaryData` via JUCE's `juce_add_binary_data()` CMake function. The `ResourceProvider` callback maps URL paths to binary resources:

```cpp
options.withResourceProvider([](const String& path) -> std::optional<Resource> {
    if (path == "/")            return Resource { loadBinary("index_html"), "text/html" };
    if (path == "/styles.css")  return Resource { loadBinary("styles_css"), "text/css" };
    if (path == "/phantom.js")  return Resource { loadBinary("phantom_js"), "application/javascript" };
    // ... etc
    return std::nullopt;
});
```

No network access. No external dependencies. Everything bundled in the plugin binary.

## File Structure

```
Source/
  PluginProcessor.h/cpp   (existing + real-time atomics + FFT — already implemented)
  PluginEditor.h/cpp      (rewritten: WebBrowserComponent + relays + native functions)
  Parameters.h            (existing, unchanged)

  WebUI/                  (new — served via ResourceProvider)
    index.html            (main document, layout structure)
    styles.css            (neumorphic panels, knobs, header, glow effects)
    phantom.js            (JUCE bridge, relay binding, data polling, mode switching)
    recipe-wheel.js       (Three.js scene: rings, tanks, energy flow, bloom)
    spectrum.js           (Canvas2D analyzer: bar + line modes, grid, labels)
    knob.js               (custom <phantom-knob> web component)
    three.module.js       (Three.js library, bundled locally)

  UI/                     (to be deleted — replaced by WebUI)

CMakeLists.txt            (updated: remove UI/*.cpp, add juce_add_binary_data for WebUI/)
```

## Colour Palette

All monochrome white phosphor (carried from mockup v21):

| Name | Value | Usage |
|---|---|---|
| background | `#06060c` | Plugin/page background |
| panelDark | `rgba(8,8,16,0.92)` | Neumorphic panel fill |
| phosphorWhite | `#ffffff` | All active accents, arc fills, glow |
| oledBlack | `#000000` | Knob wells, spectrum bg, meter bg |
| ridgeBright | `rgba(255,255,255,0.16)` | Inner lip ring |
| ridgeDark | `rgba(0,0,0,0.7)` | Gap ring |
| trackDim | `rgba(255,255,255,0.07)` | Inactive arcs, dim pathways |
| textDim | `rgba(255,255,255,0.28)` | Labels, inactive text |
| seamGlow | `rgba(255,255,255,0.16)` | Panel gap seam brightness |
| metalGradient | `#a0a8b8 → #dde0ec → #808898 → #c8ccd8` | Etched text fill |

## Parameters — UI Mapping

### Main View (always visible)

| Parameter | Location | Control |
|---|---|---|
| `mode` | Header | Toggle (Effect/Instrument) |
| `harmonic_saturation` | Harmonic Engine | Knob Large |
| `phantom_strength` | Harmonic Engine | Knob Medium (labeled "Strength") |
| `output_gain` | Output | Knob Large |
| `ghost` | Ghost | Knob Large (labeled "Amount") |
| `phantom_threshold` | Ghost | Knob Medium |
| `ghost_mode` | Ghost | Toggle (Rpl/Add) |
| `stereo_width` | Stereo | Knob Large |
| `tracking_sensitivity` | Pitch Tracker (Effect mode) | Knob Medium |
| `tracking_glide` | Pitch Tracker (Effect mode) | Knob Medium |
| `sidechain_duck_amount` | Sidechain | Knob Medium |
| `sidechain_duck_attack` | Sidechain | Knob Medium |
| `sidechain_duck_release` | Sidechain | Knob Medium |
| `deconfliction_mode` | Deconfliction (Instrument mode) | Selector |
| `max_voices` | Deconfliction (Instrument mode) | Knob Medium |
| `stagger_delay` | Deconfliction (Instrument mode) | Knob Medium |
| `recipe_preset` | Preset strip | 5 clickable labels |
| `recipe_h2` – `recipe_h8` | Recipe wheel | Visual tanks (read-only in main view) |

### Settings Overlay

| Parameter | Control |
|---|---|
| `binaural_mode` | 3-option selector |
| `binaural_width` | Knob Medium |
| `recipe_rotation` | Knob Medium |
| `recipe_phase_h2` – `recipe_phase_h8` | 7 x Knob Medium |

## Implementation Phases

### Phase 1 — C++ Shell + Resource Serving
- Rewrite PluginEditor as WebBrowserComponent shell
- Delete Source/UI/ directory
- Set up ResourceProvider with placeholder index.html
- Configure WebView2 backend with temp directory for plugins
- Add juce_add_binary_data to CMakeLists
- Verify: plugin loads, shows HTML in the webview

### Phase 2 — Static Layout + Knob Component
- Build index.html with full layout structure (header, panels, spectrum area)
- Build styles.css with all neumorphic styles from mockup
- Build knob.js web component (SVG rendering, drag interaction, double-click reset)
- Verify: static UI visible with non-functional but correctly styled knobs

### Phase 3 — Parameter Binding
- Add all 35 relay objects to PluginEditor
- Build phantom.js with JUCE bridge integration
- Wire all knobs to relays
- Wire mode toggle, preset strip, ghost mode toggle
- Implement mode switching (PitchTracker ↔ Deconfliction panel visibility)
- Verify: turn knobs, hear parameter changes in audio

### Phase 4 — Recipe Wheel
- Build recipe-wheel.js with full Three.js scene
- Holographic rings, harmonic tanks, center OLED
- Energy flow particles (spawn at center, flow outward along spokes)
- UnrealBloomPass for glow
- Wire to harmonic amplitude relays for tank fills
- Wire to getPitchInfo() for center display
- Verify: wheel animates, tanks reflect preset changes, pitch displays

### Phase 5 — Spectrum + Meters + Real-Time Data
- Build spectrum.js (bar + line modes)
- Wire to getSpectrumData() native function
- Add I/O meter elements wired to getPeakLevels()
- Add gray fundamental / white harmonics coloring
- Verify: spectrum responds to audio, meters move

### Phase 6 — Settings Overlay + Polish
- Build settings overlay panel with secondary parameter knobs
- Preset selection logic (C++ writes H2-H8 when preset changes)
- Fine-tune animations, glow intensities, spacing
- Performance profiling (target: <16ms frame time for 60fps)
- Cross-platform testing (Windows WebView2, macOS WebKit)

## Notes

- Three.js is bundled locally (~600KB minified). No CDN, no network access.
- The `juce_gui_extra` module must be linked (already is via juce_audio_utils dependency).
- `JUCE_USE_WIN_WEBVIEW2` must be enabled in CMake compile definitions for Windows WebView2 support. JUCE 8 may enable this by default — verify during Phase 1.
- WebView2 runtime is pre-installed on Windows 10 21H2+ and Windows 11. For older systems, JUCE can prompt automatic download.
- The ResourceProvider serves from BinaryData — no temp files, no file:// URLs, no security issues.
- All parameter relays must be created before the WebBrowserComponent is constructed, since they're passed via `withOptionsFrom()`.
- `getNativeFunction()` calls are async (return Promises). The animation loop must handle this gracefully — use the previous frame's data if the promise hasn't resolved yet.
