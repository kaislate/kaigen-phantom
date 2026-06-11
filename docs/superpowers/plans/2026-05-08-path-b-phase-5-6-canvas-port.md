# Path B — Phase 5.6: Canvas-Drawing Port

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the WebView2 canvas-drawing code (knob, recipe wheel, spectrum, oscilloscope, circuit-board) faithfully to native JUCE Graphics. Phase 5.5 brought structural parity; Phase 5.6 brings visual character — neumorphic OLED knobs, holographic recipe wheel, Bézier-curve spectrum, multi-trace oscilloscope, animated circuit-board.

**Why this exists:** Phase 5.5 ported the CSS palette but NOT the JS canvas code that draws each visually distinctive widget. The result was structurally correct but felt like a mockup. Phase 5.6 closes the gap by translating each canvas-rendered widget from JS to JUCE one at a time.

**Source of truth:** the WebView2 canvas code at `Source/WebUI/`:
- `knob.js` (417 lines, SVG) — every knob
- `recipe-wheel.js` (299 lines, Canvas 2D) — the radial harmonic widget
- `spectrum.js` (514 lines, Canvas 2D) — frequency visualizer (Bézier curves)
- `oscilloscope.js` (296 lines, Canvas 2D) — time-domain visualizer (3 layers)
- `circuit-board.js` (219 lines, Canvas 2D) — animated advanced-panel background

**Architecture:** each ported widget gets:
- A `juce::Component` subclass with a custom `paint()` matching the JS draw stack layer-by-layer
- A `juce::Timer` (60 Hz or task-specific) replacing `requestAnimationFrame` where animation is needed
- Faithful color/gradient/dimension constants (no eyeballing — read the JS)
- Same parameter wiring as before (no behavior changes; this is a paint-code rewrite)

**Tech Stack:** JUCE 8.0.4, C++20, MSVC 17.14.

**Branch:** continue on `feature/pr3b-macro-editor`.

**Effort estimate:** ~5 days of focused work, broken into 7 tasks. Each task ends with a commit + a side-by-side comparison check against WebView2.

---

## Task ordering rationale

Order by visual impact (highest first) and dependency:

1. **PhantomKnob rewrite** — affects every knob in the editor. Single biggest improvement. Must come first because nearly every panel hosts knobs.
2. **RecipeWheel rewrite** — most visually distinctive single widget. The user explicitly called this out as "ridiculous" in the current port.
3. **Spectrum rewrite** — primary monitoring UI; current bar-graph is the wrong algorithm entirely (should be Bézier curves with gradients).
4. **Oscilloscope rewrite** — 3-layer time-domain trace replaces single line.
5. **CircuitBoard** — animated background for the Advanced panel.
6. **Polish** — slot row final tweaks, button width fixes, layout audit.
7. **Final smoke** — side-by-side review with WebView2.

---

## Task 1: PhantomKnob rewrite — neumorphic body + OLED well + arc indicator + value text

**Files:**
- Replace: `Source/UI/widgets/PhantomKnob.h` and `.cpp`
- Possibly modify: `Source/UI/PhantomLookAndFeel.cpp` — remove the `drawRotarySlider` override since PhantomKnob now does its own drawing

**Goal:** every knob renders as a neumorphic raised body with a black OLED well at center, a faint background arc track, a bright glow halo + sharp white indicator arc showing value, and 3-layer value text (0.3, 0.6, 1.0 alpha) over the OLED.

**JS reference:** `Source/WebUI/knob.js` — particularly the `_render()` method (lines 200-300+).

**Visual layers (top to bottom of paint stack — translate exactly):**

1. **Neumorphic raised body** — radial-gradient ellipse at 35% / 30%:
   - `rgba(255,255,255,0.24)` at 0% (top-left highlight)
   - through semi-transparent gradients to
   - `rgba(0,0,0,0.07)` at 100% (bottom-right shadow)
   - Plus four box-shadow layers (offset shadows ±3-7 px, blur 12-32 px) to give the bulge effect:
     - TL: `-3px -3px 12px rgba(255,255,255,0.70)` + `-5px -6px 22px rgba(255,255,255,0.34)`
     - BR: `3px 4px 14px rgba(0,0,0,0.34)` + `5px 7px 24px rgba(0,0,0,0.16)`
   - Sized variants per knob size (small=56, medium=88, large=114 px diameters):
     - small: shadow offsets 2/3/10/17 px
     - large: shadow offsets 4/7/18/32 px

