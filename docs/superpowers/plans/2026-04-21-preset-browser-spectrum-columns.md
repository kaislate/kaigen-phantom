# Preset Browser Spectrum Columns Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add spectrum thumbnails, a Skip column, and click-to-sort headers to the preset browser's in-list view, with skip-driven octave shift and a SUB BASS label replacing the curve when peaks fall below the display frame.

**Architecture:** Extend `PreviewData` with `int skip`, serialize into the `getAllPresets` JSON. Teach `preset-spectrum.js` to accept an `options.variant` (`'preview'` default, `'thumbnail'` stripped) and to apply `fundamentalHz = crossover / 2^skip`, short-circuiting to a big SUB BASS label when the fundamental falls below 6 Hz. Restructure `renderBrowserList` to emit a header row + six-column grid rows with an inline thumbnail SVG per preset. Add a module-scope `browserSort = { column, dir }` with a dispatch-by-column comparator, including a spectral-centroid helper for the Shape sort and a favorites-first branch for the Heart sort.

**Tech Stack:** C++20 / JUCE 8 (ValueTree, APVTS, DynamicObject, JSON); Catch2 v3 for C++ tests; vanilla JS + inline SVG (no build step); WebView2 on Windows.

**Spec:** [2026-04-21-preset-browser-spectrum-columns-design.md](../specs/2026-04-21-preset-browser-spectrum-columns-design.md)

---

## File Structure

**Modify:**
- `Source/PresetManager.h` — add `int skip` to `PreviewData`
- `Source/PresetManager.cpp` — extract `SYNTH_SKIP` in `readPreviewFromState`, rounded and clamped
- `Source/PluginEditor.cpp` — emit `preview.skip` in the `getAllPresets` JSON
- `tests/PresetPreviewTests.cpp` — extend three existing cases plus one new clamp test
- `Source/WebUI/preset-spectrum.js` — skip-driven shift, SUB BASS guard, `options.variant`
- `Source/WebUI/preset-system.js` — header row, row restructure, thumbnail render call, sort state, comparator, click handlers, preview-card variant arg
- `Source/WebUI/styles.css` — header row, sortable/static span styles, sort-arrow

Each file has a single, narrow responsibility. The C++ side only handles data extraction and serialization; the JS renderer stays focused on drawing; `preset-system.js` is the one layer that changes shape the most, and even there the changes are localized to `renderBrowserList` + a new comparator + one updated call site.

---

### Task 1: Extend `PreviewData` with `skip`

Minimal data-model change. No behavior yet.

**Files:**
- Modify: `Source/PresetManager.h`

- [ ] **Step 1: Add the `skip` field to `PreviewData`**

In `Source/PresetManager.h`, find the `PreviewData` struct (currently containing `float h[7]` and `float crossover`) and add the skip field:

```cpp
// Parameter values extracted from a preset for the browser preview spectrum.
// Populated once at scan time so hovering a preset triggers no disk I/O.
struct PreviewData
{
    float h[7]       {};       // recipe_h2 .. recipe_h8, normalized 0..1
    float crossover  = 120.0f; // phantom_threshold in Hz (matches APVTS default in Parameters.h)
    int   skip       = 0;      // synth_skip, 0..8 (matches APVTS default)
};
```

- [ ] **Step 2: Verify the project still compiles**

Run: `cmake --build build --config RelWithDebInfo --target Phantom_VST3`

Expected: clean build. Nothing reads `skip` yet.

- [ ] **Step 3: Commit**

```bash
git add Source/PresetManager.h
git commit -m "feat(preset): add skip field to PreviewData"
```

---

### Task 2: TDD the `SYNTH_SKIP` extraction

Test-first extension to `readPreviewFromState` so legacy presets still default to skip=0 and out-of-range values clamp.

**Files:**
- Modify: `tests/PresetPreviewTests.cpp`
- Modify: `Source/PresetManager.cpp`

- [ ] **Step 1: Extend existing tests with skip assertions, add clamp test**

Open `tests/PresetPreviewTests.cpp`. In the three existing test cases, add `SYNTH_SKIP` assertions. Also add one new test case at the bottom.

**Test 1 — "extracts all seven harmonic weights":** add one more input and one more assertion. Replace the existing state map and assertions block with:

```cpp
TEST_CASE("readPreviewFromState extracts all seven harmonic weights")
{
    // Recipe params are stored in APVTS as 0..100 percentages;
    // readPreviewFromState normalizes them to 0..1.
    auto state = makeState({
        { ParamID::RECIPE_H2, 10.0f },
        { ParamID::RECIPE_H3, 20.0f },
        { ParamID::RECIPE_H4, 30.0f },
        { ParamID::RECIPE_H5, 40.0f },
        { ParamID::RECIPE_H6, 50.0f },
        { ParamID::RECIPE_H7, 60.0f },
        { ParamID::RECIPE_H8, 70.0f },
        { ParamID::PHANTOM_THRESHOLD, 120.0f },
        { ParamID::SYNTH_SKIP, 3.0f },
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
    REQUIRE(preview.skip == 3);
}
```

