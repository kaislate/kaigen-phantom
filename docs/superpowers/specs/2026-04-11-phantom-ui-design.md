# Kaigen Phantom — UI Implementation Design Spec

**Date:** 2026-04-11
**Status:** Approved for implementation
**Mockup reference:** `.superpowers/brainstorm/37941-1775788189/content/mockup-v21.html`

## Overview

The Kaigen Phantom plugin has a complete DSP backend (29/29 tests passing, 35 APVTS parameters). This spec defines the UI implementation using pure JUCE 2D Graphics — no OpenGL. The visual language is **white phosphor neumorphic**: dark surfaces with directional lighting (top-left highlight, bottom-right shadow), OLED-black knob wells with pronounced lip/ridge borders, and bright white phosphor glow accents throughout.

## Window

- **Size:** 920 x 620 pixels (fixed, non-resizable)
- **Background:** Dark near-black (`#06060c`)

## Component Tree

```
PhantomEditor (920x620)
+-- HeaderBar (920x50)
|   +-- Logo (PNG image, 34px tall)
|   +-- "PHANTOM" label (etched metallic gradient)
|   +-- ModeToggle (Effect / Instrument)
|   +-- StatusIndicator (LED + "Idle" label)
|   +-- "KAIGEN" label (etched metallic gradient)
|
+-- BodyPanel (flex row, fills remaining height minus footer)
|   +-- RecipeWheelPanel (316px fixed width)
|   |   +-- "Recipe Engine . H2-H8" stamp label
|   |   +-- RecipeWheel (280px diameter, custom painted)
|   |   |   +-- Neumorphic raised circular mount
|   |   |   +-- Holographic rings (5-6 concentric, low opacity, slowly rotating)
|   |   |   +-- 7 harmonic tank pathways (radial bars from center, amplitude-driven)
|   |   |   +-- 7 harmonic node dots (brightness = amplitude)
|   |   |   +-- H2-H8 labels at perimeter
|   |   |   +-- Center OLED display (note name + Hz, lip/ridge border)
|   |   +-- PresetStrip (Warm / Aggr / Hollow / Dense / Custom)
|   |
|   +-- GlowSeam (3px vertical divider)
|   |
|   +-- RightPanel (flex column)
|       +-- ControlRow1 (fixed height)
|       |   +-- HarmonicEnginePanel: Saturation (lg), Strength (md)
|       |   +-- GhostPanel: Amount (lg), Threshold (md), ghost_mode toggle (Rpl/Add)
|       |   +-- OutputPanel: Gain (lg)
|       |
|       +-- GlowSeam (horizontal)
|       |
|       +-- ControlRow2 (fixed height, mode-switched)
|       |   EFFECT MODE:
|       |   +-- PitchTrackerPanel: Sensitivity (md), Glide (md)
|       |   +-- SidechainPanel: Amount (md), Attack (md), Release (md)
|       |   +-- StereoPanel: Width (lg)
|       |   INSTRUMENT MODE:
|       |   +-- DeconflictionPanel: Mode (selector), MaxVoices (md), StaggerDelay (md)
|       |   +-- SidechainPanel: Amount (md), Attack (md), Release (md)
|       |   +-- StereoPanel: Width (lg)
|       |
|       +-- GlowSeam (horizontal)
|       |
|       +-- SpectrumRow (flex, fills remaining height)
|           +-- InputMeter (14px wide)
|           +-- SpectrumAnalyzer (fills width, bar + line modes)
|           +-- OutputMeter (14px wide)
|
+-- FooterBar (920x38)
    +-- Bypass, Recipe, Ghost, Binaural, Deconflict, Settings buttons
    +-- Version label ("KAIGEN PHANTOM . v0.1.0")
```

## Visual Language

### Colour Palette (PhantomColours.h)

All colours are monochrome white phosphor. No semantic colours (amber, green, purple) in this version.