2. **OLED well** — a black ellipse at center with three concentric stroked rings forming the bezel:
   - Inner stroke: `rgba(255,255,255,0.28)` at 0.75 px width (bright inner edge)
   - Dark step ring: `rgba(0,0,0,0.92)` at 0.75 px width, radius `oledR + 0.75` (inset darkness)
   - Outer soft halo: `rgba(255,255,255,0.10)` at 0.5 px width, radius `oledR + 1.5`

3. **Arc track (faint background)** — an arc spanning 135° to 405° (270° sweep):
   - `rgba(255,255,255,0.06)` at 3.5 px width
   - radius = `oledR - 4`

4. **Glow halo arc** (drawn when value > 0):
   - `rgba(255,255,255,0.45)` at 6 px width
   - Same arc, but only from 135° to `135 + (270 × value)°`
   - Apply Gaussian blur (stdDeviation = 2 px) — emulate via `juce::DropShadow` with a clipping mask, OR via stacked alpha-falloff strokes if DropShadow on a Path is too expensive

5. **Sharp value arc** (top of glow):
   - `#fff` (full white) at 2.8 px width
   - Same arc as #4 but no blur
   - Drawn opaque so the user sees a clean filled arc

6. **Value text (3-layer shadow effect)** — three rendering passes of the same string:
   - Layer 1: alpha 0.30 (faint outer halo, slight Y offset of +1)
   - Layer 2: alpha 0.60 (middle layer)
   - Layer 3: alpha 1.00 (sharp top layer)
   - Font: Courier New, weight 700, size scales with knob size (small ~9 px, medium ~12 px, large ~14 px)
   - Position: vertically centered or slightly above OLED center
   - Drag state: text grows on drag (small→13 px, medium→18 px, large→22 px) with 150 ms ease

7. **Label text** (waveform/special knobs only — most knobs skip this):
   - Below OLED center
   - Font: Kalam / Segoe Script (cursive), weight 400, size scales with knob

8. **Waveform polyline** (only for `data-oled="waveform"` knobs — currently none in our native editor; defer):
   - 64-point polyline plotting `shapedWave()`-derived points
   - White stroke at 1.5 px width, 0.85 opacity
   - Range: x ∈ [cx ± 55% × oledR], y ∈ [cy − 20% × oledR ± 22% × oledR]

### Code skeleton

```cpp
// Source/UI/widgets/PhantomKnob.h
class PhantomKnob : public juce::Component
{
public:
    enum class Size { Small, Medium, Large };

    PhantomKnob(juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& paramID,
                 Size size,
                 const juce::String& labelText);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    juce::Slider& getSlider() noexcept { return slider; }

private:
    void paintBody(juce::Graphics& g, juce::Point<float> centre, float radius);
    void paintOLED(juce::Graphics& g, juce::Point<float> centre, float oledR);
    void paintArcTrack(juce::Graphics& g, juce::Point<float> centre, float arcR);
    void paintIndicatorArc(juce::Graphics& g, juce::Point<float> centre, float arcR, float value);
    void paintValueText(juce::Graphics& g, juce::Point<float> centre, juce::String text);
    juce::String formatValue() const;

    Size size;
    juce::String labelText;
    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    bool isDragging { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomKnob)
};
```

### Steps

- [ ] **Step 1: Read `Source/WebUI/knob.js`** — particularly `_render()`, `describeArc()`, `_buildWaveformPoints()`. Note exact gradient stops, shadow offsets, arc start/end angles, font sizes per size variant.

- [ ] **Step 2: Implement PhantomKnob.h** with the layer interface above.

- [ ] **Step 3: Implement PhantomKnob.cpp** layer-by-layer:
  - `paintBody`: radial gradient + 4-layer drop shadow (use `juce::DropShadow` for outer offsets; use `Path::createPathWithRoundedCorners` if needed for the bevel inset effect)
  - `paintOLED`: `fillEllipse(black)` + 3 `drawEllipse(stroke)` calls at increasing radii
  - `paintArcTrack`: `Path::addCentredArc()` from 135° to 405° + `strokePath(0.06f white)`
  - `paintIndicatorArc`: same `Path::addCentredArc()` but partial; draw glow with thicker alpha 0.45 stroke + sharp top with width 2.8 white
  - `paintValueText`: three `drawText` passes with alpha 0.3 / 0.6 / 1.0
  - Format value via `formatValue()` — read from APVTS param's NormalisableRange; default to `String(0.5f, 2)` (e.g. "0.50") unless slider has `getTextFromValueFunction` set