**Test 2 — "returns defaults when params are missing":** add the skip-default assertion. Replace with:

```cpp
TEST_CASE("readPreviewFromState returns defaults when params are missing")
{
    juce::ValueTree empty("STATE");
    auto preview = PresetManager::readPreviewFromState(empty);

    for (int i = 0; i < 7; ++i)
        REQUIRE(preview.h[i] == Catch::Approx(0.0f));
    REQUIRE(preview.crossover == Catch::Approx(120.0f));  // matches APVTS default
    REQUIRE(preview.skip == 0);
}
```

**Test 3 — "handles partial params (legacy preset)":** add one skip input and one assertion. Replace with:

```cpp
TEST_CASE("readPreviewFromState handles partial params (legacy preset)")
{
    auto state = makeState({
        { ParamID::RECIPE_H3, 75.0f },   // 75% → 0.75 normalized
        { ParamID::PHANTOM_THRESHOLD, 45.0f },
        { ParamID::SYNTH_SKIP, 2.0f },
    });

    auto preview = PresetManager::readPreviewFromState(state);

    // Only H3 was set; all other harmonics should remain at their 0.0 default.
    REQUIRE(preview.h[0] == Catch::Approx(0.0f));   // H2
    REQUIRE(preview.h[1] == Catch::Approx(0.75f));  // H3 (75% normalized)
    REQUIRE(preview.h[2] == Catch::Approx(0.0f));   // H4
    REQUIRE(preview.h[3] == Catch::Approx(0.0f));   // H5
    REQUIRE(preview.h[4] == Catch::Approx(0.0f));   // H6
    REQUIRE(preview.h[5] == Catch::Approx(0.0f));   // H7
    REQUIRE(preview.h[6] == Catch::Approx(0.0f));   // H8
    REQUIRE(preview.crossover == Catch::Approx(45.0f));
    REQUIRE(preview.skip == 2);
}
```

**New Test 4 — "clamps skip to valid range":** append at the bottom of the file:

```cpp
TEST_CASE("readPreviewFromState clamps skip to valid range")
{
    auto tooHigh = makeState({ { ParamID::SYNTH_SKIP, 42.0f } });
    REQUIRE(PresetManager::readPreviewFromState(tooHigh).skip == 8);

    auto negative = makeState({ { ParamID::SYNTH_SKIP, -3.0f } });
    REQUIRE(PresetManager::readPreviewFromState(negative).skip == 0);

    // Fractional values should round to nearest int.
    auto fraction = makeState({ { ParamID::SYNTH_SKIP, 2.6f } });
    REQUIRE(PresetManager::readPreviewFromState(fraction).skip == 3);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Build and run:

```bash
cmake --build build --config RelWithDebInfo --target KaigenPhantomTests
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

Expected: four test failures — the three existing cases fail on the new `preview.skip` assertions, and the new "clamps skip" case fails.

- [ ] **Step 3: Implement the skip extraction**

In `Source/PresetManager.cpp`, find `readPreviewFromState`. The current loop body iterates each PARAM child and dispatches on `id`. Add a third branch for `SYNTH_SKIP` between the existing `PHANTOM_THRESHOLD` check and the `paramIds[h]` loop:

```cpp
if (id == ParamID::PHANTOM_THRESHOLD)
{
    data.crossover = value;
    continue;
}

if (id == ParamID::SYNTH_SKIP)
{
    // APVTS stores this as a stepped float; round and clamp to the valid 0..8 range
    // so corrupted or out-of-range presets still render safely.
    data.skip = juce::jlimit(0, 8, juce::roundToInt(value));
    continue;
}

for (int h = 0; h < 7; ++h)
{
    ...
}
```

(Leave the rest of the function unchanged.)

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake --build build --config RelWithDebInfo --target KaigenPhantomTests
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

Expected: all tests pass. Count should increase by 1 (the new "clamps" case) over the previous baseline; assertions increase by ~7.

- [ ] **Step 5: Commit**

```bash
git add Source/PresetManager.cpp tests/PresetPreviewTests.cpp
git commit -m "feat(preset): extract SYNTH_SKIP into PreviewData"
```

---

### Task 3: Serialize `skip` into the `getAllPresets` JSON

**Files:**
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Add the `skip` property to the preview JSON object**

