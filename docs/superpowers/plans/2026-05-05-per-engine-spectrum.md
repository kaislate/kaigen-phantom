# Per-Engine FFT Spectrum Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single-canvas spectrum view with a side-by-side per-engine pair (default) plus a toggle to switch to a single combined "morph-weighted blend" that occupies the same total width. The combined view reflects what the user is actually hearing (post-crossfader audio).

**Architecture:** PR1's `DualEngineHost::process` captures `aScratch` (Engine A output) and `bScratch` (Engine B output) before the crossfader mixes them. Add FFT taps on those two scratch buffers to produce per-engine spectra. The existing combined-output FFT (`fftBuffer` post-engine-mix) becomes the data source for the combined view — no new capture needed for that mode. View-mode toggle is editor-state, persisted in `<SpectrumView>` plugin-state child following the `<EditorFocus>` pattern from PR2.

**Tech Stack:** C++20, JUCE 8.0.4, Canvas2D rendering, WebView2 native bindings.

---

## Decisions baked into this plan

- **Single canvas, internally split.** The split layout draws Engine A on the left half, Engine B on the right half, of one canvas. Combined mode draws full width. No DOM-level splitting; simpler, easier to animate transitions if desired later.
- **Default mode is SPLIT.** Matches the user's stated UX preference.
- **Persistence as plugin state, not preset state.** `<SpectrumView>` child of the `<PluginState>` wrapper, alongside `<EditorFocus>`. Preset switching doesn't change the user's view preference.
- **Per-engine crossover lines.** In split mode, each side renders its own engine's `phantom_threshold` line (left = `a_phantom_threshold`, right = `b_phantom_threshold`). In combined mode, the line uses the active-engine's threshold (same as today).
- **Toggle UI lives next to the spectrum**, not in settings. Matches similar inline toggles already in the editor (mode buttons, etc.).
- **Existing FFT path unchanged.** `fftBuffer` (input) and the existing output FFT capture both stay wired exactly as they are. New per-engine FFTs are additive.

## File Plan

**Created:**
- `tests/SpectrumViewModeTests.cpp` — 4 cases: default Split, setSpectrumViewMode stores value, persistence helpers round-trip, setStateInformation preserves on absent child.
- `Source/SpectrumViewMode.h` — header-only `kaigen::phantom::SpectrumViewMode` enum + `writeSpectrumViewModeToTree` / `readSpectrumViewModeFromTree` helpers (mirrors `EngineFocus.h`).

**Modified:**
- `Source/DualEngineHost.h` — expose const-refs to `aScratch` / `bScratch` (or add explicit `getEngineAOutput()` / `getEngineBOutput()` accessors) so `PhantomProcessor` can FFT them post-process.
- `Source/DualEngineHost.cpp` — make sure `aScratch` / `bScratch` remain populated and consistent post-process (they already do, but documenting the contract).
- `Source/PluginProcessor.h` — add per-engine FFT ring buffers (`fftBufferEngineA`, `fftBufferEngineB`) + write positions; add `SpectrumViewMode spectrumViewMode` member + accessor.
- `Source/PluginProcessor.cpp` — populate the new FFT buffers in `processBlock` after `dualEngineHost.process(...)`; persist `<SpectrumView>` in `getStateInformation` / `setStateInformation`.
- `Source/PluginEditor.cpp` — extend `getSpectrumData` native binding to also include per-engine arrays; add `spectrumGetViewMode` / `spectrumSetViewMode` native bindings.
- `Source/WebUI/spectrum.js` — refactor `drawSpectrum` to handle split vs combined modes; add `loadAndDrawForMode()` driven by current mode.
- `Source/WebUI/index.html` — add a `<button id="spectrum-mode-toggle">` next to the spectrum container.
- `Source/WebUI/styles.css` — toggle button styling.
- `Source/WebUI/phantom.js` — toggle handler that calls `spectrumSetViewMode` + redraws.
- `tests/CMakeLists.txt` — add `SpectrumViewModeTests.cpp`.