- [ ] **Step 4: Drag interaction** — increase value text font size by ~1.4× during drag (small→13 px, medium→18 px, large→22 px). Use a `juce::ComponentAnimator` or animate via Timer + lerp.

- [ ] **Step 5: Update `PhantomLookAndFeel`** — REMOVE the `drawRotarySlider` override. Phantom-Knob now paints itself as a Component, not as a JUCE Slider via LookAndFeel. The hidden `juce::Slider` member exists only for APVTS attachment + double-click reset, not for visuals.

- [ ] **Step 6: Build and smoke** — open in Live; every knob in the editor should now have the proper neumorphic body + OLED well + value text. Compare against the WebView2 reference image.

- [ ] **Step 7: Iteration** — likely 2-4 iteration commits as we tune gradient stops, shadow offsets, font sizes. Each iteration ends with a side-by-side screenshot vs WebView2.

- [ ] **Step 8: Commit**

```bash
git add Source/UI/widgets/PhantomKnob.{h,cpp} Source/UI/PhantomLookAndFeel.cpp
git commit -m "feat(path-b): PhantomKnob full canvas port — neumorphic body + OLED + arc indicator"
```

---

## Task 2: RecipeWheel rewrite — holographic rings + animated spokes + particles + scan line

**Files:**
- Replace: `Source/UI/widgets/RecipeWheel.h` and `.cpp`

**Goal:** the most visually-dense widget — 6 rotating holographic rings, 7 spoke meters with amplitude-driven gradients, 140 traveling particles, a rotating scan line, and a pulsing center glow. All running at 30 Hz on a JUCE Timer.

**JS reference:** `Source/WebUI/recipe-wheel.js` — read it carefully. The 30 fps throttle, the per-spoke gradient + glow, the particle spawn/wrap logic, the scan line gradient — all need exact translation.

**Visual layers (top to bottom of paint stack — translate exactly):**

1. **Background radial gradient**:
   - `radial-gradient` from `rgba(10,10,20,0.4)` at center → `rgba(3,3,8,0.6)` at 60% → `rgba(0,0,0,0.8)` at edge
   - Filled circle at radius R = `min(w,h)*0.5 - 4`

2. **Holographic rings (6 concentric, each rotating independently)**:
   - `ringRadii  = [0.92, 0.77, 0.60, 0.40, 0.22, 0.96]` (fraction of R)
   - `ringWidths = [0.8, 0.6, 0.5, 0.6, 0.8, 0.3]` (px)
   - `ringAlphas = [0.18, 0.12, 0.08, 0.13, 0.22, 0.06]`
   - `ringSpeeds = [0.015, -0.020, 0.010, -0.025, 0.012, -0.008]` (rad/frame)
   - Each ring is a partial arc starting at `ringRot[i]` covering full TAU; rotation makes it appear to spin
   - Stroke color: `rgba(255,255,255, ringAlphas[i])`

3. **Spoke tracks (dim full-length lines for each of 7 spokes)**:
   - Hot spoke (dragging or hovering): `rgba(255,255,255,0.12)`, width 5 px
   - Cold spoke: `rgba(255,255,255,0.05)`, width 5 px
   - `lineCap = round`, `lineJoin = round`

4. **Spoke glow halo (per spoke, proportional to amplitude)**:
   - Color: `rgba(255,255,255, 0.25*amp + (hot ? 0.15 : 0))`
   - Width: `(hot ? 12 : 9)` px
   - Drawn first (behind fill line) so it appears as a soft halo

5. **Spoke fill line with gradient (sharp, per spoke)**:
   - Linear gradient along spoke direction:
     - Stop 0 (inner end): `rgba(255,255,255, 0.55*amp + (hot ? 0.2 : 0))`
     - Stop 1 (outer end): `rgba(255,255,255, 0.08*amp + (hot ? 0.1 : 0))`
   - Width: `(hot ? 5 : 3.5)` px
   - Path: from innerR to `innerR + (outerR - innerR) * amp`