| Name | Value | Usage |
|---|---|---|
| `background` | `#06060c` | Plugin background fill |
| `panelDark` | `#08081092` (with alpha) | Neumorphic panel fill |
| `panelHighlight` | `#ffffff06` | Top-left light spill on panels |
| `panelShadowLight` | `#ffffff06` | Outer neumorphic highlight (top-left) |
| `panelShadowDark` | `#000000c0` | Outer neumorphic shadow (bottom-right) |
| `phosphorWhite` | `#ffffff` | All active accents, arc fills, text glow |
| `oledBlack` | `#000000` | Knob wells, spectrum background, meter background |
| `ridgeBright` | `#ffffff28` | Inner lip ring on OLED wells |
| `ridgeDark` | `#000000b3` | Gap ring between lip and outer edge |
| `ridgeOuter` | `#ffffff12` | Outer edge ring |
| `trackDim` | `#ffffff12` | Inactive arc track, dim tank pathways |
| `textDim` | `#ffffff48` | Secondary labels, inactive text |
| `textGlow` | `#ffffff` | Primary values, drawn with multi-pass glow |
| `seamGlow` | `#ffffff29` | Panel gap seam center brightness |
| `etchDark` | `#000000f2` | Top shadow on etched text |
| `etchLight` | `#ffffff12` | Bottom catch on etched text |
| `metalGradient` | linear `#a0a8b8` to `#dde0ec` to `#808898` to `#c8ccd8` | Etched metallic text fill |

### Neumorphic Panel Rendering (NeumorphicPanel base class)

Every control group panel inherits this rendering:

1. **Fill:** `panelDark` with 8px rounded corners
2. **Inset shadows:** Dark gradient overlays on top-left edges (4px blur) and bottom-right edges (16px blur) creating the sunken-into-surface look
3. **Top-left light spill:** Radial gradient ellipse at (-25%, -35%) position, `panelHighlight` fading to transparent
4. **Outer shadows:** Two offset rounded rects behind the panel — `panelShadowLight` offset (-5, -5) with 14px blur, `panelShadowDark` offset (5, 5) with 18px blur

### Knob Rendering (PhantomLookAndFeel::drawRotarySlider)

Two sizes, same rendering at different scales:

| | Large | Medium |
|---|---|---|
| Outer diameter | 100px | 72px |
| OLED inset | 13px | 9px |
| Arc stroke | 2.8px | 2.8px |
| Value font | 12px bold | 10px bold |
| Label font | 5px | 5px |

Rendering layers (back to front):

1. **Neumorphic outer shadow** — Two offset filled circles: highlight top-left (-5,-5, 12px blur, `panelShadowLight`), shadow bottom-right (6, 6, 16px blur, `panelShadowDark`)
2. **Volcano surface** — Filled circle with radial gradient: bright at 32 deg / 28 deg from top-left, fading to dark at bottom-right
3. **OLED well** — Filled black circle inset from volcano edge
4. **Lip/ridge** — Three concentric stroked arcs at the OLED boundary:
   - Inner: 1.5px stroke, `ridgeBright`
   - Gap: 1.5px stroke, `ridgeDark`
   - Outer: 1px stroke, `ridgeOuter`
5. **Arc track** — `Path::addArc` full sweep, `trackDim`, 3.5px stroke
6. **Arc value** — `Path::addArc` proportional to value, `phosphorWhite`, 2.8px stroke
7. **Arc glow** — Same path as arc value but 6px stroke at 30% opacity (halo effect)
8. **Value text** — Drawn 3 times: first at 40% opacity with 2px offset blur simulation, then at 70%, then at 100% — creates OLED glow without GPU shaders
9. **Label text** — `textDim` colour, small caps, centered below value

**Double-click reset:** `PhantomKnob::mouseDoubleClick()` resets to the parameter's default value from APVTS.

### Recipe Wheel Rendering (RecipeWheel::paint)

The most complex single component. Painted in layers:

1. **Neumorphic raised mount** — Same directional gradient as knob volcano but 280px diameter. Outer neumorphic shadows match panel style.
2. **Holographic rings** — 5-6 concentric `Path::addArc` full circles at radii 28-126px, stroke width 0.3-0.8px, opacity 0.035-0.13. Rotated by small increments on each Timer tick to create slow ethereal drift.
3. **Tank pathway tracks** — 7 lines from center (140,140) radiating outward to perimeter, drawn with `trackDim` at 5px stroke.
4. **Tank fills** — For each harmonic H2-H8: a bright white line from a threshold radius outward, length proportional to `RECIPE_H2..H8` parameter value. Uses gradient from 55% opacity at origin to 8% at tip. Drawn at 3.5px stroke.
5. **Tank caps** — Small filled circles (radius 3px) at each fill endpoint, opacity proportional to amplitude.
6. **Harmonic node dots** — At each spoke endpoint, a large soft circle (glow halo) + small bright circle (core), mimicking v7's dual-element ethereal style.
7. **Scan line** — Single line from center to radius 122px, rotating slowly. Very low opacity (0.05).
8. **Center OLED display** — 96px diameter black circle with the same triple-ring lip/ridge as knobs. Text content:
   - Line 1: "FUND" (6px, dim)
   - Line 2: "A2 . 110Hz" (14px bold, bright white glow) — driven by `pitchTracker.getSmoothedPitch()` in Effect mode, or current MIDI note in Instrument mode
   - Line 3: Current preset name (7px, white)