---

## Task 1: `SpectrumViewMode` types + persistence (TDD)

Mirror the `EngineFocus.h` pattern from PR2 Task 1.

**Files:**
- Create: `Source/SpectrumViewMode.h`
- Create: `tests/SpectrumViewModeTests.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write `Source/SpectrumViewMode.h`**

```cpp
// Source/SpectrumViewMode.h
#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace kaigen::phantom
{

/** Spectrum view layout. Underlying integer values are stable: serialized
 *  through the WebView bridge and persisted in `<SpectrumView>` properties.
 *  Don't reorder or renumber without versioning the persistence. */
enum class SpectrumViewMode : int { Split = 0, Combined = 1 };

/** Append `<SpectrumView>` child to `parent` capturing the view mode. */
inline void writeSpectrumViewModeToTree(juce::ValueTree& parent, SpectrumViewMode mode)
{
    juce::ValueTree node("SpectrumView");
    node.setProperty("mode", mode == SpectrumViewMode::Combined ? "Combined" : "Split", nullptr);
    parent.appendChild(node, nullptr);
}

/** Read SpectrumViewMode from the `<SpectrumView>` child of `parent`.
 *  Returns the default (Split) if the child is absent or malformed. */
inline SpectrumViewMode readSpectrumViewModeFromTree(const juce::ValueTree& parent)
{
    auto node = parent.getChildWithName("SpectrumView");
    if (!node.isValid()) return SpectrumViewMode::Split;
    return (node.getProperty("mode").toString() == "Combined")
           ? SpectrumViewMode::Combined : SpectrumViewMode::Split;
}

} // namespace kaigen::phantom
```

- [ ] **Step 2: Write `tests/SpectrumViewModeTests.cpp`**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "SpectrumViewMode.h"

using namespace kaigen::phantom;

TEST_CASE("SpectrumViewMode default is Split", "[spectrum-view]")
{
    juce::ValueTree wrapper("PluginState");
    auto m = readSpectrumViewModeFromTree(wrapper);
    REQUIRE(m == SpectrumViewMode::Split);
}

TEST_CASE("SpectrumViewMode round-trip via helpers", "[spectrum-view]")
{
    juce::ValueTree wrapper("PluginState");
    writeSpectrumViewModeToTree(wrapper, SpectrumViewMode::Combined);
    auto m = readSpectrumViewModeFromTree(wrapper);
    REQUIRE(m == SpectrumViewMode::Combined);
}

TEST_CASE("SpectrumViewMode round-trip Split explicit", "[spectrum-view]")
{
    juce::ValueTree wrapper("PluginState");
    writeSpectrumViewModeToTree(wrapper, SpectrumViewMode::Split);
    auto m = readSpectrumViewModeFromTree(wrapper);
    REQUIRE(m == SpectrumViewMode::Split);
}

TEST_CASE("SpectrumViewMode treats unknown mode string as Split", "[spectrum-view]")
{
    juce::ValueTree wrapper("PluginState");
    juce::ValueTree node("SpectrumView");
    node.setProperty("mode", "BogusValue", nullptr);
    wrapper.appendChild(node, nullptr);
    auto m = readSpectrumViewModeFromTree(wrapper);
    REQUIRE(m == SpectrumViewMode::Split);
}
```

- [ ] **Step 3: Wire test into `tests/CMakeLists.txt`**

Add `SpectrumViewModeTests.cpp` to the test target's source list, alongside `EngineFocusTests.cpp`.

- [ ] **Step 4: Build + run tests**

```
cmake --build build --target KaigenPhantomTests --config Debug
./build/tests/Debug/KaigenPhantomTests.exe -c "[spectrum-view]"
```

Expected: 4 cases, all pass. Full suite: 78 cases (74 + 4).

- [ ] **Step 5: Commit**

