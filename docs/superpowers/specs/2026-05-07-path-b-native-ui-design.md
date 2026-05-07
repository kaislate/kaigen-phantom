# Path B: Native UI Replacement Design

**Status:** Design — proposes replacing the WebView2-based plugin editor with a native JUCE-based editor.
**Date:** 2026-05-07.
**Branch (during project):** to be created — likely `path-b-native-ui` or similar.

---

## Background

The current editor is built entirely on `juce::WebBrowserComponent` (WebView2 on Windows) with a 7,400-line WebUI codebase across 16 JS/CSS/HTML files. Every UI update crosses a process boundary between the host and a Chromium renderer, and every plugin instance carries its own ~200-400 MB of Chromium overhead plus IPC traffic concentrated through a shared user-data folder.

Empirically, this saturates Ableton Live's UI thread at 2-3 plugin instances. Mouse drag becomes laggy, drag values arrive out of order producing rubber-banding, and at the limit the host becomes unresponsive even though audio + MIDI still work. We invested significant effort in optimizing within the WebView2 architecture (poll-rate reductions, audio-thread FFT migration, lock-free buffers, drag-state guards, foreground-active gating) and reached a tolerable two-instance state — but no amount of optimization changes the fundamental cost of a Chromium-per-instance UI.

Native UI plugins (Vital, Pro-Q, Serum, FabFilter, etc.) routinely run 10+ instances per project without slowing the host because their UI is in-process, paints directly with native graphics APIs, and does no polling.

This spec describes a one-time rewrite of the editor layer to use JUCE-native `juce::Component` rendering, behind a runtime toggle that allows safe migration without removing the WebView2 path until the native path is stable.

## Goals

- **10+ plugin instances per project run smoothly** (the headline goal — this plugin should never be the one slowing a session).
- **Visual identity preserved** — users wouldn't recognize a redesign happened. The neumorphic knobs, steel-blue accents, dark panel surfaces, recipe wheel, matrix grid all stay.
- **DSP / audio path unchanged** — modulation engine, parameter store, presets, FFT pipeline all stay exactly as they are.
- **Smooth visualizations** — spectrum and oscilloscope keep their fluid look. Frame-rate budget at 60fps for those two specifically.
- **Safe migration** — runtime toggle keeps WebView2 available throughout the project. No big-bang merge.

## Non-goals

- Visual redesign. The look stays as designed.
- Editor window resize support. Current is fixed 1300×970; this is preserved.
- DSP changes. Audio path is already optimized.
- New features. The matrix view as just shipped (PR3b-rev) is the target — no new functionality during the rewrite.
- Cross-platform. Windows-only as today.
- Preset format changes. APVTS state stays compatible.

## High-level approach

A new `NativePluginEditor` class is added alongside the existing WebView2 `PhantomEditor`. `PhantomProcessor::createEditor()` reads a persisted `useNativeEditor` boolean from plugin state and instantiates one or the other. WebView2 stays the default during the rewrite; per-component progress is testable by flipping the toggle and reloading the plugin in any DAW. Once the native editor is stable for a release cycle, the default flips. After 1-2 release cycles in the wild without rollback requests, the WebView2 path is deleted.

The native UI is structured as a hierarchy of `juce::Component` subclasses. Static visuals (knob bodies, panel surfaces, decorative elements) are baked as **SVG assets** authored to match the current CSS-rendered look exactly, bundled via `juce_add_binary_data`, and rendered at runtime via `juce::Drawable::createFromSVG`. Dynamic state (knob indicator angle, ring fill, depth labels, cell colors, modulation glow) is drawn programmatically with `juce::Graphics` on top of the baked layer. The spectrum and oscilloscope use `juce::OpenGLContext` for 60fps animation; everything else uses standard Component painting.

State binding uses JUCE-stock attachment classes (`SliderParameterAttachment`, `ButtonParameterAttachment`, `ComboBoxParameterAttachment`) — no custom relay layer. This eliminates the entire class of bugs we hit with the WebSliderRelay (rubber-banding under load, LINK-mode echo doubling, the `userIsDragging` guard hack) because attachments are synchronous, in-process, and JUCE has battle-tested them.

## Component hierarchy