6. **Spoke cap dot (at fill endpoint)**:
   - `radius = (hot ? 5 : 3) px`
   - Color: `rgba(255,255,255, 0.8*amp + (hot ? 0.2 : 0))`

7. **Outer node circle (at outerR endpoint per spoke)**:
   - Halo (radial gradient): `rgba(255,255,255, 0.4*amp)` at center → transparent at radius `nodeSize*3`
   - Inner solid: radius `nodeSize*0.5`, color `rgba(255,255,255, 0.5+amp*0.45)`
   - `nodeSize = (3 + amp*10)` px

8. **Spoke label (on hover/drag, hot && amp > 0)**:
   - Text: `Math.round(amp*100) + '%'`
   - Position: at cap dot offset +16 px from center direction
   - Font: bold 9 px monospace
   - Color: `rgba(255,255,255,0.9)`

9. **Particles (20 per spoke, 140 total)**:
   - Per particle: small filled circle radius 1.4 px
   - Position: `r = innerR + (outerR - innerR) * progress` along spoke direction
   - Color: `rgba(255,255,255, amp*(1 - progress)*0.85)`
   - Speed: `0.004 + amp*0.020` (normalized progress per frame)
   - Lifecycle: progress 0→1, then wraps back to random start

10. **Scan line (rotating radial gradient)**:
    - `scanAngle += 0.018` per frame
    - Radial line from center to `(outerR*0.95)` in direction of scanAngle
    - Linear gradient along the line:
      - Stop 0: `rgba(255,255,255, 0)`
      - Stop 0.7: `rgba(255,255,255, 0.04)`
      - Stop 1: `rgba(255,255,255, 0.12)`
    - Width: 1.5 px

11. **Center glow (pulsing)**:
    - `shimmerT += 0.05` per frame
    - `pulse = 0.7 + 0.3*sin(shimmerT)`
    - Radial gradient from center to innerR:
      - Stop 0: `rgba(255,255,255, 0.25*pulse)`
      - Stop 0.5: `rgba(255,255,255, 0.08*pulse)`
      - Stop 1: `rgba(255,255,255, 0)`

### Steps

- [ ] **Step 1: Read `Source/WebUI/recipe-wheel.js`** — note `draw()`, `drawRing()`, `drawSpoke()`, `drawParticles()`, `drawScan()`, `drawCenterGlow()`. Pay attention to `hitSpoke()` for click handling.

- [ ] **Step 2: Replace RecipeWheel.h** with a clean Component subclass (no hidden Slider; use direct mouseDown/Drag/Up to set H2-H8 APVTS params via `RangedAudioParameter::setValueNotifyingHost`).

- [ ] **Step 3: Replace RecipeWheel.cpp** layer-by-layer per the spec above. Each `drawXxx()` helper paints one layer.

- [ ] **Step 4: 30 Hz Timer** — `class RecipeWheel : public juce::Component, private juce::Timer { ... void timerCallback() override { repaint(); } };` Started in ctor at `startTimerHz(30)`, stopped in dtor.

- [ ] **Step 5: Particle simulation** — own `std::array<Particle, 140>`. Each tick advance progress; wrap when ≥ 1. Each particle stores `(spokeIndex, progress, randomOffset)`.

- [ ] **Step 6: Hit testing** — `hitSpoke(Point<int>)` returns spoke index if click is within `max(12 px, R*0.06)` perpendicular distance to a spoke line. Drag updates corresponding harmonic amplitude.

- [ ] **Step 7: APVTS wiring** — read amplitudes from H2-H8 params (recipe_h2 through recipe_h8) on each tick to drive spoke fills. Drag writes back to params via attachment (or direct `setValueNotifyingHost`).

- [ ] **Step 8: Build and iterate** against WebView2 reference. Likely 2-4 iteration commits.

- [ ] **Step 9: Commit**

```bash
git commit -m "feat(path-b): RecipeWheel full canvas port — rings + spokes + particles + scan"
```

---

## Task 3: Spectrum rewrite — Bézier curves + gradients + grid + Split/Combined modes

**Files:**
- Replace: `Source/UI/visualizers/Spectrum.h` and `.cpp`

**Goal:** the spectrum visualizer renders as smoothed area-fill curves (input + output + peak hold), not bars. With frequency grid, dB grid, and labels. Two view modes: Combined (full width) and Split (engine A | engine B side-by-side).