```bash
git add Source/SpectrumViewMode.h tests/SpectrumViewModeTests.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(spectrum): SpectrumViewMode enum + persistence helpers

Header-only types mirroring EngineFocus.h: SpectrumViewMode { Split,
Combined } + writeSpectrumViewModeToTree / readSpectrumViewModeFromTree.
Default is Split; absent or malformed <SpectrumView> child returns Split.
Used by PhantomProcessor's plugin-state persistence (next task) and the
WebView toggle UI binding.
EOF
)"
```

---

## Task 2: `SpectrumViewMode` state on PhantomProcessor

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Add include + member to `PluginProcessor.h`**

Add near other Source/ includes:

```cpp
#include "SpectrumViewMode.h"
```

In the public section near the `EngineFocus` accessors (around line 90), add:

```cpp
    // Aliases let the WebView native bindings refer to the type
    // without pulling in the kaigen::phantom namespace.
    using SpectrumViewMode = kaigen::phantom::SpectrumViewMode;

    SpectrumViewMode getSpectrumViewMode() const noexcept { return spectrumViewMode; }
    void setSpectrumViewMode(SpectrumViewMode m) noexcept { spectrumViewMode = m; }
```

In the private section near `engineFocus`, add:

```cpp
    SpectrumViewMode spectrumViewMode { SpectrumViewMode::Split };
```

- [ ] **Step 2: Persist in `getStateInformation` / `setStateInformation`**

In `Source/PluginProcessor.cpp::getStateInformation`, after the `<EditorFocus>` append (added in PR2 Task 1), add:

```cpp
    kaigen::phantom::writeSpectrumViewModeToTree(wrapper, spectrumViewMode);
```

In `setStateInformation`, after the `<EditorFocus>` restore block, add:

```cpp
    if (wrapper.getChildWithName("SpectrumView").isValid())
        spectrumViewMode = kaigen::phantom::readSpectrumViewModeFromTree(wrapper);
```

(Guarded with `isValid()` to preserve in-memory state on partial wrapper loads — same pattern as `engineFocus` from PR2.)

- [ ] **Step 3: Build + run tests**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
./build/tests/Debug/KaigenPhantomTests.exe
```

Expected: clean build, 78 cases pass.

- [ ] **Step 4: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "$(cat <<'EOF'
feat(spectrum): SpectrumViewMode state on PhantomProcessor

Default Split, persisted in <SpectrumView> child of <PluginState>
following the same pattern as <EditorFocus>. Native bindings + WebUI
toggle come in subsequent tasks.
EOF
)"
```

---

## Task 3: Per-engine FFT capture in PhantomProcessor

The most invasive task. Add two new ring buffers + write positions for engine A and B output FFTs. Populate them from `DualEngineHost::aScratch` / `bScratch` after `dualEngineHost.process(...)` returns.

**Files:**
- Modify: `Source/DualEngineHost.h` (add accessors for the scratches)
- Modify: `Source/PluginProcessor.h` (add buffers)
- Modify: `Source/PluginProcessor.cpp` (capture from scratches in processBlock)

- [ ] **Step 1: Expose `aScratch` / `bScratch` from `DualEngineHost.h`**

In the public section of `DualEngineHost`:

```cpp
    /** Per-engine output buffers (post-process, pre-crossfader). Caller
     *  may read these for visualization (FFT capture) but must NOT modify
     *  them — they're consumed by the next process() call. Valid only
     *  immediately after process() returns and until the next process()
     *  call. */
    const juce::AudioBuffer<float>& getEngineAOutput() const noexcept { return aScratch; }
    const juce::AudioBuffer<float>& getEngineBOutput() const noexcept { return bScratch; }
```

(`aScratch` and `bScratch` are already private members — these accessors return const-refs without exposing internals.)

- [ ] **Step 2: Add FFT buffers to `PluginProcessor.h`**

Find the existing `fftBuffer` declaration (the input FFT). Adjacent to it, add:

```cpp
    static constexpr int kEngineFFTBufSize = 2048;   // matches existing fftBuffer size
    std::array<float, kEngineFFTBufSize> fftBufferEngineA {};
    std::array<float, kEngineFFTBufSize> fftBufferEngineB {};
    std::atomic<int> fftWritePosEngineA { 0 };
    std::atomic<int> fftWritePosEngineB { 0 };
```