```
NativePluginEditor                        // top-level; owns OpenGLContext
├── TopBar                                // preset selector, "ADVANCED" toggle button
├── MainArea                              // 920px tall content
│   ├── LeftPanel
│   │   ├── RecipeWheel                   // custom widget — drag spokes for H2..H8
│   │   ├── GhostSection
│   │   │   ├── PhantomKnob[ghost]        // "Amount"
│   │   │   └── GhostCrossoverColumn      // mode toggle + Crossover + Strength
│   │   └── FilterSection                 // LPF + LinkButton + HPF + slope toggle
│   └── RightPanel
│       ├── HarmonicEngineSection         // Saturation, Shape, Skip
│       ├── StereoSection                 // Width
│       ├── LevelsSection                 // MeterIn + In knob + Out knob + MeterOut + Auto button
│       ├── AdvancedPanel                 // collapsible ~150px; 3 sub-sections of mini knobs
│       └── VizSection                    // Oscilloscope + Spectrum (OpenGL)
├── ModulationPanel                       // always-visible bottom panel
│   ├── ModeBar                           // SLOTS / MATRIX toggle, routing counter
│   ├── SlotRow                           // 11 slots (4 macros with knobs, 1 morph, LFO/random placeholders)
│   └── EngineLabels                      // ENGINE A | ENGINE B
└── MatrixView                            // overlay with ComponentAnimator transitions
    ├── ModulatorStrip                    // left fixed-width column with 10 modulator rows
    └── DestinationGrid                   // right side; category groups + cells
```

## Reusable widgets

Each is a `juce::Component` subclass. Listed with key responsibilities:

- **`PhantomKnob`** (large/medium/small variants) — wraps a `juce::Slider`, draws baked-asset body + dynamic indicator + ring overlay. Owns its own `SliderParameterAttachment`.
- **`PhantomMiniKnob`** — smaller variant for the advanced panel; same architecture, smaller asset.
- **`IOMeter`** — vertical bar; reads atomic peak from processor; draws dB-scaled fill + peak hold + clip indicator. Pure Graphics (lightweight enough not to need OpenGL).
- **`ToggleGroup`** — multi-button radio (ghost mode "Replace/Combine/Phantom Only", filter slope "-6/-12/-24", etc.). Wraps an `juce::AudioParameterChoice` attachment.
- **`LinkButton`** — the LPF/HPF link toggle (with the chain-link SVG icon).
- **`AdvancedToggle`** / **`SeamLatch`** — the "ADVANCED" expand/collapse handle.
- **`ModSlot`** — variants: `MacroSlot` (contains a `PhantomKnob`), `MorphSlot` (read-only ring), `LfoPlaceholder`, `RandomPlaceholder`.
- **`MatrixCell`** — the click/drag/right-click cell. Stateful: empty / routed / negative / modulating.
- **`MatrixCategoryHeader`** — expandable column-group header.
- **`MatrixModRow`** — left strip per modulator (icon + name + value).
- **`Popover`** — right-click cell quick-actions menu.
- **`Spectrum`**, **`Oscilloscope`** — OpenGL-backed `Component`s reading audio-thread atomic snapshot arrays.
- **`RecipeWheel`** — custom interactive widget (see "Risk areas" — this is the highest-effort port).
- **`PresetSelector`** — dropdown / browser modal for preset save/load (replaces 967-line `preset-system.js`).

## Rendering strategy

### Two paths

1. **`juce::Graphics`** for everything static or low-frequency: knob bodies, panel surfaces, button shapes, decorative elements, text labels, modulation panel layout, matrix grid lines/borders, IOMeter, popovers. Repaints only when state changes.

2. **`juce::OpenGLContext`** attached to `Spectrum` and `Oscilloscope` only. These need 60fps fluid animation. The OpenGL context renders on its own thread; the audio side updates the existing atomic snapshot arrays (`engineASpectrum`, `engineBSpectrum`, oscilloscope ring buffers). The OpenGL render thread reads them lock-free via `memory_order_relaxed`.

### OpenGL fallback

If `OpenGLContext::makeActive()` fails at editor construction (rare driver issues), `Spectrum` and `Oscilloscope` fall back to `Graphics` rendering at 30fps. Detection happens once at construction; user never sees a runtime failure.

### Asset pipeline (the "D" approach)

Static visuals are baked as **SVG files** authored once. Asset list:

- Knob body — three variants (large / medium / small)
- Mini-knob body
- Panel surface — single SVG, used with 9-slice scaling for variable-size panels
- Button surface — active / inactive variants
- Recipe wheel rim + spoke separators
- Slot dot bases — four type-color variants (macro teal / lfo blue / random purple / morph white)
- Decorative panel-divider lines
- Circuit-board cosmetic background — PNG (too organic for SVG)

What stays paint-code (computed every frame from state):

- Knob indicator angle (line drawn over baked body)
- Knob ring fill (polar `Path` from current value → conic-equivalent)
- Numeric value labels next to knobs
- IOMeter fill height + peak marker
- Spectrum bars (OpenGL)
- Oscilloscope waveform (OpenGL)
- Matrix cell colors + depth labels
- Modulation glow / pulse animations
- Active-state highlights (steel-blue glow on active toggles)
- Recipe wheel spoke positions (drawn over baked rim)

### Asset authoring

Source SVGs live in `Source/Assets/svg/`. CMake bakes them via `juce_add_binary_data` into a new `BinaryData::*Asset` namespace. Color tokens (steel blue, panel surfaces, dot type-colors) live in a single `Source/UI/Theme.h` file referenced both from SVG-rendering code and paint code, so visual changes happen in one place.

### Theme tokens

`Theme.h` defines `juce::Colour` constants matching current CSS values:

```cpp
namespace kaigen::phantom::Theme {
    inline const juce::Colour steelBlue       { 0xff4A90E2 };
    inline const juce::Colour macroTeal       { 0xff5DD3E0 };
    inline const juce::Colour lfoBlue         { 0xff4A90E2 };
    inline const juce::Colour randomPurple    { 0xff9990E0 };
    inline const juce::Colour morphWhite      { 0xffEFEFF2 };
    inline const juce::Colour panelBg         { 0xff0E1116 };
    inline const juce::Colour matrixBg        { 0xff0A0C10 };
    // ...
}
```

## Layout

`juce::FlexBox` and `juce::Grid` for top-level structure (mirrors current CSS Grid/Flexbox). Manual `setBounds()` for tightly-packed clusters where flex feels heavy-handed. All layout in component-level `resized()` callbacks. Editor stays fixed 1300×970 (no resize support).

## State binding

JUCE-stock attachments only:

```cpp
class PhantomKnob : public juce::Component, private juce::Slider::Listener {
    juce::Slider slider;  // hidden; we provide custom paint
    std::unique_ptr<juce::SliderParameterAttachment> attachment;

public:
    PhantomKnob(juce::AudioProcessorValueTreeState& apvts, juce::StringRef paramID) {
        attachment = std::make_unique<juce::SliderParameterAttachment>(
            *apvts.getParameter(paramID), slider);
        slider.addListener(this);  // for repaint on value change
    }
    void paint(juce::Graphics&) override;  // baked body + dynamic indicator
    void mouseDrag(const juce::MouseEvent&) override;  // delegates to slider
    void sliderValueChanged(juce::Slider*) override { repaint(); }
};
```

This eliminates the entire WebSliderRelay echo-loop class of bugs because attachments are synchronous and in-process.

### LINK mode

Per-engine knob pairs (`a_ghost` / `b_ghost`, etc.) need LINK behavior — when on, dragging either writes both. This is currently handled by the `_logicalSliderCache` wrapper in `juce-frontend.js`. Native equivalent: a thin `LinkedKnob` wrapper that owns two `SliderParameterAttachment`s and sets both via `setValueNotifyingHost` when LINK is active. Logic lives in one place; tested with a Catch2 unit test.

### Modulation engine integration

The matrix view's cell click handlers call `processor.getModulationEngineA().addRouting(...)` directly — no JSON, no IPC, no `withNativeFunction` lambdas. Same for `removeRouting`, `setRoutingDepth`, `setMacroName`. Engine focus, link mode, and matrix mode persist via the existing `EngineFocus` / `MatrixViewState` headers (or a new `EditorViewState.h` if the layout shifts).

## Threading & performance model

**Threads:**

1. **Audio thread** — unchanged. ProcessBlock + FFTs (already moved here) + ring buffer fills.
2. **Message thread** — JUCE Component paint, attachment value updates, mouse/keyboard events, layout. All native UI work happens here in-process.
3. **OpenGL render thread** — owned per editor by `juce::OpenGLContext`. Drives spectrum + oscilloscope at 60fps. Reads audio-thread atomic snapshots on each frame.