In `Source/PluginEditor.cpp`, find the `getAllPresets` lambda's per-preset loop. It currently builds the `preview` DynamicObject with `h` and `crossover`. Add one line after `crossover`:

```cpp
// Preview: 7 harmonic weights + crossover Hz + skip count
auto* preview = new juce::DynamicObject();
juce::Array<juce::var> hArr;
for (int i = 0; i < 7; ++i)
    hArr.add(juce::var(p.preview.h[i]));
preview->setProperty("h",         juce::var(hArr));
preview->setProperty("crossover", juce::var(p.preview.crossover));
preview->setProperty("skip",      juce::var(p.preview.skip));   // NEW
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel
```

Expected: clean build. VST3 auto-copies to `%LOCALAPPDATA%/Programs/Common/VST3/`.

- [ ] **Step 3: Commit**

```bash
git add Source/PluginEditor.cpp
git commit -m "feat(preset): include skip in getAllPresets JSON"
```

---

### Task 4: Extend `preset-spectrum.js` with skip shift, SUB BASS guard, and variant option

**Files:**
- Modify: `Source/WebUI/preset-spectrum.js`

This is the biggest single-file change in the plan. The new logic adds:
1. `options.variant` parameter (`'preview'` default, `'thumbnail'` stripped)
2. Skip-driven fundamental shift: `fundamentalHz = crossover / 2^skip`
3. Sub-bass guard: when `fundamentalHz < 6`, replace the curve with a centered "SUB BASS" label instead

- [ ] **Step 1: Rewrite `preset-spectrum.js`**

Open `Source/WebUI/preset-spectrum.js` and replace its contents with the following (the diff from the current version is: new `renderSubBassLabel` helper, new `options` arg, the fundamental calculation now includes skip, the variant-specific suppression of axis labels / Hz label / guide lines, and a new default viewBox per variant):