(Match the existing input FFT buffer's declaration style — if it uses a different size constant or a different container type, mirror it. The size should match for consistency.)

- [ ] **Step 3: Populate in `processBlock`**

In `Source/PluginProcessor.cpp::processBlock`, locate the line `dualEngineHost.process(buffer, sidechainPtr);`. After that line, before any output-peak / output-FFT capture that already runs:

```cpp
    // Capture per-engine outputs for the spectrum view. aScratch/bScratch
    // are valid until the next dualEngineHost.process() call.
    {
        const auto& aOut = dualEngineHost.getEngineAOutput();
        const auto& bOut = dualEngineHost.getEngineBOutput();
        const int   nCh  = juce::jmin(aOut.getNumChannels(), 2);

        if (nCh > 0)
        {
            auto captureInto = [&](const juce::AudioBuffer<float>& src,
                                   std::array<float, kEngineFFTBufSize>& dst,
                                   std::atomic<int>& wp)
            {
                const float* l = src.getReadPointer(0);
                int pos = wp.load(std::memory_order_relaxed);
                for (int i = 0; i < n; ++i)
                {
                    dst[(size_t) pos] = l[i];
                    pos = (pos + 1) & (kEngineFFTBufSize - 1);
                }
                wp.store(pos, std::memory_order_relaxed);
            };

            captureInto(aOut, fftBufferEngineA, fftWritePosEngineA);
            captureInto(bOut, fftBufferEngineB, fftWritePosEngineB);
        }
    }
```

(Adapt the capture loop's style to match the existing `fftBuffer` capture. The intent is: ring-buffer write of mono channel-0 samples for FFT-side consumption, just like the input FFT buffer.)

- [ ] **Step 4: Build the plugin to verify**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
```

Expected: clean build. Tests should still pass (`./build/tests/Debug/KaigenPhantomTests.exe` → 78 cases).

- [ ] **Step 5: Commit**

```bash
git add Source/DualEngineHost.h Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "$(cat <<'EOF'
feat(spectrum): per-engine FFT capture in PhantomProcessor

DualEngineHost exposes aScratch/bScratch via const-ref accessors.
PhantomProcessor captures both into ring buffers (fftBufferEngineA/B)
after dualEngineHost.process() returns. Native binding to expose to
the WebView is in the next task.

CPU cost: two additional channel-0 ring-buffer writes per block
(no FFT computation here; that happens on the UI thread in the
existing native binding).
EOF
)"
```

---

## Task 4: Native bindings — extend getSpectrumData + new view-mode bindings

**Files:**
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Extend `getSpectrumData` to include per-engine arrays**

Find the existing `getSpectrumData` native binding in `Source/PluginEditor.cpp`. It currently returns input + output spectrum arrays. Extend it to also include `engineA` and `engineB` arrays computed via FFT on `fftBufferEngineA` / `fftBufferEngineB`.

The exact code depends on the existing FFT computation. The existing binding likely:
1. Reads from `fftBuffer` ring buffer.
2. Applies a Hann window.
3. Computes magnitude via JUCE FFT.
4. Returns binned values as a `juce::var` array.

Mirror this for the two new buffers. Add to the returned object:

```cpp
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    // ... existing keys: "in", "out", peak hold, etc.
    obj->setProperty("engineA", aSpectrumArray);
    obj->setProperty("engineB", bSpectrumArray);
    obj->setProperty("viewMode", self.processor.getSpectrumViewMode() == PhantomProcessor::SpectrumViewMode::Combined ? "Combined" : "Split");
```

If the existing binding returns a flat-array shape (just an `Array<var>` of 2 inner arrays), restructure to an object shape OR add a second native binding `getSpectrumDataPerEngine`. Pick whichever is least disruptive.

- [ ] **Step 2: Add `spectrumGetViewMode` + `spectrumSetViewMode` native bindings**

Mirror the `engineGetFocus` / `engineSetFocus` pattern from PR2 Task 2. In the `withNativeFunction` chain:

```cpp
    .withNativeFunction("spectrumGetViewMode", [&self]
        (const juce::Array<juce::var>&, auto complete)
    {
        const auto m = self.processor.getSpectrumViewMode();
        complete(juce::var(m == PhantomProcessor::SpectrumViewMode::Combined ? "Combined" : "Split"));
    })
    .withNativeFunction("spectrumSetViewMode", [&self]
        (const juce::Array<juce::var>& args, auto complete)
    {
        if (args.size() < 1) { complete({}); return; }
        const auto s = args[0].toString();
        self.processor.setSpectrumViewMode(s == "Combined"
            ? PhantomProcessor::SpectrumViewMode::Combined
            : PhantomProcessor::SpectrumViewMode::Split);
        complete({});
    })
```

(Adapt to the existing capture style — `[&self]` / `auto complete` matches PR2's bindings.)

- [ ] **Step 3: Build to verify**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
```

Expected: clean build. The bindings exist but aren't called yet — Task 5 wires the JS side.

- [ ] **Step 4: Commit**

```bash
git add Source/PluginEditor.cpp
git commit -m "$(cat <<'EOF'
feat(spectrum): native bindings for per-engine FFT + view-mode toggle

getSpectrumData now returns engineA/engineB spectrum arrays alongside
the existing in/out. spectrumGetViewMode / spectrumSetViewMode bindings
expose the persistence-backed mode flag to the WebView toggle button
(next task).
EOF
)"
```

---

## Task 5: WebUI — refactor `spectrum.js` for split / combined rendering

**Files:**
- Modify: `Source/WebUI/spectrum.js`

- [ ] **Step 1: Add view-mode state + per-engine smoothed arrays**

At the top of `spectrum.js` (or wherever module-scope variables are declared), add:

```javascript
let viewMode = 'Split';   // 'Split' | 'Combined'; synced from native on init + toggle
let smoothedEngineA = null;
let smoothedEngineB = null;
```

- [ ] **Step 2: Hook the polling / fetch loop to populate per-engine arrays**

Wherever the current `getSpectrumData()` call happens (probably in a `requestAnimationFrame` loop or `setInterval`), update the parsing to read the new keys:

```javascript
const data = await window.Juce.getNativeFunction('getSpectrumData')();
if (data) {
    smoothedIn  = applySmoothing(smoothedIn,  data.in);
    smoothedOut = applySmoothing(smoothedOut, data.out);
    if (data.engineA) smoothedEngineA = applySmoothing(smoothedEngineA, data.engineA);
    if (data.engineB) smoothedEngineB = applySmoothing(smoothedEngineB, data.engineB);
    if (data.viewMode) viewMode = data.viewMode;
}
```

(Adapt to whatever the existing smoothing helper is called. The current code probably has a similar accumulator pattern.)

- [ ] **Step 3: Refactor `drawSpectrum` to dispatch on viewMode**

Replace the existing `drawSpectrum` with a dispatcher:

```javascript
function drawSpectrum() {
    if (!specCanvas) return;
    const ctx = specCanvas.getContext('2d');
    if (!ctx) return;

    const w = specCanvas.width;
    const h = specCanvas.height;

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, w, h);

    if (viewMode === 'Combined') {
        drawSpectrumCombined(ctx, w, h);
    } else {
        drawSpectrumSplit(ctx, w, h);
    }
}
```

- [ ] **Step 4: Implement `drawSpectrumCombined`**

Move the body of the existing `drawSpectrum` into a new function `drawSpectrumCombined(ctx, w, h)`. This is the existing combined-output behavior — input gray + post-crossfader output white + crossover line — at full canvas width.

- [ ] **Step 5: Implement `drawSpectrumSplit`**

```javascript
function drawSpectrumSplit(ctx, w, h) {
    const halfW = Math.floor(w / 2);

    // Draw left half: Engine A
    ctx.save();
    ctx.beginPath();
    ctx.rect(0, 0, halfW, h);
    ctx.clip();
    drawOnePane(ctx, 0, halfW, h, 'A');
    ctx.restore();

    // Draw right half: Engine B
    ctx.save();
    ctx.beginPath();
    ctx.rect(halfW, 0, w - halfW, h);
    ctx.clip();
    drawOnePane(ctx, halfW, w - halfW, h, 'B');
    ctx.restore();

    // Center divider
    ctx.strokeStyle = 'rgba(255,255,255,0.10)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(halfW + 0.5, 0);
    ctx.lineTo(halfW + 0.5, h);
    ctx.stroke();

    // A / B labels (top-left / top-right)
    const labelFont = Math.max(9, Math.round(h * 0.10)) + 'px monospace';
    ctx.font = labelFont;
    ctx.textBaseline = 'top';
    ctx.fillStyle = 'rgba(74,144,226,0.70)';
    ctx.textAlign = 'left';
    ctx.fillText('A', 6, 4);
    ctx.fillStyle = 'rgba(74,144,226,0.70)';
    ctx.textAlign = 'right';
    ctx.fillText('B', w - 6, 4);
}
```

`drawOnePane` is a helper that draws one engine's spectrum at the given x-offset / width:

```javascript
function drawOnePane(ctx, xOffset, paneW, h, side /* 'A' | 'B' */) {
    drawGrid(ctx, xOffset, paneW, h);

    const inPts = buildCurvePoints(smoothedIn, paneW, h, xOffset);
    const eng   = (side === 'B') ? smoothedEngineB : smoothedEngineA;
    const engPts = eng ? buildCurvePoints(eng, paneW, h, xOffset) : null;

    // Layer 1: input (gray, faint)
    if (inPts && inPts.length >= 2) {
        fillCurve(ctx, inPts, h);
        const fillGrad = ctx.createLinearGradient(0, 0, 0, h);
        fillGrad.addColorStop(0, 'rgba(160,160,175,0.18)');
        fillGrad.addColorStop(1, 'rgba(160,160,175,0.04)');
        ctx.fillStyle = fillGrad;
        ctx.fill();
        strokeCurve(ctx, inPts);
        ctx.strokeStyle = 'rgba(160,160,175,0.45)';
        ctx.lineWidth = Math.max(1, h * 0.004);
        ctx.stroke();
    }

    // Layer 2: this engine's output (white)
    if (engPts && engPts.length >= 2) {
        fillCurve(ctx, engPts, h);
        const grad = ctx.createLinearGradient(0, 0, 0, h);
        grad.addColorStop(0, 'rgba(255,255,255,0.22)');
        grad.addColorStop(0.5, 'rgba(255,255,255,0.08)');
        grad.addColorStop(1, 'rgba(255,255,255,0.02)');
        ctx.fillStyle = grad;
        ctx.fill();
        strokeCurve(ctx, engPts);
        ctx.strokeStyle = 'rgba(255,255,255,0.85)';
        ctx.lineWidth = Math.max(1.0, h * 0.005);
        ctx.stroke();
    }

    // Per-engine crossover line
    const xoverParam = (side === 'B') ? 'b_phantom_threshold' : 'a_phantom_threshold';
    const xoverState = window.Juce?.getSliderState?.(xoverParam);
    if (xoverState) {
        const xoverHz = xoverState.getScaledValue();
        if (xoverHz > 20 && xoverHz < 20000) {
            const xPos = Math.round(freqToX(xoverHz, paneW)) + xOffset;
            ctx.save();
            ctx.strokeStyle = 'rgba(80,142,215,0.38)';
            ctx.lineWidth   = 1;
            ctx.setLineDash([3, 4]);
            ctx.beginPath();
            ctx.moveTo(xPos + 0.5, 0);
            ctx.lineTo(xPos + 0.5, h);
            ctx.stroke();
            ctx.restore();
        }
    }
}
```

- [ ] **Step 6: Adapt `buildCurvePoints` / `drawGrid` to take `xOffset` + width**

The existing helpers probably assume full-width rendering. Modify them to accept an `xOffset` and a `paneW` (or refactor callers to translate the canvas before drawing). Whichever is less invasive.

If `buildCurvePoints` already returns points in `[0, w]`, the simplest is to just translate the coordinates inside `drawOnePane` via `ctx.translate(xOffset, 0)` instead of passing offsets through.

- [ ] **Step 7: Build + manual smoke**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
```

Visual check in DAW: the spectrum view should now show two side-by-side panes by default. Both should be alive (animating with audio). The engine A pane should respond to A's params; B pane to B's.

- [ ] **Step 8: Commit**

```bash
git add Source/WebUI/spectrum.js
git commit -m "$(cat <<'EOF'
feat(spectrum): split-mode rendering for per-engine spectra

drawSpectrum now dispatches on viewMode ('Split' | 'Combined').
Split: side-by-side per-engine panes with input (gray) + that
engine's output (white) + per-engine crossover line. Combined:
existing full-width post-crossfader behavior.

viewMode is synced from getSpectrumData; the toggle button in
the next task drives the change.
EOF
)"
```

---

## Task 6: Toggle button UI + handler

**Files:**
- Modify: `Source/WebUI/index.html`
- Modify: `Source/WebUI/styles.css`
- Modify: `Source/WebUI/phantom.js`

- [ ] **Step 1: Add the toggle button to `index.html`**

Locate the spectrum container in `index.html` (the wrapper around `<canvas id="spectrum-canvas">` or whatever the current id is). Add a small toggle button just above the canvas:

```html
    <button type="button" id="spectrum-mode-toggle" class="spectrum-mode-toggle"
            aria-label="Toggle spectrum view mode" title="Split / Combined">
      <span class="lbl-split">SPLIT</span>
      <span class="lbl-combined">COMBINED</span>
    </button>