**JS reference:** `Source/WebUI/spectrum.js` — particularly `_renderCombined()` and `_renderSplit()`.

**Visual layers:**

1. **Black background** — `g.fillAll(black)`

2. **Frequency grid** — vertical lines at major octaves [50, 100, 200, 500, 1000, 2000, 5000, 10000] Hz; horizontal dB lines at [-12, -24, -36, -48] dB. Translate Hz → x via log scale `freqToX(hz, w)`; dB → y linearly across [0, h] mapped from [0, -60] dBFS.

3. **Frequency labels** (bottom-left, monospace, color `rgba(255,255,255,0.22)`): "30", "100", "300", "1k", "3k", "10k" at their respective x positions.

4. **dB labels** (right edge): "0", "-12", "-24", "-36", "-48".

5. **Input signal curve** (gray dim layer):
   - Quadratic Bézier-interpolated path from 80 spectrum bins
   - Path closed at bottom for fill
   - Gradient fill: top `rgba(160,160,175,0.20)` → bottom `rgba(160,160,175,0.04)`
   - Stroke: `rgba(160,160,175,0.50)` at width `max(1, h*0.004)`

6. **Output signal curve** (bright white main layer):
   - Same Bézier interpolation
   - Gradient fill: top `rgba(255,255,255,0.28)` → middle `rgba(255,255,255,0.10)` → bottom `rgba(255,255,255,0.02)`
   - Stroke: `rgba(255,255,255,0.90)` at width `max(1.2, h*0.006)`
   - Glow: `juce::DropShadow` with color `rgba(255,255,255,0.6)` and blur 4

7. **Peak-hold curve** (thin):
   - Stroke: `rgba(255,255,255,0.25)`, width 1 px
   - Same Bézier path, but using `peakOut[]` array (decays at 0.003/frame)

8. **Crossover line** (vertical dashed):
   - Read `phantom_threshold` (or `a_/b_phantom_threshold` in Split mode) from APVTS
   - Color: `rgba(80,142,215,0.38)`
   - Dashed pattern `[3, 4]`
   - Label: rounded Hz value in `rgba(80,142,215,0.60)` at top

9. **Split mode**: each half is rendered identically to Combined but clipped to its half + uses per-engine spectrum data. Center divider thin white line. Engine labels "A" (top-left) and "B" (top-right) in `rgba(74,144,226,0.70)`.

### Bézier curve construction

```cpp
juce::Path buildSpectrumPath(const float* bins80, int w, int h)
{
    juce::Path p;
    auto pointAt = [&](int i) -> juce::Point<float> {
        const float hz = binToFreq(i);
        const float x  = freqToX(hz, (float) w);
        const float y  = (float) h * (1.0f - bins80[i]);
        return { x, y };
    };

    p.startNewSubPath(pointAt(0));
    for (int i = 1; i < 79; ++i)
    {
        const auto a = pointAt(i);
        const auto b = pointAt(i + 1);
        const auto cp = (a + b) * 0.5f;   // Bézier control point = midpoint of a and b
        p.quadraticTo(a, cp);
    }
    p.lineTo(pointAt(79));
    return p;
}
```

(For fill, append `lineTo(w, h)` and `lineTo(0, h)` and `closeSubPath()`.)

### Steps

- [ ] **Step 1: Read `Source/WebUI/spectrum.js`** — particularly the bin smoothing logic, peak decay, Bézier curve construction in `_drawCurve()`, and the Split-mode clipping.

- [ ] **Step 2: Replace Spectrum.h** with a clean Component holding 80-bin smoothed input/output/engine-A/engine-B arrays + peak hold array.

- [ ] **Step 3: Replace Spectrum.cpp** — implement `paint()` with the layer order above; implement Bézier path builder; implement bin smoothing (`smoothed += (raw - smoothed) * (raw > smoothed ? 0.5 : 0.08)` per bin); implement peak decay (`peak = max(0, peak - 0.003)`).

- [ ] **Step 4: Wire spectrum-data flow** — currently `processor.spectrumData` and `processor.spectrumOutputData` are accessible from Spectrum.cpp. Read them on Timer tick (30 Hz) and update the smoothed arrays.