```javascript
/**
 * preset-spectrum.js — renders the preset-preview harmonic fingerprint.
 *
 * Gaussian-peak-sum curve through H2..H8 on a log-frequency axis, plus a
 * dashed crossover marker. Monochrome ink on the preset browser's translucent
 * preview panel.
 *
 * Data contract:
 *   preview = { h: [7 floats 0..1], crossover: Hz, skip: int 0..8 }
 *
 * Call signature:
 *   render(svgEl, preview)                           → preview-card variant (default)
 *   render(svgEl, preview, { variant: 'thumbnail' }) → stripped for browser rows
 *
 * If the effective fundamental (crossover / 2^skip) drops below 6 Hz,
 * the curve is replaced by a big muted "SUB BASS" label.
 */
(function(){

const DISPLAY_FREQ_LOW  = 30;
const DISPLAY_FREQ_HIGH = 1000;
const SIGMA             = 0.04; // Gaussian peak width in log10(Hz)
const SAMPLES           = 48;
const SUB_BASS_CUTOFF   = 6.0;  // Hz — below this, show SUB BASS instead of curve

function esc(s) { return String(s).replace(/[&<>]/g, c => ({ '&':'&amp;','<':'&lt;','>':'&gt;' }[c])); }

function freqToX(freq, width) {
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(DISPLAY_FREQ_HIGH);
    const f = Math.max(freq, DISPLAY_FREQ_LOW);
    return ((Math.log10(f) - logLo) / (logHi - logLo)) * width;
}

function effectiveFundamental(preview) {
    const cross = Math.max(10, preview.crossover || 120);
    const skip  = Math.max(0, preview.skip || 0);
    return cross / Math.pow(2, skip);
}

function evaluate(freq, preview, fundamental) {
    const logF = Math.log10(freq);
    let s = 0;
    for (let i = 0; i < 7; ++i) {
        const centerHz = (i + 2) * fundamental;
        const d = (logF - Math.log10(centerHz)) / SIGMA;
        s += (preview.h[i] || 0) * Math.exp(-d * d);
    }
    return Math.max(0, Math.min(1, s));
}

function pickAxisLabels() {
    // Fixed display range means fixed label choices.
    return [
        { hz: 30,   label: '30' },
        { hz: 300,  label: '300' },
        { hz: 1000, label: '1k' },
    ];
}

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

function renderSubBassLabel(svgEl, variant) {
    const isThumbnail = variant === 'thumbnail';
    const W = isThumbnail ? 170 : 280;
    const H = isThumbnail ? 26  : 56;
    const fontSize = isThumbnail ? 10 : 20;
    const spacing  = isThumbnail ? '0.08em' : '0.15em';
    svgEl.setAttribute('viewBox', `0 0 ${W} ${H}`);
    svgEl.setAttribute('preserveAspectRatio', 'none');
    svgEl.innerHTML = `
        <text x="${W/2}" y="${H/2 + fontSize/3.5}" text-anchor="middle"
              font-size="${fontSize}" font-weight="700" letter-spacing="${spacing}"
              fill="rgba(0,0,0,0.22)"
              font-family="'Space Grotesk', system-ui, sans-serif">SUB BASS</text>
    `;
}

function render(svgEl, preview, options) {
    if (!svgEl || !preview || !Array.isArray(preview.h)) return;
    options = options || {};
    const isThumbnail = options.variant === 'thumbnail';
    const variant     = isThumbnail ? 'thumbnail' : 'preview';

    const fundamentalHz = effectiveFundamental(preview);
    if (fundamentalHz < SUB_BASS_CUTOFF) {
        renderSubBassLabel(svgEl, variant);
        return;
    }

    const W = isThumbnail ? 170 : 280;
    const H = isThumbnail ? 26  : 56;
    svgEl.setAttribute('viewBox', `0 0 ${W} ${H}`);
    svgEl.setAttribute('preserveAspectRatio', 'none');

    const crossover = Math.max(10, preview.crossover || 120);

    // Build curve samples across the fixed display range.
    const logLo = Math.log10(DISPLAY_FREQ_LOW);
    const logHi = Math.log10(DISPLAY_FREQ_HIGH);
    const pts = [];
    for (let i = 0; i < SAMPLES; ++i) {
        const t = i / (SAMPLES - 1);
        const f = Math.pow(10, logLo + (logHi - logLo) * t);
        const x = freqToX(f, W);
        const y = H * (1 - evaluate(f, preview, fundamentalHz));
        pts.push({ x, y });
    }

    const curvePath = buildPath(pts);
    const fillPath  = `${curvePath} L ${W.toFixed(2)} ${H} L 0 ${H} Z`;

    const xoverInRange = crossover >= DISPLAY_FREQ_LOW && crossover <= DISPLAY_FREQ_HIGH;
    const xoverX = xoverInRange ? freqToX(crossover, W) : -1;

    // Variant-specific chrome: thumbnail drops axis labels, Hz label, and guide lines.
    const guideLines = isThumbnail ? '' : [14, 28, 42].map(y =>
        `<line x1="0" y1="${y}" x2="${W}" y2="${y}" stroke="rgba(0,0,0,0.05)" stroke-width="1"/>`
    ).join('');

    const xoverMarker = xoverInRange
        ? (isThumbnail
            ? `<line x1="${xoverX.toFixed(2)}" y1="1" x2="${xoverX.toFixed(2)}" y2="${H - 1}" stroke="rgba(0,0,0,0.28)" stroke-width="0.5" stroke-dasharray="1.5 1.5"/>`
            : `<line x1="${xoverX.toFixed(2)}" y1="2" x2="${xoverX.toFixed(2)}" y2="50" stroke="rgba(0,0,0,0.30)" stroke-width="0.6" stroke-dasharray="2 2"/>`)
        : '';

    const xoverLabel = (!isThumbnail && xoverInRange)
        ? (() => {
              const lbl = crossover < 100
                  ? `${crossover.toFixed(0)}Hz`
                  : `${(crossover / 1000).toFixed(crossover < 1000 ? 0 : 1)}${crossover < 1000 ? 'Hz' : 'kHz'}`;
              return `<text x="${(xoverX + 2).toFixed(2)}" y="9" font-size="5.5" fill="rgba(0,0,0,0.40)" font-family="monospace">${esc(lbl)}</text>`;
          })()
        : '';

    const axisLabelsSvg = isThumbnail ? '' : pickAxisLabels().map(a => {
        const ax = freqToX(a.hz, W);
        return `<text x="${ax.toFixed(2)}" y="52" font-size="6" fill="rgba(0,0,0,0.38)" font-family="monospace" text-anchor="middle">${esc(a.label)}</text>`;
    }).join('');

    const curveStroke = isThumbnail ? 1.0 : 1.2;
    const fillGradientId = isThumbnail ? 'preset-spec-fill-thumb' : 'preset-spec-fill';

    svgEl.innerHTML = `
        <defs>
            <linearGradient id="${fillGradientId}" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stop-color="rgba(0,0,0,0.22)"/>
                <stop offset="100%" stop-color="rgba(0,0,0,0.02)"/>
            </linearGradient>
        </defs>
        ${guideLines}
        ${xoverMarker}
        ${xoverLabel}
        <path d="${fillPath}" fill="url(#${fillGradientId})"/>
        <path d="${curvePath}" fill="none" stroke="rgba(0,0,0,0.72)" stroke-width="${curveStroke}"
              stroke-linejoin="round" stroke-linecap="round"/>
        ${axisLabelsSvg}
    `;
}

window.PresetSpectrum = { render };

})();
```