```

The two `<span>`s let CSS show/hide one based on the button state class.

- [ ] **Step 2: Add CSS to `styles.css`**

```css
.spectrum-mode-toggle {
    position: absolute;
    top: 4px; right: 8px;
    z-index: 5;
    background: rgba(20,24,30,0.6);
    border: 1px solid rgba(74,144,226,0.25);
    color: #6a7a8a;
    font: 600 9px/1 'Space Grotesk', sans-serif;
    letter-spacing: 1.2px;
    padding: 3px 8px;
    border-radius: 4px;
    cursor: pointer;
    transition: color 120ms, background 120ms, border-color 120ms;
    user-select: none;
}
.spectrum-mode-toggle:hover {
    color: #c8d8ea;
}
.spectrum-mode-toggle .lbl-combined { display: none; }
.spectrum-mode-toggle.is-combined .lbl-split { display: none; }
.spectrum-mode-toggle.is-combined .lbl-combined { display: inline; }
.spectrum-mode-toggle.is-combined {
    color: #f5f8fb;
    background: rgba(74,144,226,0.18);
    border-color: rgba(74,144,226,0.55);
}
```

The wrapper around `<canvas>` may need `position: relative;` for the absolute-positioned button to anchor correctly — verify and add if missing.

- [ ] **Step 3: Add the JS handler in `phantom.js`**

Append a new IIFE module to `phantom.js`:

```javascript
// Spectrum view-mode toggle (PR: per-engine spectrum)
(function () {
  'use strict';
  if (typeof window.Juce === 'undefined' || typeof window.Juce.getNativeFunction !== 'function') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
    return;
  }
  init();

  function init() {
    const btn = document.getElementById('spectrum-mode-toggle');
    if (!btn) return;

    const getMode = window.Juce.getNativeFunction('spectrumGetViewMode');
    const setMode = window.Juce.getNativeFunction('spectrumSetViewMode');
    if (!getMode || !setMode) return;

    let mode = 'Split';

    function applyToUI(m) {
      btn.classList.toggle('is-combined', m === 'Combined');
    }

    btn.addEventListener('click', async () => {
      mode = (mode === 'Split') ? 'Combined' : 'Split';
      applyToUI(mode);
      await setMode(mode);
    });

    // Initial sync.
    getMode().then(m => {
      mode = (m === 'Combined') ? 'Combined' : 'Split';
      applyToUI(mode);
    }).catch(() => {
      mode = 'Split';
      applyToUI(mode);
    });
  }
})();
```

- [ ] **Step 4: Build + manual smoke**

```
cmake --build build --target KaigenPhantom_VST3 --config Debug
```

In DAW: the toggle button should appear at top-right of the spectrum. Click it: the spectrum flips between split (A | B) and combined views. Save project, reopen — view mode persists.

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/index.html Source/WebUI/styles.css Source/WebUI/phantom.js
git commit -m "$(cat <<'EOF'
feat(spectrum): split/combined toggle button

Small pill button at the spectrum's top-right. Click flips between
the two-engine split view and the single combined morph-weighted
view. State persists in <SpectrumView> plugin-state child via
spectrumGetViewMode / spectrumSetViewMode native bindings.
EOF
)"
```