9. **H2-H8 labels** — Positioned at perimeter, 8px bold monospace, `textDim` with subtle glow shadow.

**Timer:** RecipeWheel runs at ~15fps to animate ring rotation, scan line, and node shimmer. Pitch display updates at the same rate.

### Spectrum Analyzer

- **Data:** 80 frequency bins, log-spaced from ~30Hz to ~16kHz
- **Update rate:** ~30fps via Timer pulling from lock-free FIFO
- **Two render modes** toggled by a small button in the panel corner:

**Bar mode:**
- 80 vertical rectangles, width = panel_width/80 minus 1px gap
- Fill: vertical gradient — 50% white at bottom to 4% at top
- Peak cap: 1px bright line at bar top with 3px glow
- Fundamental band (bins covering the detected pitch) drawn in gray (15% opacity) behind the white harmonic bars

**Line mode:**
- `Path` connecting the tops of all 80 bins with `Path::cubicTo()` for smooth curves
- Stroke: 1.5px white at 60% opacity
- Fill below the curve: vertical gradient from 20% white at bottom to 0% at top
- Same fundamental-vs-harmonic colouring (gray curve behind white)

**Grid:** Horizontal lines at -48/-36/-24/-12/0 dB, vertical lines at decade frequencies. All drawn at 2% white opacity. Frequency and dB labels in 5px monospace at 8% opacity.

### I/O Level Meters

- 14px wide, full spectrum row height
- Black background with same neumorphic inset as panels
- Fill: vertical gradient from 35% white at bottom to 5% at top, height proportional to peak level
- Peak hold line: 1px at 50% white
- "IN" / "OUT" label at bottom, 5px vertical text

### Header Bar

- Stippled dot texture: 3px grid of 0.5px dots at 2% white opacity (drawn via tiled `Path` or `Image`)
- Bottom edge glow seam: radial gradient, brighter at center, fading to edges
- PHANTOM / KAIGEN text: metallic gradient fill with etched effect — dark `DropShadow` offset (0, -1) above the text (the cut), light shadow offset (0, 1) below (light catching the groove floor)

### Footer Bar

- Same stipple texture as header
- Top edge glow seam
- Navigation buttons: neumorphic inset (inactive) or raised (active) with LED dot

### Glow Seams

- 3px wide/tall `Component`
- Draws a 1px center line with linear gradient (transparent edges, 16% white center)
- Behind it, a wider (8px) blurred gradient at 4% for the soft bloom effect

## Data Flow

### Parameter Binding

All 35 parameters bind via standard JUCE attachments:

```
juce::AudioProcessorValueTreeState::SliderAttachment  -> PhantomKnob
juce::AudioProcessorValueTreeState::ButtonAttachment   -> Toggle buttons
juce::AudioProcessorValueTreeState::ComboBoxAttachment -> Mode toggle, preset selector, deconfliction mode
```

Each panel component creates and owns its attachments after creating its child controls.

### Real-Time Data (Audio Thread to UI)

Added to `PhantomProcessor`:

```cpp
// Atomic values set in processBlock(), read by UI Timer
std::atomic<float> currentPitch { -1.0f };    // from pitchTracker.getSmoothedPitch()
std::atomic<float> peakInL  { 0.0f };
std::atomic<float> peakInR  { 0.0f };
std::atomic<float> peakOutL { 0.0f };
std::atomic<float> peakOutR { 0.0f };

// Spectrum data via lock-free FIFO
juce::AbstractFifo spectrumFifo { 2 };        // double-buffer
std::array<std::array<float, 80>, 2> spectrumBuffers;
```