Notes on the rewrite:
- `effectiveFundamental(preview)` centralizes the crossover/skip math so the same number is used for the curve evaluation and the sub-bass guard check
- Axis labels are now a fixed set (`30 / 300 / 1k`) since the display range no longer adapts
- `fillGradientId` differs between variants so the two gradient `<defs>` can coexist on the same page when the browser list renders many thumbnails alongside one preview card
- Existing call sites that pass no third argument continue to work — the `options || {}` fallback keeps everything backward compatible

- [ ] **Step 2: Build**

```bash
cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/preset-spectrum.js
git commit -m "feat(ui): skip shift + SUB BASS + thumbnail variant in preset-spectrum.js"
```

---

### Task 5: Add CSS for the browser header and sort affordances

**Files:**
- Modify: `Source/WebUI/styles.css`

- [ ] **Step 1: Append header and sort-span rules**

Open `Source/WebUI/styles.css` and append these rules at the bottom of the file (after any existing browser-related rules):

```css
/* ── Preset browser header row ──────────────────────────────────────────── */

.browser-header {
    display: grid;
    grid-template-columns: 1fr 72px 72px 140px 40px 30px;
    gap: 8px;
    padding: 4px 12px;
    font-size: 9px;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    font-weight: 600;
    color: rgba(0, 0, 0, 0.55);
    border-bottom: 1px solid rgba(0, 0, 0, 0.10);
    user-select: none;
}

.browser-header .sortable {
    cursor: pointer;
    transition: color 0.1s ease;
}

.browser-header .sortable:hover {
    color: rgba(0, 0, 0, 0.75);
}

.browser-header .sortable.active {
    color: rgba(0, 0, 0, 0.85);
}

.browser-header .static {
    cursor: default;
    color: rgba(0, 0, 0, 0.40);
}

.browser-header .arrow {
    opacity: 0.75;
    margin-left: 2px;
    font-size: 8px;
}

/* Thumbnail sits inside a grid cell — let mouse events pass through to the row. */
.browser-row svg.browser-shape {
    display: block;
    width: 100%;
    height: 26px;
    pointer-events: none;
}
```

- [ ] **Step 2: Build and reload**

```bash
cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel
```