**No polling timers.** APVTS attachments are push-based — when a parameter changes, the relevant component's `setValue` is called and `repaint()` invalidates the right region. Exception: a 10Hz tick reads live macro values for matrix cell pulse animations. Direct atomic loads, no IPC.

**Animations:**

- Matrix overlay fade/slide: `juce::ComponentAnimator::animateComponent` (240ms tween).
- Slot panel collapse/expand: same — animate the panel's height bound between 150px and 28px.
- Cell pulse on live modulation: 10Hz timer setting per-cell `modPulse` floats and triggering targeted repaints; cell `paint()` reads it and renders the glow.
- Knob/ring updates: handled by attachment → repaint chain, no explicit animation.

**Multi-instance scaling:**

Each editor instance has its own Component tree. Painting one doesn't affect another. No shared Chromium process. Per-instance memory drops from ~300MB (Chromium overhead) to ~30-60MB depending on baked asset size — across 10 instances, ~2.5GB recovered.

## Migration toggle

A `useNativeEditor` boolean is added to plugin state, persisted in `<PluginState>` alongside `EngineFocus` etc. Default initially: `false` (WebView2). User-changeable via a settings affordance (or a JSON field they edit during dev).

```cpp
juce::AudioProcessorEditor* PhantomProcessor::createEditor() {
    if (useNativeEditor.load())
        return new NativePluginEditor(*this, apvts);
    return new PhantomEditor(*this, apvts);  // existing WebView2 editor
}
```

The WebView2 editor is unchanged during the project. Only after native is stable does the default flip; after 1-2 release cycles, WebView2 is deleted.

## Phasing (high-level — actual implementation plans come from writing-plans)

| Phase | Scope | Estimated effort |
|-------|-------|-------|
| 0 — Foundation | `NativePluginEditor` skeleton, runtime toggle, `Theme.h`, asset CMake plumbing, blank panels at correct positions | ~1 week |
| 1 — Knob system | `PhantomKnob`, `PhantomMiniKnob`, `ToggleGroup`, `LinkButton`, baked knob assets, APVTS attachments. Most main-editor controls functional | ~1.5 weeks |
| 2 — Visualizers | `Spectrum`, `Oscilloscope`, `IOMeter`. OpenGL contexts, audio-thread atomic reads. 60fps spectrum + scope | ~1 week |
| 3 — Specialized widgets | `RecipeWheel` (high-risk), `CrossoverColumn`, `FilterRow`, advanced-panel collapse | ~1.5 weeks |
| 4 — Preset system | Native preset browser/save/load replacing `preset-system.js` (967 lines) | ~1.5 weeks |
| 5 — Modulation system | `ModulationPanel`, `MatrixView` with all interactions (click empty, drag, right-click popover, name editor, live pulse, slot-click handoff) | ~2 weeks |
| 6 — Cutover | Switch default to native, multi-instance smoke, after release cycle delete WebView2 + WebUI files + WebView2 SDK CMake fetch | ~3 days |

**Total estimate:** 8-9 weeks single-developer focused work. Phases ship to `master` independently — toggle still defaults to WebView2 throughout, native progressively becomes more complete. Dogfooding possible at any time by flipping the toggle.

## Out of scope

- Visual redesign. Look stays exactly as is.
- DSP changes. Audio path unchanged.
- Editor resizing. Fixed 1300×970 stays.
- New features. Matrix view as shipped is the target.
- Preset format changes. APVTS state stays compatible.
- Cross-platform. Windows-only.

## Risk areas

1. **Recipe wheel (highest)** — currently 299 lines of custom canvas drawing with polar coordinate math, draggable spokes per harmonic, hit-testing within angular slices. Native port needs `Path` drawing for rim/spokes, `mouseDrag` math, careful repaint regions. **Mitigation:** prototype in Phase 3 explicitly. If it overruns the 1.5-week budget significantly, we know early.

2. **Asset-baking workflow** — hand-authoring SVGs that match current CSS-rendered look exactly is a labor cost we haven't measured. **Mitigation:** Phase 0 starts with a single asset (medium knob body) as a proof. If it takes a day, budget holds. If it takes a week, drop to PNG screenshots for harder assets.

3. **OpenGL availability** — driver issues exist for some users. **Mitigation:** documented Graphics-rendering fallback at construction.