- [ ] **Step 5: Build and iterate** — confirm the spectrum renders as smooth curves with proper grid + crossover line.

- [ ] **Step 6: Commit**

```bash
git commit -m "feat(path-b): Spectrum full canvas port — Bezier curves + gradients + grid"
```

---

## Task 4: Oscilloscope rewrite — 3-layer trigger-aligned trace + gate threshold

**Files:**
- Replace: `Source/UI/visualizers/Oscilloscope.h` and `.cpp`

**Goal:** time-domain oscilloscope with 3 traces (input gray, synth amber-blue, output white), zero-crossing trigger alignment, optional gate threshold lines (RESYN mode), and a legend.

**JS reference:** `Source/WebUI/oscilloscope.js`.

**Visual layers:**

1. **Black background**

2. **Zero line** (horizontal at h*0.5): `rgba(255,255,255,0.06)`, width 1

3. **Time-grid lines** (7 vertical lines at i*h/8): `rgba(255,255,255,0.03)`, width 1

4. **Gate threshold lines** (only in RESYN mode if Gate > 0):
   - Two horizontal dashed lines above/below center at `mid ± gateThr * (h*0.38)`
   - Color: `rgba(80,142,215,0.50)`, width 1, dash `[4, 4]`

5. **Zero-crossing markers** (vertical lines at first positive-slope zero-crossing):
   - Valid (spaced > minPeriod): `rgba(80,142,215,0.30)`, width 1
   - Invalid: `rgba(255,100,60,0.14)`, width 1, dashed `[2, 4]`

6. **Input waveform** (back layer): `rgba(160,160,175,0.28)`, width 1, no glow

7. **Synth waveform** (middle layer): `rgba(80,142,215,0.82)`, width 1.5, glow blur 5

8. **Output waveform** (front layer): `rgba(255,255,255,0.62)`, width 1, no glow

9. **Auto-scale** (toggle): find peak across visible channels (capped at 8×); apply `normScale = min(1/peak, 8)` to all channels.

10. **Legend** (top-left): "IN" / "SYNTH" / "OUT" labels in respective colors at appropriate alphas.

### Steps

- [ ] **Step 1: Read `Source/WebUI/oscilloscope.js`**.
- [ ] **Step 2: Replace Oscilloscope.h** to hold 3 ring buffers + write positions + auto-scale flag.
- [ ] **Step 3: Replace Oscilloscope.cpp** with layer-by-layer paint.
- [ ] **Step 4: Trigger logic** — find first positive-slope zero-crossing in older half of buffer; align display window to start there.
- [ ] **Step 5: 30 Hz Timer** repaints; data ingestion already wired via existing `processor.oscSynthBuf` etc.
- [ ] **Step 6: Build, iterate, commit**.

```bash
git commit -m "feat(path-b): Oscilloscope full canvas port — 3-layer trigger trace + gate"
```

---

## Task 5: CircuitBoard — animated background for Advanced panel

**Files:**
- Create: `Source/UI/widgets/CircuitBoard.h` and `.cpp`
- Modify: `Source/UI/panels/RightPanel.cpp` (or wherever the Advanced panel lives) to host the CircuitBoard as a child component when expanded
- Modify: `CMakeLists.txt`

**Goal:** the Advanced panel's expanded state shows an animated circuit-board background — static traces with periodic traveling pulses and joint flashes.

**JS reference:** `Source/WebUI/circuit-board.js`.

**Implementation notes:**

- Define 8 trace polylines (normalized 0-100% canvas coords; scale at resize): copy directly from `circuit-board.js`'s `buildTraces()`.
- Define joint set (deduped trace endpoints).
- On Timer (60 Hz): advance pulses via `t += speed*dt`, position via piecewise-linear interpolation along trace.
- On pulse completion (t ≥ 1): find nearest joint, set `flashUntil = now + 300 ms`.
- Spawn pulses at random 800 + rand(0, 700) ms intervals; max 5 in flight.
- Visibility: pause Timer when panel is collapsed.

This is the simplest port. All geometry is straightforward; no complex gradients.

### Steps

- [ ] **Step 1: Read `Source/WebUI/circuit-board.js`** — note exact trace points, pulse spawn logic, joint flash timing.
- [ ] **Step 2: Implement CircuitBoard.h/.cpp**.
- [ ] **Step 3: Embed in Advanced panel** — only visible/timer-running when Advanced is expanded.
- [ ] **Step 4: Build, iterate, commit**.

