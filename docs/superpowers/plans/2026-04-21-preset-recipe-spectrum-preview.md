# Preset Recipe Spectrum Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render a monochrome spectrum curve inline at the top of the preset browser's preview card, showing the preset's H2–H8 harmonic shape plus a dashed crossover marker.

**Architecture:** `PresetManager` extracts the seven recipe weights and crossover Hz from each preset's APVTS state during its disk scan and caches them on `PresetInfo::preview`. The `getAllPresets` native function includes a `preview` object in each JSON entry. A new `preset-spectrum.js` module renders the curve as inline SVG using the same log-frequency mapping the main analyzer uses. `preset-system.js:updatePreview` prepends an `<svg>` and calls the renderer.

**Tech Stack:** C++20 / JUCE 8 (`ValueTree` / `APVTS` / `DynamicObject` / `JSON`); Catch2 for C++ tests; vanilla JS + inline SVG (no build step); CMake 3.22+; WebView2 on Windows.

**Spec:** [2026-04-21-preset-recipe-spectrum-preview-design.md](../specs/2026-04-21-preset-recipe-spectrum-preview-design.md)

---

## File Structure

**Create:**
- `Source/WebUI/preset-spectrum.js` — Curve math + SVG rendering module (IIFE, exposes `window.PresetSpectrum.render`)
- `tests/PresetPreviewTests.cpp` — Unit tests for `PresetManager::readPreviewFromState`

**Modify:**
- `Source/PresetManager.h` — Add `PreviewData` struct; add `preview` field to `PresetInfo`; add static declaration for `readPreviewFromState`
- `Source/PresetManager.cpp` — Implement `readPreviewFromState`; call it during `scanPresetsFromDisk`
- `Source/PluginEditor.cpp` — Include `preview` in the JSON entries returned by the `getAllPresets` native function
- `Source/WebUI/preset-system.js` — In `updatePreview`, prepend an `<svg>` and call `window.PresetSpectrum.render`
- `Source/WebUI/index.html` — Add `<script src="/preset-spectrum.js">` tag before `preset-system.js`
- `CMakeLists.txt` — Add `Source/WebUI/preset-spectrum.js` to the BinaryData sources; add `tests/PresetPreviewTests.cpp` to the test target

Each file has a single, narrow responsibility: the C++ changes touch data extraction and serialization only; the JS module handles just the curve/SVG render; `preset-system.js` gets a two-line insertion.

---

### Task 1: Extend the PresetInfo data model

Adds the carrier struct for the preview data. No behavior change yet.

**Files:**
- Modify: `Source/PresetManager.h`

- [ ] **Step 1: Add `PreviewData` struct and `preview` field to `PresetInfo`**

In `Source/PresetManager.h`, immediately after the `PresetMetadata` struct definition (around line 21), add:

```cpp
// Parameter values extracted from a preset for the browser preview spectrum.
// Populated once at scan time so hovering a preset triggers no disk I/O.
struct PreviewData
{
    float h[7]      {};       // recipe_h2 .. recipe_h8, normalized 0..1
    float crossover = 120.0f; // phantom_threshold in Hz (matches APVTS default in Parameters.h)
};
```

Then update `PresetInfo` (around line 23) to carry it:

```cpp
struct PresetInfo
{
    PresetMetadata metadata;
    PreviewData    preview;
    juce::File     file;
};
```

Also add the new static declaration to the `private:` section near the existing `readMetadataFromFile` (around line 124):

```cpp
// Extract the preview parameter values from a preset's APVTS state tree.
// Missing recipe params default to 0; missing crossover defaults to 80 Hz.
static PreviewData readPreviewFromState(const juce::ValueTree& state);
```

- [ ] **Step 2: Verify the project still compiles**

Run: `cmake --build build --config RelWithDebInfo --target Phantom_VST3`

Expected: build succeeds. (Nothing reads `preview` yet, but the struct must compile.)

- [ ] **Step 3: Commit**

```bash
git add Source/PresetManager.h
git commit -m "feat(preset): add PreviewData struct on PresetInfo"
```

---

### Task 2: Implement `readPreviewFromState` with TDD

Test-first extraction of the seven harmonic weights and the crossover frequency from an APVTS state `ValueTree`.