4. **LINK mode + modulation interaction** — when LINK is on AND a routing exists, dragging writes both `a_*` and `b_*`, which both get modulated. **Mitigation:** isolate LINK logic in `LinkedKnob` wrapper and unit-test it.

5. **DAW integration edge cases** — VST3 hosts can hot-swap, reparent, open/close in rapid succession. **Mitigation:** dogfood in Live aggressively from Phase 1 onward — toggle makes this trivial.

6. **Visual drift during the project** — tweaking WebView2 UI mid-rewrite causes native to diverge. **Mitigation:** freeze WebView2 visual changes for the project duration, OR commit to porting any visual change into native within 24 hours.

## Testing strategy

- **Visual diffing:** side-by-side screenshots of WebView2 and native at each phase milestone. Catch fidelity drift early.
- **Catch2 unit tests:** add coverage for LINK fan-out logic, matrix-view modulator/destination lookup, asset bundle integrity.
- **Manual smoke per phase:** open plugin, exercise the controls being tested in that phase, verify audio + state persist correctly.
- **Multi-instance smoke (Phase 6):** verify 10+ instances in Live work as expected. Compare CPU/memory against current WebView2 baseline.

## Files affected

### New files (top-level)

```
Source/UI/                            # new directory
├── NativePluginEditor.{h,cpp}
├── Theme.h
├── EditorViewState.h                 # if not folded into existing headers
├── widgets/
│   ├── PhantomKnob.{h,cpp}
│   ├── PhantomMiniKnob.{h,cpp}
│   ├── IOMeter.{h,cpp}
│   ├── ToggleGroup.{h,cpp}
│   ├── LinkButton.{h,cpp}
│   ├── ModSlot.{h,cpp}
│   ├── MatrixCell.{h,cpp}
│   ├── MatrixCategoryHeader.{h,cpp}
│   ├── MatrixModRow.{h,cpp}
│   ├── Popover.{h,cpp}
│   ├── RecipeWheel.{h,cpp}
│   └── PresetSelector.{h,cpp}
├── visualizers/
│   ├── Spectrum.{h,cpp}              # OpenGL-backed
│   └── Oscilloscope.{h,cpp}          # OpenGL-backed
├── panels/
│   ├── TopBar.{h,cpp}
│   ├── LeftPanel.{h,cpp}
│   ├── RightPanel.{h,cpp}
│   ├── ModulationPanel.{h,cpp}
│   └── MatrixView.{h,cpp}
└── Assets/
    ├── svg/                          # hand-authored static visuals
    │   ├── knob_large.svg
    │   ├── knob_medium.svg
    │   ├── knob_small.svg
    │   ├── knob_mini.svg
    │   ├── panel_surface.svg
    │   ├── button.svg
    │   ├── button_active.svg
    │   ├── recipe_wheel.svg
    │   ├── slot_dot_macro.svg
    │   ├── slot_dot_lfo.svg
    │   ├── slot_dot_random.svg
    │   ├── slot_dot_morph.svg
    │   └── divider.svg
    └── png/
        └── circuit_board.png
```

### Modified files

- `Source/PluginProcessor.{h,cpp}` — add `useNativeEditor` to plugin state; modify `createEditor()` to branch on flag.
- `CMakeLists.txt` — add `Source/UI/` source files to the build target; add `Source/UI/Assets/` files to `juce_add_binary_data`.

### Removed files (Phase 6 cutover only)

- All of `Source/WebUI/` (16 files, ~7,400 lines).
- WebView2 SDK fetch in `CMakeLists.txt`.
- `Source/PluginEditor.{h,cpp}` (the WebView2 editor).
- The `juce_add_binary_data` block for WebUI files.

## Self-review notes

- **Spec coverage:** every section asked about during brainstorming is addressed (goals, fidelity strategy, migration, rendering, layout, state, threading, phasing, scope, risks).
- **Placeholder scan:** no "TBD" / "TODO" / vague-language. Every requirement is explicit.
- **Internal consistency:** rendering strategy, asset pipeline, and component decomposition reference each other consistently. Threading model and performance claims are aligned with the rendering choices.
- **Scope check:** this is a single-architecture spec. Each phase will get its own implementation plan (writing-plans skill). The scope is large but coherent — splitting further would fragment foundational decisions.
- **Ambiguity check:** asset authoring workflow and LINK mode handling spelled out. OpenGL fallback condition stated. Testing strategy specific to phases.