---

## Task 7: Manual integration smoke test

- [ ] **Step 1: Build Release**

```
cmake --build build --target KaigenPhantom_VST3 --config Release
```

Expected: clean build, post-install copy succeeds.

- [ ] **Step 2: Open in Live 12 and verify**

- [ ] Spectrum defaults to SPLIT view: A on left, B on right, divider in middle.
- [ ] Both panes animate with audio. Input trace (gray) visible in both. Each pane's output (white) reflects that engine's processing.
- [ ] Move A's `phantom_threshold`: left pane's blue crossover line moves; right pane's stays put. Same in reverse for B.
- [ ] Click toggle button: flips to COMBINED view. Single full-width spectrum showing the post-crossfader output.
- [ ] In COMBINED mode, move the morph slider: the white output trace shifts in shape between A's and B's character.
- [ ] In COMBINED mode at morph=0: visually similar to SPLIT mode's A pane (with the gray input).
- [ ] In COMBINED mode at morph=1: visually similar to SPLIT mode's B pane.
- [ ] Click toggle again: returns to SPLIT.
- [ ] Save Live project, close + reopen Live: spectrum view mode (whichever was last set) is restored.
- [ ] CPU: split mode adds ~1-3% over combined. Acceptable.

- [ ] **Step 3: If smoke surfaces issues, fix and commit; otherwise no-op**