```bash
git commit -m "feat(path-b): CircuitBoard animated background for Advanced panel"
```

---

## Task 6: Polish — slot dots, button widths, layout audit

**Files:**
- Modify: `Source/UI/widgets/ModSlot.cpp` — slot dot final styling per CSS
- Modify: button bounds throughout (segmented controls were clipping labels: "-12 dB/oc", "Phantom Onl") — increase widths in `LeftPanel`/`RightPanel`/`ModulationPanel` `resized()` methods
- Modify: anything else discovered during smoke

**Goal:** fix the small visible issues left from Phase 5.5 — button label clipping, slot dot details, any remaining spacing problems.

### Steps

- [ ] **Step 1: Audit button widths** — check `juce::Font::getStringWidth(label)` against allotted bounds for every TextButton; widen where labels are clipped.
- [ ] **Step 2: Slot dot final styling** — verify ring + glow + active-state animations match WebView2.
- [ ] **Step 3: Layout audit** — open in Live, walk through every panel, note any remaining spacing/alignment issues.
- [ ] **Step 4: Bundle fixes into one commit**.

```bash
git commit -m "feat(path-b): Phase 5.6 polish — button widths, slot dots, layout audit"
```

---

## Task 7: Final smoke + side-by-side review

**Files:** none modified — verification only.

- [ ] **Step 1: Open WebView2 reference + native side-by-side in Live** (two tracks).
- [ ] **Step 2: Walk through every visible widget**: knobs (saturation, shape, skip, width, in, out, advanced row), recipe wheel, ghost section, filter section, harmonic engine, stereo, levels, oscilloscope, spectrum, slot row, matrix view, advanced panel.
- [ ] **Step 3: Document any remaining gaps** — note in a follow-up commit message or push as additional polish iterations.

If ALL widgets visually match: Phase 5.6 done. Phase 6 (cutover — delete WebView2) becomes appropriate.

If 1-2 widgets are still visibly off: do a final small iteration commit.

If many widgets are still off: STOP and reassess scope; possibly fall back to Option 1 (stay on WebView2).

---

## Self-review notes

### Spec coverage

Phase 5.6 ports each canvas-rendered widget faithfully:
- ✓ Knob: neumorphic body + OLED + arc + value text — Task 1
- ✓ Recipe wheel: rings + spokes + particles + scan + center glow — Task 2
- ✓ Spectrum: Bézier + gradients + grid + Split/Combined — Task 3
- ✓ Oscilloscope: 3-layer trigger trace + gate — Task 4
- ✓ Circuit-board: animated background — Task 5
- ✓ Polish: button widths, slot dots — Task 6
- ✓ Smoke: side-by-side review — Task 7

### Risk areas

- **Task 1 (knob)** — every knob in the editor changes simultaneously. If something is wrong, the whole UI looks worse. Iterate carefully.
- **Task 2 (recipe wheel)** — most complex single port; 7 layers + 30 Hz animation + 140 particles. Easy to get wrong.
- **Task 3 (spectrum)** — Bézier path math must be exactly right. Off-by-one in bin index → visible kinks.
- **Task 5 (circuit-board)** — only relevant when Advanced is expanded; lower visual impact.
- **Drift between phases** — Phase 5.5's Theme tokens may not perfectly match the JS canvas's hardcoded RGBA values. Where they conflict, prefer the JS values (they're the source of truth for visual fidelity).

### Effort estimate

| Task | Time |
|------|------|
| 1 — Knob port | 1 day |
| 2 — Recipe wheel | 1.5 days |
| 3 — Spectrum | 1 day |
| 4 — Oscilloscope | 0.5 day |
| 5 — Circuit-board | 0.5 day |
| 6 — Polish | 0.5 day |
| 7 — Final smoke | 0 (review only) |
| | **~5 days** |

### What ships at the end

A native editor that, opened next to WebView2 in Live, is visually indistinguishable. Knobs have integrated OLED value displays. The recipe wheel feels like an instrument. The spectrum looks like the WebView2's curved frequency response, not a bar graph. The oscilloscope shows three layered traces with proper triggering. The Advanced panel has its animated background. Phase 6 (cutover) becomes appropriate after Phase 5.6 lands.