In `processBlock()`:
- Compute peak levels from input/output buffers, store to atomics
- Run FFT on output buffer, bin into 80 log-spaced bands, push to FIFO

### UI Timer

`PhantomEditor` owns a single `juce::Timer` running at 30fps:

```cpp
void timerCallback() override
{
    // Pitch -> RecipeWheel
    float pitch = processor.currentPitch.load();
    recipeWheelPanel.updatePitch(pitch);

    // Peaks -> Meters
    inputMeter.setLevel(processor.peakInL.load(), processor.peakInR.load());
    outputMeter.setLevel(processor.peakOutL.load(), processor.peakOutR.load());

    // Spectrum -> Analyzer (pull from FIFO if available)
    spectrumAnalyzer.pullSpectrum(processor);
}
```

Components only call `repaint()` when their data actually changes (pitch moved, level changed, new spectrum frame).

### Mode Switching

`PhantomEditor` registers as a `juce::AudioProcessorValueTreeState::Listener` on the `MODE` parameter.

```cpp
void parameterChanged(const juce::String& id, float value) override
{
    if (id == ParamID::MODE)
    {
        bool isEffect = (value < 0.5f);
        pitchTrackerPanel.setVisible(isEffect);
        deconflictionPanel.setVisible(!isEffect);
        controlRow2.resized();  // re-layout
    }
}
```

Both panels exist at all times. Only visibility toggles.

### Recipe Preset Selection

When a preset label is clicked:
1. `RECIPE_PRESET` parameter updates via attachment
2. A `ParameterListener` on the editor fires
3. Reads preset's hardcoded amplitude values (defined in a static table)
4. Writes to `RECIPE_H2` through `RECIPE_H8` via `getParameter()->setValueNotifyingHost()`
5. Tank fills on RecipeWheel update automatically (they read APVTS values in `paint()`)

## File Structure

```
Source/
  PluginProcessor.h/cpp        (add atomics, spectrum FIFO)
  PluginEditor.h/cpp           (layout, Timer, mode switching)
  Parameters.h                 (existing, unchanged)

  UI/
    PhantomLookAndFeel.h/cpp   (~400-500 lines, all custom rendering)
    PhantomColours.h           (colour constants namespace)

    HeaderBar.h/cpp
    FooterBar.h/cpp

    RecipeWheel.h/cpp          (~250-300 lines, custom painted)
    RecipeWheelPanel.h/cpp     (wheel + preset strip container)

    PhantomKnob.h/cpp          (Slider subclass, size enum, double-click reset)
    NeumorphicPanel.h/cpp      (base class for panel backgrounds)

    HarmonicEnginePanel.h/cpp  (Saturation lg, Strength md)
    GhostPanel.h/cpp           (Amount lg, Threshold md, Mode toggle)
    OutputPanel.h/cpp          (Gain lg)
    PitchTrackerPanel.h/cpp    (Sensitivity md, Glide md)
    SidechainPanel.h/cpp       (Amount md, Attack md, Release md)
    StereoPanel.h/cpp          (Width lg)
    DeconflictionPanel.h/cpp   (Mode selector, MaxVoices md, StaggerDelay md)

    SpectrumAnalyzer.h/cpp     (bar + line modes, 80 bins)
    LevelMeter.h/cpp           (thin vertical I/O meter)
    GlowSeam.h/cpp             (3px divider component)

  Engines/                     (existing, unchanged)
```

~20 new files. Most are 80-200 lines. Heaviest: PhantomLookAndFeel (~500 lines), RecipeWheel (~300 lines), SpectrumAnalyzer (~200 lines).

## Implementation Phases

### Phase 1 — Foundation
- `PhantomColours.h`
- `PhantomLookAndFeel` (knob rendering, panel backgrounds, button styles)
- `PhantomKnob` (Slider subclass with double-click reset, Large/Medium enum)
- `NeumorphicPanel` (base class)
- `PluginEditor` (920x620 skeleton layout with panel regions)
- `HeaderBar` + `FooterBar`
- `GlowSeam`

**Verifiable result:** Dark neumorphic shell visible in DAW with header/footer chrome.

### Phase 2 — Knob Panels (Top Row)
- `HarmonicEnginePanel`
- `GhostPanel`
- `OutputPanel`
- Wire all APVTS SliderAttachments