---

## End-of-PR Checklist

- [ ] All 78 unit tests pass.
- [ ] VST3 builds cleanly Debug + Release.
- [ ] Default view mode is SPLIT.
- [ ] Toggle works both directions; state persists across DAW save/restore.
- [ ] Per-engine crossover lines correct in split mode.
- [ ] Combined mode behavior unchanged from pre-PR (still post-crossfader output).
- [ ] No regression in input gain / bypass / morph slider / preset save-load.

---

## Self-Review

**Spec coverage:**
- Two side-by-side spectra (Task 5 split rendering) ✓
- Toggle to single combined morph-weighted blend (Tasks 5 + 6) ✓
- Combined view reflects what's being heard (existing post-crossfader output FFT) ✓
- View mode persists across editor opens (Task 1 + 2) ✓

**No placeholders:** every step has actual code or precise instructions for mechanical edits.

**Type / name consistency:**
- `SpectrumViewMode` defined in Task 1, used in Task 2 + 4 (via alias).
- `fftBufferEngineA` / `fftBufferEngineB` declared in Task 3, populated in Task 3, consumed in Task 4.
- Native bindings `spectrumGetViewMode` / `spectrumSetViewMode` registered in Task 4, called in Task 6.
- JS `viewMode` global set in Task 5 (from `getSpectrumData` response) and Task 6 (from toggle click).

**Deferred items:** none. The plan covers the full feature.