Expected: clean build. The BinaryData pipeline picks up `styles.css` automatically.

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/styles.css
git commit -m "style(browser): add header row and sortable-span styles"
```

---

### Task 6: Restructure `renderBrowserList` — header, thumbnail column, skip column

This task lands the new layout visually. Sort interactivity comes in Task 7; for now, all headers render but clicking them does nothing.

**Files:**
- Modify: `Source/WebUI/preset-system.js`

- [ ] **Step 1: Replace `renderBrowserList` with the new layout**

Open `Source/WebUI/preset-system.js`. Find `renderBrowserList` (around line 602) and replace the whole function with the version below. The changes are:

1. Write a `.browser-header` row above the list
2. Change each row's grid to `1fr 72px 72px 140px 40px 30px`
3. Add an inline `<svg class="browser-shape">` cell between Designer and Skip
4. Add a Skip cell (em-dash when zero)
5. After setting innerHTML, walk the rows and call `PresetSpectrum.render` on each thumbnail SVG

```javascript
function renderBrowserList(searchTerm) {
    const listDiv = document.getElementById('browser-list');
    if (!listDiv) return;
    const cache = state.presetsCache || {};
    const q = (searchTerm || '').toLowerCase().trim();

    // Flatten + filter
    const rows = [];
    for (const [packName, list] of Object.entries(cache)) {
        for (const p of list) {
            if (browserFilter === 'favorites' && !p.metadata.isFavorite) continue;
            if (browserFilter !== 'all' && browserFilter !== 'favorites' && packName !== browserFilter) continue;
            if (q && !p.metadata.name.toLowerCase().includes(q)) continue;
            rows.push({ pack: packName, meta: p.metadata, preview: p.preview });
        }
    }

    updateBrowserCount(rows.length);

    // Header row (always rendered, even when list is empty — keeps the chrome consistent)
    const headerHtml = `
        <div class="browser-header">
            <span data-sort="name" class="sortable active">Name <span class="arrow">↑</span></span>
            <span data-sort="type" class="sortable">Type</span>
            <span data-sort="designer" class="sortable">Designer</span>
            <span data-sort="shape" class="sortable">Shape</span>
            <span data-sort="skip" class="sortable">Skip</span>
            <span data-sort="heart" class="sortable" title="Sort by favorites">♥</span>
        </div>
    `;

    if (rows.length === 0) {
        listDiv.innerHTML = headerHtml +
            '<div style="padding: 16px; color: rgba(0,0,0,0.50); text-align: center; font-size: 11px;">No presets found</div>';
        return;
    }

    const rowsHtml = rows.map(r => {
        const isCurrent = r.meta.name === state.currentName && r.pack === state.currentPack;
        const bg = isCurrent ? 'rgba(0,0,0,0.10)' : 'transparent';
        const heart = r.meta.isFavorite ? '♥' : '♡';
        const heartCol = r.meta.isFavorite ? '#c74a4a' : 'rgba(0,0,0,0.40)';
        const skipVal = (r.preview && r.preview.skip) ? String(r.preview.skip) : '—';
        return `
            <div class="browser-row" data-name="${escapeAttr(r.meta.name)}" data-pack="${escapeAttr(r.pack)}"
                 style="display: grid; grid-template-columns: 1fr 72px 72px 140px 40px 30px; gap: 8px; padding: 6px 12px; background: ${bg}; border-radius: 3px; align-items: center; cursor: pointer; font-size: 11px; margin-bottom: 2px;">
                <div style="color: rgba(0,0,0,0.85); font-weight: 500;">${escapeHtml(r.meta.name)}</div>
                <div style="color: rgba(0,0,0,0.60);">${escapeHtml(r.meta.type || '')}</div>
                <div style="color: rgba(0,0,0,0.60);">${escapeHtml(r.meta.designer || '')}</div>
                <svg class="browser-shape" viewBox="0 0 170 26" preserveAspectRatio="none"></svg>
                <div style="color: rgba(0,0,0,0.60); font-variant-numeric: tabular-nums;">${skipVal}</div>
                <div class="browser-heart" style="color: ${heartCol}; text-align: center; cursor: pointer;">${heart}</div>
            </div>
        `;
    }).join('');

    listDiv.innerHTML = headerHtml + rowsHtml;

    // Populate each thumbnail SVG now that the DOM exists.
    listDiv.querySelectorAll('.browser-row').forEach((row, i) => {
        const svg = row.querySelector('.browser-shape');
        if (svg && rows[i].preview && window.PresetSpectrum) {
            window.PresetSpectrum.render(svg, rows[i].preview, { variant: 'thumbnail' });
        }

        const name = row.getAttribute('data-name');
        const pack = row.getAttribute('data-pack');
        row.addEventListener('click', () => loadPreset(name, pack));
        row.addEventListener('mouseover', () => {
            const rn = row.getAttribute('data-name');
            const rp = row.getAttribute('data-pack');
            const isCur = rn === state.currentName && rp === state.currentPack;
            if (!isCur) row.style.background = 'rgba(0,0,0,0.06)';
            updatePreview(rn, rp);
        });
        row.addEventListener('mouseout', () => {
            const rn = row.getAttribute('data-name');
            const rp = row.getAttribute('data-pack');
            const isCur = rn === state.currentName && rp === state.currentPack;
            row.style.background = isCur ? 'rgba(0,0,0,0.10)' : 'transparent';
        });

        const heart = row.querySelector('.browser-heart');
        heart.addEventListener('click', async (e) => {
            e.stopPropagation();
            const entry = flatPresetList().find(p => p.name === name && p.pack === pack);
            const newFav = entry ? !entry.meta.isFavorite : true;
            await setFavorite(name, pack, newFav);
            renderBrowserList(document.getElementById('browser-search').value);
        });
    });
}
```

Nothing else in the file changes in this task. `flatPresetList` already propagates `p.preview` (it was updated in the previous feature's Task 7), so the filtered `rows` array's `preview` field is already populated.

- [ ] **Step 2: Build and smoke-test**

```bash
cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel
```

Expected: clean build. If you load the plugin in Ableton and open the browser, you should see the new header row, six-column grid, thumbnails rendering per preset, and Skip column populated. Clicking headers does nothing yet — that's Task 7.

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/preset-system.js
git commit -m "feat(ui): restructure browser list with header, thumbnails, skip column"
```

---

### Task 7: Add sort state, comparator, and click handlers

**Files:**
- Modify: `Source/WebUI/preset-system.js`

- [ ] **Step 1: Add module-scope sort state and comparator**

At the top of the module scope (near other `const` declarations like `state`, `native`, `el`, around the top of the IIFE), add:

```javascript
// Sort state for the preset browser list. Persists for the lifetime of the
// editor window; resets to defaults when the plugin is re-opened.
const browserSort = { column: 'name', dir: 'asc' };

// Weighted-mean peak frequency in Hz — used by the Shape column sort.
// Uses the same effective fundamental the renderer uses, so skip and
// crossover both feed in. Returns 0 for presets with no harmonic content
// (they sort to the bottom in ascending order).
function spectralCentroid(preview) {
    if (!preview || !Array.isArray(preview.h)) return 0;
    const fund = Math.max(0.01, (preview.crossover || 120) / Math.pow(2, preview.skip || 0));
    let num = 0, den = 0;
    for (let i = 0; i < 7; ++i) {
        const w = preview.h[i] || 0;
        num += w * (i + 2) * fund;
        den += w;
    }
    return den > 0 ? num / den : 0;
}

function compareRows(a, b) {
    const dir = browserSort.dir === 'asc' ? 1 : -1;
    const col = browserSort.column;
    const byName = a.meta.name.localeCompare(b.meta.name); // ascending tiebreaker

    if (col === 'name')     return dir * byName;
    if (col === 'type')     return (dir * (a.meta.type || '').localeCompare(b.meta.type || '')) || byName;
    if (col === 'designer') return (dir * (a.meta.designer || '').localeCompare(b.meta.designer || '')) || byName;
    if (col === 'skip')     return (dir * ((a.preview?.skip ?? 0) - (b.preview?.skip ?? 0))) || byName;
    if (col === 'shape')    return (dir * (spectralCentroid(a.preview) - spectralCentroid(b.preview))) || byName;
    if (col === 'heart') {
        // Ascending = favorites on top. (true should compare as "less than" false in asc order.)
        const av = a.meta.isFavorite ? 0 : 1;
        const bv = b.meta.isFavorite ? 0 : 1;
        return (dir * (av - bv)) || byName;
    }
    return byName;
}
```

Place these right after the existing `const state = { ... }` block near the top of the IIFE. They're module-scope helpers.

- [ ] **Step 2: Apply the sort inside `renderBrowserList`**

Still in `renderBrowserList`, after the filter pass but before computing `rowsHtml`, sort the `rows` array:

```javascript
// ... existing filter pass builds `rows` ...

updateBrowserCount(rows.length);

rows.sort(compareRows);   // NEW

// Header row (always rendered, even when list is empty — keeps the chrome consistent)
const headerHtml = `
    ...
```

- [ ] **Step 3: Make the header reflect the active sort**

Replace the hard-coded `active` class and arrow glyph in the header HTML. The header building block inside `renderBrowserList` should read the current `browserSort` state. Replace the `headerHtml` construction with:

```javascript
const headerCols = [
    { id: 'name',     label: 'Name' },
    { id: 'type',     label: 'Type' },
    { id: 'designer', label: 'Designer' },
    { id: 'shape',    label: 'Shape' },
    { id: 'skip',     label: 'Skip' },
    { id: 'heart',    label: '♥', title: 'Sort by favorites' },
];
const arrow = browserSort.dir === 'asc' ? '↑' : '↓';
const headerHtml = `
    <div class="browser-header">
        ${headerCols.map(c => {
            const active = (browserSort.column === c.id);
            const cls = 'sortable' + (active ? ' active' : '');
            const titleAttr = c.title ? ` title="${escapeAttr(c.title)}"` : '';
            const arrowSpan = active ? ` <span class="arrow">${arrow}</span>` : '';
            return `<span data-sort="${c.id}" class="${cls}"${titleAttr}>${c.label}${arrowSpan}</span>`;
        }).join('')}
    </div>
`;
```

- [ ] **Step 4: Wire up the header click handlers**

After `listDiv.innerHTML = headerHtml + rowsHtml;` and the `querySelectorAll('.browser-row')` pass, add a second `querySelectorAll` for the headers:

```javascript
listDiv.querySelectorAll('.browser-header .sortable').forEach(span => {
    span.addEventListener('click', () => {
        const col = span.getAttribute('data-sort');
        if (!col) return;
        if (browserSort.column === col) {
            browserSort.dir = browserSort.dir === 'asc' ? 'desc' : 'asc';
        } else {
            browserSort.column = col;
            browserSort.dir = 'asc';
        }
        renderBrowserList(document.getElementById('browser-search').value);
    });
});
```

- [ ] **Step 5: Build**

```bash
cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel
```

Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/preset-system.js
git commit -m "feat(ui): click-to-sort preset browser columns"
```

---

### Task 8: Pass `variant: 'preview'` to the preview-card renderer

Explicit is better than implicit at this call site, and it makes grepping for variant usage easy.

**Files:**
- Modify: `Source/WebUI/preset-system.js`

- [ ] **Step 1: Update the `updatePreview` call to `PresetSpectrum.render`**

In `updatePreview` (around line 669), find the existing render call:

```javascript
const svg = preview.querySelector('.preview-spectrum');
if (svg && entry.preview && window.PresetSpectrum) {
    window.PresetSpectrum.render(svg, entry.preview);
}
```

Add the third argument:

```javascript
const svg = preview.querySelector('.preview-spectrum');
if (svg && entry.preview && window.PresetSpectrum) {
    window.PresetSpectrum.render(svg, entry.preview, { variant: 'preview' });
}
```