**Files:**
- Create: `tests/PresetPreviewTests.cpp`
- Modify: `Source/PresetManager.cpp`
- Modify: `CMakeLists.txt` (add test file)

- [ ] **Step 1: Write the failing tests**

Create `tests/PresetPreviewTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PresetManager.h"
#include "Parameters.h"

using kaigen::phantom::PresetManager;
using kaigen::phantom::PreviewData;

// Helpers ────────────────────────────────────────────────────────────────

// Build a minimal APVTS-style state tree with an inner <PARAM id="..." value="..."/>
// per parameter. replaceState reads from exactly this shape.
static juce::ValueTree makeState(const std::map<juce::String, float>& params)
{
    juce::ValueTree state("STATE");
    for (const auto& [id, value] : params)
    {
        juce::ValueTree p("PARAM");
        p.setProperty("id",    id,    nullptr);
        p.setProperty("value", value, nullptr);
        state.appendChild(p, nullptr);
    }
    return state;
}

// Tests ──────────────────────────────────────────────────────────────────

TEST_CASE("readPreviewFromState extracts all seven harmonic weights")
{
    auto state = makeState({
        { ParamID::RECIPE_H2, 0.10f },
        { ParamID::RECIPE_H3, 0.20f },
        { ParamID::RECIPE_H4, 0.30f },
        { ParamID::RECIPE_H5, 0.40f },
        { ParamID::RECIPE_H6, 0.50f },
        { ParamID::RECIPE_H7, 0.60f },
        { ParamID::RECIPE_H8, 0.70f },
        { ParamID::PHANTOM_THRESHOLD, 120.0f },
    });

    auto preview = PresetManager::readPreviewFromState(state);

    REQUIRE(preview.h[0] == Catch::Approx(0.10f));
    REQUIRE(preview.h[1] == Catch::Approx(0.20f));
    REQUIRE(preview.h[2] == Catch::Approx(0.30f));
    REQUIRE(preview.h[3] == Catch::Approx(0.40f));
    REQUIRE(preview.h[4] == Catch::Approx(0.50f));
    REQUIRE(preview.h[5] == Catch::Approx(0.60f));
    REQUIRE(preview.h[6] == Catch::Approx(0.70f));
    REQUIRE(preview.crossover == Catch::Approx(120.0f));
}

TEST_CASE("readPreviewFromState returns defaults when params are missing")
{
    juce::ValueTree empty("STATE");
    auto preview = PresetManager::readPreviewFromState(empty);

    for (int i = 0; i < 7; ++i)
        REQUIRE(preview.h[i] == Catch::Approx(0.0f));
    REQUIRE(preview.crossover == Catch::Approx(120.0f));  // matches APVTS default
}

TEST_CASE("readPreviewFromState handles partial params (legacy preset)")
{
    auto state = makeState({
        { ParamID::RECIPE_H3, 0.75f },
        { ParamID::PHANTOM_THRESHOLD, 45.0f },
    });

    auto preview = PresetManager::readPreviewFromState(state);

    REQUIRE(preview.h[0] == Catch::Approx(0.0f));   // H2 missing
    REQUIRE(preview.h[1] == Catch::Approx(0.75f));  // H3 present
    REQUIRE(preview.h[6] == Catch::Approx(0.0f));   // H8 missing
    REQUIRE(preview.crossover == Catch::Approx(45.0f));
}
```

- [ ] **Step 2: Register the test file in CMake**

In `CMakeLists.txt`, find the `PhantomTests` target's source list (near the other `tests/*.cpp` entries) and add `tests/PresetPreviewTests.cpp`. If the `PresetManager` sources aren't already linked into `PhantomTests`, add `Source/PresetManager.cpp` as well.

