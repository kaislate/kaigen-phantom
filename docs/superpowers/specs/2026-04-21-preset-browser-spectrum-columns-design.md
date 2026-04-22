# Preset Browser — Spectrum Thumbnails, Skip Column, Sortable Headers

**Date:** 2026-04-21
**Status:** Approved for implementation planning
**Scope:** Morning-sized feature. Builds on the preset recipe spectrum preview (PR #5).

---

## 1. Summary

Turn the preset browser's in-list view into a real sortable table:

- **Spectrum thumbnail column** — a compact monochrome curve per row, stripped of axis labels
- **Skip column** — shows the preset's `SYNTH_SKIP` integer (0–8); renders as "—" when 0
- **Sortable headers** — click-to-sort on every column: Name, Type, Designer, Shape (by spectral centroid), Skip, ♥ (favorites first)
- **Skip-driven shift** — the rendered curve's fundamental is `crossover ÷ 2^skip`, so higher skip pushes the whole shape one octave left per step
- **SUB BASS tag** — when the effective fundamental drops below 6 Hz, both the thumbnail and the preview card replace the curve with a big muted "SUB BASS" label

All changes build on `PreviewData` and `preset-spectrum.js` from the prior feature.

## 2. Motivation

The hover-only preview card added value for *examining a single preset*, but browsing still reads as a flat text list. A spectrum thumbnail per row lets users compare shapes at a glance — the intended payoff of that work. Two additional factors matter for phantom specifically:

- **`SYNTH_SKIP` is a powerful, often-used tonal knob** that the current preview ignores. A preset with skip=3 sounds an octave-plus lower than its crossover implies. Surfacing this in the list column *and* in the curve position makes the preset's tonal region instantly legible.
- **Browsing order currently can't be changed**, which hurts as the library grows. Click-to-sort is a table-browser convention users expect.

The SUB BASS tag handles the corner case where `2^skip` pushes peaks off the log-frequency display — giving a semantic label (more informative than an empty graph) without having to design per-preset adaptive zoom.

## 3. Data Source

### 3.1 Extend `PreviewData`

```cpp
struct PreviewData
{
    float h[7]       {};
    float crossover  = 120.0f; // phantom_threshold in Hz (matches APVTS default)
    int   skip       = 0;      // synth_skip, 0..8 (matches APVTS default)
};
```

`int` is the natural type — `SYNTH_SKIP` has a stepped `NormalisableRange<float>(0.0f, 8.0f, 1.0f)` and the consuming code always treats it as an integer count.

### 3.2 Extraction

`PresetManager::readPreviewFromState` is extended to pick up one more parameter ID:

```cpp
// inside the child-walk
if (id == ParamID::SYNTH_SKIP)
{
    data.skip = juce::jlimit(0, 8, juce::roundToInt(value));
    continue;
}
```

`roundToInt` handles the floating-point representation; `jlimit` clamps to the valid range in case of corrupted presets. No normalization by 100 (unlike the h values) — skip is not a percentage.

### 3.3 JSON serialization

`PluginEditor.cpp`'s `getAllPresets` lambda gains one line in the preview builder:

```cpp
preview->setProperty("skip", juce::var(p.preview.skip));
```

Resulting JSON:

```json
"preview": {
    "h":        [0.75, 0.35, 0.60, 0.20, 0.45, 0.15, 0.30],
    "crossover": 60.0,
    "skip":     1
}
```

### 3.4 JS pass-through

`flatPresetList()` currently carries `{ name, pack, meta, preview }` — no change needed, since `preview` is already passed as an object and now includes `skip`. The renderer and any sort comparator read `entry.preview.skip` directly.

## 4. Rendering

### 4.1 Skip-driven frequency shift

`preset-spectrum.js:evaluate` currently uses `fundamentalHz = preview.crossover`. Change to:

```js
const fundamentalHz = Math.max(0.01, (preview.crossover || 80) / Math.pow(2, preview.skip || 0));
```

That single line implements the octave-per-step shift. Everything else in `evaluate` is unchanged; the seven peak centers at `(i+2) × fundamentalHz` propagate the shift automatically.

### 4.2 SUB BASS guard

Inside `render`, before building the curve path:

```js
const fundamentalHz = Math.max(0.01, (preview.crossover || 80) / Math.pow(2, preview.skip || 0));

if (fundamentalHz < 6.0)
{
    renderSubBassLabel(svgEl, options);
    return;
}
```

`renderSubBassLabel` emits a single `<text>` element centered in the viewBox. ViewBox dimensions follow the variant so the label sits correctly in both the small thumbnail and the larger preview card:

```js
function renderSubBassLabel(svgEl, variant) {
    const isThumbnail = variant === 'thumbnail';
    const W = isThumbnail ? 170 : 280;
    const H = isThumbnail ? 26 : 56;
    const fontSize = isThumbnail ? 10 : 20;
    const spacing = isThumbnail ? '0.08em' : '0.15em';
    svgEl.setAttribute('viewBox', `0 0 ${W} ${H}`);
    svgEl.setAttribute('preserveAspectRatio', 'none');
    svgEl.innerHTML = `
        <text x="${W/2}" y="${H/2 + fontSize/3.5}" text-anchor="middle"
              font-size="${fontSize}" font-weight="700" letter-spacing="${spacing}"
              fill="rgba(0,0,0,0.22)"
              font-family="'Space Grotesk', system-ui, sans-serif">SUB BASS</text>
    `;
}
```

### 4.3 Variant option

The function signature becomes `render(svgEl, preview, options = {})` — the default empty object lets existing callers (which pass no third arg) continue to work, with variant implicitly falling back to `'preview'`. Two variants:

- `options.variant === 'preview'` (default, matches today's call site): full detail — axis labels, crossover Hz label, guide lines, curve
- `options.variant === 'thumbnail'`: stripped — curve + thin dashed crossover tick only; no axis labels, no Hz text, no guide lines

Rationale: the two variants share almost all their code (curve math, path building, SUB BASS guard, crossover-x computation), so one function with a mode flag is cleaner than two separate functions or two files. Implementation flips four conditional blocks:

```js
const isThumbnail = options.variant === 'thumbnail';

// Guide lines
const guideLines = isThumbnail ? '' : [14, 28, 42].map(...).join('');

// Crossover marker: always present if crossover is in-range, but
// the Hz label only renders on the 'preview' variant.
const xoverLabel = isThumbnail ? '' :
    `<text x="${(xoverX + 2).toFixed(2)}" ...>${esc(hzLabel)}</text>`;

// Axis labels
const axisLabelsSvg = isThumbnail ? '' : axisLabels.map(...).join('');

// Curve stroke width — a touch thinner at thumbnail scale
const curveStroke = isThumbnail ? 1.0 : 1.2;
```

The thumbnail variant also uses a smaller viewBox aspect ratio — `0 0 170 26` — matching the list-row height. `preserveAspectRatio="none"` continues to let the SVG scale to whatever width the column grants.

### 4.4 Display range

The display range stays fixed at **30 Hz → 1 kHz** across all non-sub-bass presets, for both variants. With the fixed frame, visual comparison between two rows is precise. When the sub-bass guard trips, the curve isn't rendered anyway.

## 5. Browser List Layout

### 5.1 Header row

A new `<div class="browser-header">` rendered once above the list, inside `renderBrowserList`:

```html
<div class="browser-header" style="display: grid;
     grid-template-columns: 1fr 72px 72px 140px 40px 30px;
     gap: 8px; padding: 4px 12px;
     font-size: 9px; text-transform: uppercase; letter-spacing: 0.06em;
     font-weight: 600; color: rgba(0,0,0,0.55);
     border-bottom: 1px solid rgba(0,0,0,0.10);">
    <span data-sort="name" class="sortable active">Name <span class="arrow">↑</span></span>
    <span data-sort="type" class="sortable">Type</span>
    <span data-sort="designer" class="sortable">Designer</span>
    <span data-sort="shape" class="sortable">Shape</span>
    <span data-sort="skip" class="sortable">Skip</span>
    <span data-sort="heart" class="sortable" title="Sort by favorites">♥</span>
</div>
```

Styles for `.sortable` add `cursor: pointer; user-select: none;`. Hover state: `color: rgba(0,0,0,0.75)`. `.active` highlights the currently-sorted column in full black. The arrow (`↑` or `↓`) appears only inside the active header span.

For the ♥ header, since the glyph itself is the label, the sort arrow renders to the *right* of the heart when that column is active (e.g., `♥ ↑`). The `title="Sort by favorites"` attribute provides a tooltip so the interaction is discoverable despite the glyph-only header.

### 5.2 Row structure

Each row updates to the matching grid:

```html
<div class="browser-row" data-name="..." data-pack="..."
     style="display: grid;
            grid-template-columns: 1fr 72px 72px 140px 40px 30px;
            gap: 8px; padding: 6px 12px; ...">
    <div class="name">Bright Lead</div>
    <div class="type">Synth</div>
    <div class="designer">Kaigen</div>
    <svg class="browser-shape" viewBox="0 0 170 26"
         preserveAspectRatio="none"
         style="width: 100%; height: 26px; display: block;"></svg>
    <div class="skip">—</div>
    <div class="heart">♡</div>
</div>
```

After `innerHTML` is written, a second pass populates each `<svg class="browser-shape">` via `PresetSpectrum.render(svg, entry.preview, { variant: 'thumbnail' })`.

Skip cell content: `entry.preview.skip === 0 ? '—' : String(entry.preview.skip)`. The em-dash reads as "not engaged" without adding visual weight.

### 5.3 Column widths

Fixed widths in px for Type/Designer/Shape/Skip/♥ keep the columns aligned across rows. Name takes remaining space (`1fr`). Total min-width of fixed columns: 72+72+140+40+30 + 5×8 gaps = 394 px. The existing browser modal is comfortably wider.

### 5.4 Row hover and selection

Unchanged from today — mouseover tints the row and calls `updatePreview`, click calls `loadPreset`. The thumbnail SVG must not block these events: add `pointer-events: none` to the shape SVG so clicks pass through to the parent row.

## 6. Sort State

### 6.1 State object

A new module-scope variable in `preset-system.js`:

```js
const browserSort = { column: 'name', dir: 'asc' };
```

Persists for the lifetime of the editor window; resets to defaults when the plugin is re-opened. Out of scope: persistence across restarts, per-pack sort memory.

### 6.2 Click handler

Inside `renderBrowserList`, after writing the header:

```js
listDiv.parentElement.querySelectorAll('.browser-header .sortable').forEach(span => {
    span.addEventListener('click', () => {
        const col = span.getAttribute('data-sort');
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

Two-state toggle per column (asc ↔ desc). No unsorted state — first click on a fresh column is always ascending.

### 6.3 Comparator

Secondary tie-break by name ascending keeps ordering stable when the primary column ties (e.g., many presets with Type = Synth). The chained `||` returns the first non-zero comparison; if both names also tie, final fallback is 0.

```js
function spectralCentroid(preview) {
    // Weighted-mean peak frequency in Hz. Uses the same effective
    // fundamental the renderer uses, so skip and crossover both feed in.
    // Returns 0 for presets with no harmonic content.
    if (!preview || !Array.isArray(preview.h)) return 0;
    const fund = Math.max(0.01, (preview.crossover || 80) / Math.pow(2, preview.skip || 0));
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

**Shape sort semantics.** Spectral centroid gives each preset a single representative Hz value — the weighted mean of its peak frequencies. Ascending orders left-heavy shapes (deep / sub-bass / narrow) first; descending puts right-heavy shapes (bright / top-loaded) first. High-skip presets naturally sort toward the low end because their effective fundamental is reduced. A preset with all harmonics at zero weight gets centroid 0 and sorts to the very bottom (ascending) — an acceptable degenerate case.

**Heart sort semantics.** Ascending = favorites first (the common expectation for a "favorites" column). Descending = non-favorites first, favorites at the bottom. Within each group, alphabetical.

### 6.4 Filter interaction

Filter tabs (`browserFilter` — all / favorites / pack name) still control *which* rows render. Sort runs after the filter pass. The two are fully orthogonal. Clearing or changing the filter does not reset the sort; changing the sort does not affect the filter.

## 7. Preview-Card Consistency

The existing preview card (shown on hover) also needs the skip-shift and SUB BASS behavior applied. This is a single-line change at the call site:

```js
// preset-system.js:updatePreview
window.PresetSpectrum.render(svg, entry.preview, { variant: 'preview' });
```

Passing `variant: 'preview'` is explicit for clarity even though it's the default. The underlying renderer already applies the skip shift and sub-bass guard (§4.1, §4.2), so no additional code at the preview-card call site.

## 8. Integration Points

| File | Change |
|------|--------|
| `Source/PresetManager.h` | Add `int skip = 0;` to `PreviewData` |
| `Source/PresetManager.cpp` | Extend `readPreviewFromState` to read `SYNTH_SKIP` (rounded int, clamped 0..8) |
| `Source/PluginEditor.cpp` | `getAllPresets` JSON gains `preview.skip` |
| `tests/PresetPreviewTests.cpp` | Extend the three test cases with `skip` assertions |
| `Source/WebUI/preset-spectrum.js` | Apply `2^skip` shift, SUB BASS guard, add `options.variant` for thumbnail vs preview |
| `Source/WebUI/preset-system.js` | Header row + row restructure + `browserSort` + comparator + click handlers + thumbnail render call |
| `Source/WebUI/styles.css` | `.browser-header`, `.sortable`, `.active`, `.arrow` styles |
| `Source/WebUI/index.html` | No changes expected |
| `CMakeLists.txt` | No changes expected |

## 9. Edge Cases

| Case | Behavior |
|------|----------|
| Legacy preset with no `SYNTH_SKIP` | Falls back to 0, list shows "—", curve renders normally |
| `SYNTH_SKIP` = 8 (max) | Fundamental = crossover/256, almost always triggers SUB BASS |
| Empty preset list after filter | Show existing "No presets found" message (header row still renders — cheap to keep) |
| Sort on Type with many ties | Secondary sort by name ascending |
| Sort on Skip with many zeros | Secondary sort by name ascending |
| Preview card for a sub-bass preset | Shows the big SUB BASS label in the preview card (§4.2) |
| Thumbnail for a sub-bass preset | Shows the small SUB BASS label in the list row |
| Crossover outside display range | Dashed marker suppressed; curve still renders (no Hz label in thumbnail either way) |

## 10. Testing Strategy

### 10.1 Unit tests

`tests/PresetPreviewTests.cpp` — extend the three existing cases:

```cpp
// In the "all seven harmonic weights" case, add:
{ ParamID::SYNTH_SKIP, 3.0f },
...
REQUIRE(preview.skip == 3);

// In the "defaults when missing" case, add:
REQUIRE(preview.skip == 0);

// In the "partial params" case, add a skip input and assertion.
```

Also add one new test case:

```cpp
TEST_CASE("readPreviewFromState clamps skip to valid range")
{
    auto tooHigh = makeState({ { ParamID::SYNTH_SKIP, 42.0f } });
    REQUIRE(PresetManager::readPreviewFromState(tooHigh).skip == 8);

    auto negative = makeState({ { ParamID::SYNTH_SKIP, -3.0f } });
    REQUIRE(PresetManager::readPreviewFromState(negative).skip == 0);
}
```

### 10.2 Manual verification in Ableton

1. Header row visible above the list; click Name → arrow flips to ↓, list reverses; click Type → arrow moves there, list resorts.
2. Thumbnail renders for every row, matches the preview-card shape when you hover that row.
3. Save a preset with skip=2 and crossover=120 → thumbnail peaks sit at roughly half the frequency they would at skip=0.
4. Save a preset with skip=5 and crossover=60 → thumbnail shows "SUB BASS"; preview card on hover also shows big SUB BASS.
5. Sort by Skip ascending → zero-skip presets first, high-skip last. Click again → reversed.
6. Sort by Shape → presets with low-frequency-heavy curves come first (ascending). Reverse → bright/top-heavy curves first.
7. Sort by ♥ → favorites appear at the top of the list. Reverse → favorites at the bottom.
8. Window resize → thumbnails rescale cleanly with the row width.
9. No DevTools console errors during any of the above.

## 11. Out of Scope

Deferred to future work:

- Multi-column sort (shift-click to add secondary)
- Persistent sort across plugin restarts
- Alternate shape-sort metrics (dominant-harmonic mode, total-energy mode) — the spectral centroid covers the common case
- Animated thumbnail transitions when sort reorders
- Adaptive per-preset zoom for high-skip presets (superseded by SUB BASS tag)
- Skip shift in the main in-plugin spectrum analyzer (separate future work)

## 12. Future Ideas

- **Apply the same thumbnail style to a pack-card gallery** — small preview per pack showing an aggregate harmonic fingerprint
- **Search beyond name** — filter by type, by tag, by skip range
- **Density indicator per row** — a small "how much is this preset doing" icon next to the skip
- **Click thumbnail to load** — currently only the row click loads; the thumbnail is hit-through for safety
- **Remember last-clicked preset per pack** — restore scroll/selection when returning to a pack