- [ ] **Step 2: Build**

```bash
cmake --build build --config RelWithDebInfo --target Phantom_VST3 --parallel
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add Source/WebUI/preset-system.js
git commit -m "refactor(ui): pass variant explicitly to preview-card renderer"
```

---

### Task 9: Manual verification in Ableton Live 12

No code changes. Verifies the feature end-to-end in the real host.

- [ ] **Step 1: Quit and restart Ableton**

Per project convention, Ableton caches VST3 DLLs. Clean quit + restart picks up the new build.

- [ ] **Step 2: Open the plugin and the preset browser**

Load Phantom on a track. Open the browser ("Library" button in the header).

- [ ] **Step 3: Header row and thumbnails**

Expect a six-column header (Name ↑ | Type | Designer | Shape | Skip | ♥) above the list. Every row has an inline spectrum thumbnail between Designer and Skip. Skip cell shows "—" for presets with skip=0, and integers 1..8 for higher values.

- [ ] **Step 4: Skip-driven octave shift**

Save a preset with crossover=120Hz and skip=0 → note where the peaks sit in the thumbnail. Adjust the same preset to skip=2 and save under a new name. Compare the thumbnails: the skip=2 version's peaks should sit about two octaves to the left. Higher skip values continue the shift.

- [ ] **Step 5: SUB BASS guard**

Save a preset with crossover=50Hz and skip=4 → effective fundamental is ~3.1 Hz, below the 6 Hz cutoff. The thumbnail should show a muted "SUB BASS" label instead of a curve. Hover the row: the preview card also shows the big "SUB BASS" label.

- [ ] **Step 6: Sort columns**

- Click **Name** → arrow flips to ↓, list reverses alphabetically. Click again → back to ↑.
- Click **Type** → arrow moves to Type, list sorts by type alphabetically; presets with the same type are alphabetical by name.
- Click **Designer** → sort by designer.
- Click **Shape** → presets with left-heavy (low-frequency) shapes come first in ascending order. Click again → right-heavy shapes first.
- Click **Skip** → ascending puts skip=0 presets first; descending puts skip=8 first.
- Click **♥** → favorites rise to the top. Click again → favorites sink to the bottom.

- [ ] **Step 7: Filter + sort interaction**

Select the "Favorites" filter tab, then change sort → only favorites appear, sort applies within that filtered set. Switch the filter back to "All" — sort column persists.

- [ ] **Step 8: Row interaction still works**

Click a row → preset loads. Hover a row → preview card appears with full detail (including skip-shift applied to the bigger curve). Click the heart on a row → toggles favorite, list re-renders, sort preserved.

- [ ] **Step 9: Window resize**

Drag the plugin window wider and narrower. Thumbnails rescale smoothly via `preserveAspectRatio="none"`. Header columns stay aligned with row columns.

- [ ] **Step 10: Console check**

Right-click → Inspect. Confirm zero red errors while hovering, sorting, loading.

- [ ] **Step 11: No commit unless a fix was needed**

If everything passes, no code changed — skip the commit. If any bug surfaced during verification, fix it, commit with `fix(browser): <specific issue>`, and re-run the relevant steps above.

---

## Self-Review

Spec coverage:
- §1 Summary → Tasks 1–8 implement everything listed
- §3 Data Source → Tasks 1, 2, 3 cover struct, extraction, JSON, JS pass-through
- §4 Rendering → Task 4 covers skip shift, SUB BASS guard, variant option
- §5 Browser List Layout → Tasks 5, 6 (CSS + restructure) cover header, grid, thumbnail cell, Skip cell
- §6 Sort State → Task 7 covers state object, click handler, comparator, filter interaction
- §7 Preview-Card Consistency → Task 8 passes variant explicitly
- §8 Integration Points → each file touched has a matching task
- §9 Edge Cases → handled by renderer (legacy skip=0, max skip=8 sub-bass, out-of-range crossover), comparator (missing preview, zero harmonics), and row HTML (missing skip → "—")
- §10 Testing → Task 2 extends unit tests, Task 9 covers manual verification
- §11 Out of Scope → deliberately not implemented

Placeholder scan: no TBD/TODO/vague instructions; every step has concrete code or exact commands.

Type consistency:
- `PreviewData { float h[7]; float crossover; int skip; }` consistent across Tasks 1, 2, 3
- `render(svgEl, preview, options)` signature consistent in Tasks 4 and 8
- `browserSort = { column, dir }` consistent in Task 7
- `spectralCentroid(preview)` and `compareRows(a, b)` names match between declaration and usage
- `flatPresetList` already passes `preview` through (confirmed from the previous feature's Task 7)