Run: `cmake --build build --config RelWithDebInfo --target PhantomTests` and confirm the new file is being compiled (it should fail linking because `readPreviewFromState` isn't defined yet).

Expected: link error referencing `readPreviewFromState`.

- [ ] **Step 3: Implement `readPreviewFromState` in `PresetManager.cpp`**

Add this function body in `Source/PresetManager.cpp`, near `readMetadataFromFile` (around line 80):

```cpp
PreviewData PresetManager::readPreviewFromState(const juce::ValueTree& state)
{
    PreviewData data;

    const juce::String paramIds[7] = {
        ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
        ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7,
        ParamID::RECIPE_H8,
    };

    // APVTS serializes each parameter as a <PARAM id="..." value="..."/> child.
    // Walk the tree and pick out the ones we care about.
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (! child.hasProperty("id")) continue;

        const auto id = child.getProperty("id").toString();
        const auto value = (float) (double) child.getProperty("value", 0.0);

        if (id == ParamID::PHANTOM_THRESHOLD)
        {
            data.crossover = value;
            continue;
        }

        for (int h = 0; h < 7; ++h)
        {
            if (id == paramIds[h])
            {
                data.h[h] = value;
                break;
            }
        }
    }

    return data;
}
```

Also add `#include "Parameters.h"` at the top of `PresetManager.cpp` if it isn't already present.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build --config RelWithDebInfo --target PhantomTests && ./build/RelWithDebInfo/PhantomTests.exe "[readPreviewFromState]*"`

If the tag filter doesn't match (tests above use `TEST_CASE` without tags), just run the whole suite and look for the three new test cases:

Run: `./build/RelWithDebInfo/PhantomTests.exe`

Expected: all three new test cases pass; the existing 25+ test cases still pass.

- [ ] **Step 5: Commit**

```bash
git add Source/PresetManager.cpp tests/PresetPreviewTests.cpp CMakeLists.txt
git commit -m "feat(preset): extract harmonic weights + crossover for preview"
```

---

### Task 3: Populate `PresetInfo::preview` during the disk scan

**Files:**
- Modify: `Source/PresetManager.cpp`

- [ ] **Step 1: Extend the scan loop to populate `preview`**

In `Source/PresetManager.cpp`, find `scanPresetsFromDisk` (around line 110) and locate the loop that reads each preset file (around line 156). The current body looks roughly like:

```cpp
for (const auto& presetFile : packDir.findChildFiles(juce::File::findFiles, false, "*.fxp"))
{
    PresetInfo info;
    info.file = presetFile;
    info.metadata = readMetadataFromFile(presetFile);
    info.metadata.packName = packName;
    info.metadata.isFactory = (packName == kFactoryPackName);
    info.metadata.isFavorite = isFavorite(info.metadata.name, packName);
    presets.push_back(info);
}
```

Extend it to also extract preview data. Replace with:

```cpp
for (const auto& presetFile : packDir.findChildFiles(juce::File::findFiles, false, "*.fxp"))
{
    PresetInfo info;
    info.file = presetFile;
    info.metadata = readMetadataFromFile(presetFile);
    info.metadata.packName = packName;
    info.metadata.isFactory = (packName == kFactoryPackName);
    info.metadata.isFavorite = isFavorite(info.metadata.name, packName);

    // Parse the state tree once more to extract preview parameter values.
    // readMetadataFromFile already parses the file; we repeat here to avoid
    // a signature change. The cost is negligible (few dozen presets at load).
    if (auto xml = juce::parseXML(presetFile))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid())
            info.preview = readPreviewFromState(state);
    }

    presets.push_back(info);
}
```

- [ ] **Step 2: Build**

Run: `cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel`

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add Source/PresetManager.cpp
git commit -m "feat(preset): populate PreviewData during preset scan"
```

---

### Task 4: Serialize `preview` into the `getAllPresets` JSON

**Files:**
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Extend the JSON builder**

In `Source/PluginEditor.cpp`, find the `.withNativeFunction("getAllPresets", ...)` lambda (around line 334). The current body builds each `item` with just `metadata`. Update the inner loop to also build and attach a `preview` object.

Replace the current preset-iteration block with:

```cpp
for (const auto& p : presets)
{
    auto* meta = new juce::DynamicObject();
    meta->setProperty("name",        p.metadata.name);
    meta->setProperty("type",        p.metadata.type);
    meta->setProperty("designer",    p.metadata.designer);
    meta->setProperty("description", p.metadata.description);
    meta->setProperty("isFavorite",  p.metadata.isFavorite);
    meta->setProperty("isFactory",   p.metadata.isFactory);

    // Preview: 7 harmonic weights + crossover Hz
    auto* preview = new juce::DynamicObject();
    juce::Array<juce::var> hArr;
    for (int i = 0; i < 7; ++i)
        hArr.add(juce::var(p.preview.h[i]));
    preview->setProperty("h",         juce::var(hArr));
    preview->setProperty("crossover", juce::var(p.preview.crossover));

    auto* item = new juce::DynamicObject();
    item->setProperty("metadata", juce::var(meta));
    item->setProperty("preview",  juce::var(preview));
    arr.add(juce::var(item));
}
```

- [ ] **Step 2: Build and quick-smoke in DevTools**

Run: `cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel`

Expected: clean build. VST3 auto-copies to `%LOCALAPPDATA%/Programs/Common/VST3` per project convention.

After build: open the plugin in Ableton, right-click the UI → Inspect (DevTools opens on port 9222). In the console:

```js
(await window.Juce.getNativeFunction('getAllPresets')()).Factory[0]
```

Expected: the returned object has both `metadata` and `preview` keys; `preview.h` is a 7-element array; `preview.crossover` is a number.

- [ ] **Step 3: Commit**

```bash
git add Source/PluginEditor.cpp
git commit -m "feat(preset): include preview data in getAllPresets JSON"
```

---

### Task 5: Implement `preset-spectrum.js`

Curve math + SVG rendering. Pure JS module, no external deps, no ES import — loaded as a regular script per project convention.

**Files:**
- Create: `Source/WebUI/preset-spectrum.js`

- [ ] **Step 1: Write the module**

Create `Source/WebUI/preset-spectrum.js`:

```javascript
/**
 * preset-spectrum.js — renders the preset-preview harmonic fingerprint.
 *
 * Gaussian-peak-sum curve through H2..H8 on a log-frequency axis, plus a
 * dashed crossover marker labeled in Hz. Monochrome ink on the preset
 * browser's translucent preview panel.
 *
 * Data contract:
 *   preview = { h: [7 floats 0..1], crossover: Hz }
 *
 * Uses inline SVG so the browser resolution-scales automatically and no
 * canvas resize logic is needed.
 */
(function(){

const DISPLAY_FREQ_LOW = 30;
const SIGMA            = 0.04; // Gaussian peak width in log10(Hz)
const SAMPLES          = 48;

// Same log-frequency mapping the main spectrum analyzer uses.
function freqToX(freq, width, displayHigh) {
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(displayHigh);
    const f = Math.max(freq, DISPLAY_FREQ_LOW);
    return ((Math.log10(f) - logLo) / (logHi - logLo)) * width;
}

// Sum of seven Gaussian peaks at 2f..8f where f = crossover.
function evaluate(freq, preview) {
    const fundamental = Math.max(10, preview.crossover || 80);
    const logF = Math.log10(freq);
    let s = 0;
    for (let i = 0; i < 7; ++i) {
        const centerHz = (i + 2) * fundamental;
        const d = (logF - Math.log10(centerHz)) / SIGMA;
        s += (preview.h[i] || 0) * Math.exp(-d * d);
    }
    return Math.max(0, Math.min(1, s));
}

// Pick three axis labels from a fixed set, keeping those that fit inside
// [DISPLAY_FREQ_LOW, displayHigh] and are visually spread.
function pickAxisLabels(displayHigh) {
    const candidates = [
        { hz: 30,    label: '30' },
        { hz: 100,   label: '100' },
        { hz: 300,   label: '300' },
        { hz: 1000,  label: '1k' },
        { hz: 3000,  label: '3k' },
        { hz: 10000, label: '10k' },
    ];
    const inRange = candidates.filter(c => c.hz >= DISPLAY_FREQ_LOW && c.hz <= displayHigh);
    if (inRange.length <= 3) return inRange;
    // Evenly pick three: first, middle, last
    return [inRange[0], inRange[Math.floor(inRange.length / 2)], inRange[inRange.length - 1]];
}

// Build a smooth path (quadratic-bezier through midpoints) from an array
// of points. Matches spectrum.js:strokeCurve technique.
function buildPath(pts) {
    if (pts.length < 2) return '';
    let d = `M ${pts[0].x.toFixed(2)} ${pts[0].y.toFixed(2)}`;
    for (let i = 1; i < pts.length - 1; ++i) {
        const xc = (pts[i].x + pts[i + 1].x) / 2;
        const yc = (pts[i].y + pts[i + 1].y) / 2;
        d += ` Q ${pts[i].x.toFixed(2)} ${pts[i].y.toFixed(2)} ${xc.toFixed(2)} ${yc.toFixed(2)}`;
    }
    const last = pts[pts.length - 1];
    d += ` L ${last.x.toFixed(2)} ${last.y.toFixed(2)}`;
    return d;
}

// Escape text for safe insertion as SVG content.
function esc(s) { return String(s).replace(/[&<>]/g, c => ({ '&':'&amp;','<':'&lt;','>':'&gt;' }[c])); }

function render(svgEl, preview) {
    if (!svgEl || !preview || !Array.isArray(preview.h)) return;

    const W = 280;
    const H = 56;
    svgEl.setAttribute('viewBox', `0 0 ${W} ${H}`);
    svgEl.setAttribute('preserveAspectRatio', 'none');

    const crossover = Math.max(10, preview.crossover || 80);
    const displayHigh = Math.max(1000, 10 * crossover);

    // Build curve samples.
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(displayHigh);
    const pts = [];
    for (let i = 0; i < SAMPLES; ++i) {
        const t = i / (SAMPLES - 1);
        const f = Math.pow(10, logLo + (logHi - logLo) * t);
        const x = freqToX(f, W, displayHigh);
        const y = H * (1 - evaluate(f, preview));
        pts.push({ x, y });
    }

    const curvePath = buildPath(pts);
    const fillPath  = `${curvePath} L ${W.toFixed(2)} ${H} L 0 ${H} Z`;

    const xoverX = freqToX(crossover, W, displayHigh);
    const xoverLabel = crossover < 100
        ? `${crossover.toFixed(0)}Hz`
        : `${(crossover / 1000).toFixed(crossover < 1000 ? 0 : 1)}${crossover < 1000 ? 'Hz' : 'kHz'}`;

    const axisLabels = pickAxisLabels(displayHigh);

    // Build the SVG content as an innerHTML blob — simpler than DOM building,
    // and safer because all inputs are numeric or escaped.
    const guideLines = [14, 28, 42].map(y =>
        `<line x1="0" y1="${y}" x2="${W}" y2="${y}" stroke="rgba(0,0,0,0.05)" stroke-width="1"/>`
    ).join('');

    const axisLabelsSvg = axisLabels.map(a => {
        const ax = freqToX(a.hz, W, displayHigh);
        return `<text x="${ax.toFixed(2)}" y="52" font-size="6" fill="rgba(0,0,0,0.38)" font-family="monospace" text-anchor="middle">${esc(a.label)}</text>`;
    }).join('');

    svgEl.innerHTML = `
        <defs>
            <linearGradient id="preset-spec-fill" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stop-color="rgba(0,0,0,0.22)"/>
                <stop offset="100%" stop-color="rgba(0,0,0,0.02)"/>
            </linearGradient>
        </defs>
        ${guideLines}
        <line x1="${xoverX.toFixed(2)}" y1="2" x2="${xoverX.toFixed(2)}" y2="50"
              stroke="rgba(0,0,0,0.30)" stroke-width="0.6" stroke-dasharray="2 2"/>
        <text x="${(xoverX + 2).toFixed(2)}" y="9" font-size="5.5"
              fill="rgba(0,0,0,0.40)" font-family="monospace">${esc(xoverLabel)}</text>
        <path d="${fillPath}" fill="url(#preset-spec-fill)"/>
        <path d="${curvePath}" fill="none" stroke="rgba(0,0,0,0.72)" stroke-width="1.2"
              stroke-linejoin="round" stroke-linecap="round"/>
        ${axisLabelsSvg}
    `;
}

window.PresetSpectrum = { render };

})();
```

- [ ] **Step 2: Commit**

```bash
git add Source/WebUI/preset-spectrum.js
git commit -m "feat(ui): add preset-spectrum.js curve + SVG renderer"
```

---

### Task 6: Wire `preset-spectrum.js` into the build

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `Source/WebUI/index.html`

- [ ] **Step 1: Add the JS file to BinaryData sources**

In `CMakeLists.txt`, find the block listing `Source/WebUI/*.js` files (around line 57, the `juce_add_binary_data` or similar source list) and add `Source/WebUI/preset-spectrum.js` — preferably right before `preset-system.js` so the load order matches `index.html`:

```cmake
    Source/WebUI/juce-frontend.js
    Source/WebUI/preset-spectrum.js
    Source/WebUI/preset-system.js
)
```

- [ ] **Step 2: Add the script tag to index.html**

In `Source/WebUI/index.html`, find the block of `<script src="/...">` tags near the bottom (around line 351). Add a new tag *before* `preset-system.js`:

```html
<script src="/juce-frontend.js"></script>
<script src="/phantom.js"></script>
<script src="/preset-spectrum.js"></script>
<script src="/preset-system.js"></script>
<script src="/spectrum.js"></script>
```

- [ ] **Step 3: Verify the BinaryData lookup**

BinaryData names in JUCE strip dashes — `preset-spectrum.js` becomes the identifier `presetspectrum_js`. The resource provider in `PluginEditor.cpp` already does `name.replace("-", "")` (or equivalent) for all other JS files. No code change expected here; if the file 404s at runtime, inspect `PhantomEditor::getResource` and match the existing lookup pattern.

- [ ] **Step 4: Build and confirm file is served**

Run: `cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel`

Expected: clean build.

Then open the plugin in Ableton, open DevTools, and in the Network tab reload — confirm `/preset-spectrum.js` loads with status 200.

Also run in the console:

```js
typeof window.PresetSpectrum
```

Expected: `"object"` (not `"undefined"`).

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt Source/WebUI/index.html
git commit -m "build: bundle preset-spectrum.js into BinaryData"
```

---

### Task 7: Call the renderer from `preset-system.js:updatePreview`

**Files:**
- Modify: `Source/WebUI/preset-system.js`

- [ ] **Step 1: Prepend the SVG and call the renderer**

In `Source/WebUI/preset-system.js`, find the `updatePreview` function (around line 669). The current body is:

```javascript
function updatePreview(name, pack) {
    const preview = document.getElementById('browser-preview');
    if (!preview) return;
    const entry = flatPresetList().find(p => p.name === name && p.pack === pack);
    if (!entry) { preview.textContent = 'Select a preset'; return; }
    preview.innerHTML = `
        <div style="font-size: 12px; font-weight: 600; color: rgba(0,0,0,0.85); margin-bottom: 8px;">${escapeHtml(entry.meta.name)}</div>
        <div><span style="color: rgba(0,0,0,0.50);">Type:</span> ${escapeHtml(entry.meta.type || '—')}</div>
        ...
    `;
    ...
}
```

Update the innerHTML template to include the SVG at the top, and call `window.PresetSpectrum.render` after the HTML is written:

```javascript
function updatePreview(name, pack) {
    const preview = document.getElementById('browser-preview');
    if (!preview) return;
    const entry = flatPresetList().find(p => p.name === name && p.pack === pack);
    if (!entry) { preview.textContent = 'Select a preset'; return; }
    preview.innerHTML = `
        <svg class="preview-spectrum" viewBox="0 0 280 56" preserveAspectRatio="none"
             style="width:100%;height:56px;display:block;margin-bottom:10px;"></svg>
        <div style="font-size: 12px; font-weight: 600; color: rgba(0,0,0,0.85); margin-bottom: 8px;">${escapeHtml(entry.meta.name)}</div>
        <div><span style="color: rgba(0,0,0,0.50);">Type:</span> ${escapeHtml(entry.meta.type || '—')}</div>
        <div><span style="color: rgba(0,0,0,0.50);">Designer:</span> ${escapeHtml(entry.meta.designer || '—')}</div>
        <div><span style="color: rgba(0,0,0,0.50);">Pack:</span> ${escapeHtml(entry.pack)}</div>
        ${entry.meta.description ? `<div style="margin-top: 10px; padding-top: 10px; border-top: 1px solid rgba(0,0,0,0.10); color: rgba(0,0,0,0.65); white-space: pre-wrap;">${escapeHtml(entry.meta.description)}</div>` : ''}
        ${entry.pack === 'User' ? `<div style="margin-top: 10px;"><button class="preview-delete" style="background: linear-gradient(135deg, #BBBDBF 0%, #AEAFB1 100%); color: rgba(0,0,0,0.70); border: 1px solid rgba(0,0,0,0.12); padding: 4px 10px; border-radius: 3px; cursor: pointer; font-size: 10px; font-family: 'Space Grotesk', system-ui;">Delete</button></div>` : ''}
    `;

    const svg = preview.querySelector('.preview-spectrum');
    if (svg && entry.preview && window.PresetSpectrum) {
        window.PresetSpectrum.render(svg, entry.preview);
    }

    const btn = preview.querySelector('.preview-delete');
    if (btn) {
        btn.addEventListener('click', async () => {
            if (!confirm(`Delete "${entry.meta.name}"?`)) return;
            await deletePreset(entry.meta.name, entry.pack);
            renderBrowserList(document.getElementById('browser-search').value);
            preview.textContent = 'Select a preset';
        });
    }
}
```

Only three things changed: (a) the `<svg>` line was added as the first child of the innerHTML template, (b) after setting innerHTML, the svg is selected and rendered, (c) nothing else.

- [ ] **Step 2: Check the `flatPresetList` shape**

The new code reads `entry.preview`. Confirm that `flatPresetList` (the helper that transforms the `getAllPresets` JSON into the flat entry shape) passes the `preview` property through. Search `preset-system.js` for `flatPresetList` and inspect the mapping — if it only copies `meta`, add `preview: p.preview` to the returned object.

Quick grep:

Run: `grep -n "flatPresetList\|function flatPresetList\|preview" Source/WebUI/preset-system.js`

Expected: find the mapping that produces `{ meta, pack }` entries. If `preview` isn't there, add it so the returned object becomes `{ meta, pack, preview }`.

- [ ] **Step 3: Build**

Run: `cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel`

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/preset-system.js
git commit -m "feat(ui): render preset-spectrum curve in browser preview card"
```

---

### Task 8: Manual verification in Ableton Live 12

No code change. This task verifies the feature end-to-end in the host.

- [ ] **Step 1: Quit and restart Ableton**

Per project memory, Ableton caches VST3 DLLs aggressively. A clean quit and restart is required to pick up the new build.

- [ ] **Step 2: Open the plugin and the preset browser**

Load Phantom on any track. Open the preset browser (the "Library" button in the header).

- [ ] **Step 3: Hover each preset pack's presets**

For each preset in each pack:
- The preview card shows an SVG curve above the name
- The curve has distinct peaks corresponding to H2–H8 weights (visually verify against the preset's recipe wheel once loaded)
- A dashed vertical line with a Hz label appears at the preset's crossover frequency
- The curve scales smoothly when the plugin window is resized

- [ ] **Step 4: Create and re-hover a distinctive preset**

Load any preset. Set H2–H8 manually to `[0, 1, 0, 0, 0, 0, 0]` (only H3 active) and the crossover to 60 Hz. Save as a new User preset.

Hover it in the browser. Expected: single narrow peak at ~180 Hz (3 × 60); dashed marker labeled `60Hz` left of the peak.

- [ ] **Step 5: Legacy preset check (if any exist)**

If any preset predates this change and has no recipe values serialized, hover it. Expected: flat baseline curve, marker at `80Hz` (default). No crash, no console errors.

- [ ] **Step 6: Window-resize check**

Drag the plugin window edge to resize it. The preview-panel curve should rescale cleanly without jagged edges.

- [ ] **Step 7: DevTools console check**

Right-click → Inspect. Confirm zero red errors in the console while hovering presets.

- [ ] **Step 8: Commit the verification note**

If everything passes, no code changed — skip the commit. If any bug was fixed during verification, commit it as a follow-up with a message like `fix(preset-spectrum): <specific issue>` and re-run the relevant test.

---

## Self-Review

Spec coverage:
- §1 Summary → Tasks 1–7 implement the full render path.
- §3.1–3.3 Data source → Tasks 1, 2, 4 (PreviewData, extraction, JSON).
- §4.1–4.4 Rendering → Task 5 (module), Task 7 (integration).
- §5.1–5.3 Integration points → Tasks 3, 4, 6, 7.
- §6 Edge cases → Task 2 unit tests cover missing/partial params; Task 8 manual steps cover low/high crossover.
- §7 Out of scope → not implemented (correct).
- §8 Testing strategy → Task 2 covers unit tests; Task 8 covers manual steps exactly.

Placeholder scan: no TBD/TODO/placeholder content found. All steps contain complete code or exact commands.

Type consistency:
- `PreviewData { float h[7]; float crossover; }` — consistent in Tasks 1, 2, 3, 4.
- `window.PresetSpectrum.render(svgEl, preview)` — consistent signature in Tasks 5 and 7.
- `entry.preview.h` / `entry.preview.crossover` — consistent JS shape matching the JSON in Task 4.