**Verifiable result:** Turn knobs in the UI, hear parameter changes in audio output.

### Phase 3 — Mode-Switched Row
- `PitchTrackerPanel`
- `SidechainPanel`
- `StereoPanel`
- `DeconflictionPanel`
- Mode toggle listener swaps panel visibility

**Verifiable result:** Full right panel working. Toggle Effect/Instrument, see panels swap.

### Phase 4 — Recipe Wheel
- `RecipeWheel` (full custom paint: rings, tanks, center OLED, labels)
- `RecipeWheelPanel` (wheel + preset strip)
- Add `std::atomic<float> currentPitch` to Processor
- Timer polling for pitch display

**Verifiable result:** Recipe wheel shows harmonic levels, preset selection changes tank fills, center displays detected pitch.

### Phase 5 — Spectrum & Meters
- Add spectrum FIFO + peak atomics to Processor
- `SpectrumAnalyzer` (bar + line modes, fundamental in gray, harmonics in white)
- `LevelMeter` (I/O)
- Timer integration

**Verifiable result:** Full UI complete. Real-time spectrum, meters responding to audio.

### Phase 6 — Polish
- Settings panel (binaural_width, recipe phases, phantom_strength — expandable/overlay)
- Spacing/sizing fine-tuning
- Performance profiling (ensure paint stays under 2ms)
- Any visual issues found during DAW testing

## Parameters — UI Mapping

### Always Visible (Main View)

| Parameter | Panel | Control Type |
|---|---|---|
| `MODE` | HeaderBar | Toggle (Effect/Instrument) |
| `HARMONIC_SATURATION` | HarmonicEngine | Knob Large (labeled "Saturation") |
| `PHANTOM_STRENGTH` | HarmonicEngine | Knob Medium (labeled "Strength") |
| `GHOST` | Ghost | Knob Large (labeled "Amount" — wet/dry blend) |
| `PHANTOM_THRESHOLD` | Ghost | Knob Medium |
| `GHOST_MODE` | Ghost | Toggle (Rpl/Add) |
| `OUTPUT_GAIN` | Output | Knob Large |
| `TRACKING_SENSITIVITY` | PitchTracker (Effect mode) | Knob Medium |
| `TRACKING_GLIDE` | PitchTracker (Effect mode) | Knob Medium |
| `SIDECHAIN_DUCK_AMOUNT` | Sidechain | Knob Medium |
| `SIDECHAIN_DUCK_ATTACK` | Sidechain | Knob Medium |
| `SIDECHAIN_DUCK_RELEASE` | Sidechain | Knob Medium |
| `STEREO_WIDTH` | Stereo | Knob Large |
| `DECONFLICTION_MODE` | Deconfliction (Instrument mode) | Selector |
| `MAX_VOICES` | Deconfliction (Instrument mode) | Knob Medium |
| `STAGGER_DELAY` | Deconfliction (Instrument mode) | Knob Medium |
| `RECIPE_PRESET` | RecipeWheelPanel | Preset strip buttons |
| `RECIPE_H2` - `RECIPE_H8` | RecipeWheel | Visual tanks (not directly draggable in main view) |

### Settings Panel (Phase 6)

| Parameter | Control Type |
|---|---|
| `BINAURAL_MODE` | Selector |
| `BINAURAL_WIDTH` | Knob Medium |
| `PHANTOM_STRENGTH` | Knob Medium |
| `RECIPE_ROTATION` | Knob Medium |
| `RECIPE_PHASE_H2` - `RECIPE_PHASE_H8` | 7 x Knob Medium |

## Notes

- **Strength** in HarmonicEnginePanel maps to `PHANTOM_STRENGTH` (controls how strongly the phantom harmonics are applied). The fader from the mockup has been removed — `GHOST` (wet/dry blend) lives exclusively in the GhostPanel as "Amount" to avoid duplicate controls for the same parameter.
- The spectrum FFT needs to be lightweight. Use a 512-sample FFT with Hann window, computed on every buffer. Bin into 80 log-spaced bands. This adds negligible CPU.
- RecipeWheel harmonic tanks are read-only in the main view (driven by preset selection). Direct per-harmonic editing is available in the Settings panel expanded view.
- All Timer-driven repaints should be guarded by dirty flags to avoid unnecessary paint calls.
