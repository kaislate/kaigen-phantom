# Phantom WebView UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the JUCE 2D UI with an embedded WebView that serves the HTML/CSS/JS mockup as the production UI, with Three.js holographic wheel, CSS neumorphic panels, and Canvas2D spectrum — all wired to the DSP backend via JUCE's relay system and native functions.

**Architecture:** JUCE 8's `WebBrowserComponent` (WebView2 on Windows) fills the 920x620 window. HTML/CSS/JS files are compiled into BinaryData and served via ResourceProvider. 35 APVTS parameters bind through WebSliderRelay/WebComboBoxRelay + WebParameterAttachments. Real-time data (spectrum, peaks, pitch) flows through custom native functions polled at 30fps.

**Tech Stack:** JUCE 8.0.4, C++20, WebView2, HTML/CSS/JS, Three.js 0.160.0, Canvas2D

**Spec:** `docs/superpowers/specs/2026-04-11-phantom-webview-ui-design.md`

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `Source/UI/*` (20 files) | Delete | Old JUCE 2D UI — replaced by WebView |
| `Source/PluginEditor.h` | Rewrite | WebBrowserComponent shell + relays + native functions |
| `Source/PluginEditor.cpp` | Rewrite | Relay construction, resource provider, native function registration |
| `Source/PluginProcessor.h` | Keep | Already has atomics + spectrum (from previous phase) |
| `Source/PluginProcessor.cpp` | Minor edit | Add preset-change listener for writing H2-H8 |
| `Source/Parameters.h` | Keep | Unchanged |
| `CMakeLists.txt` | Rewrite build targets | Remove UI/*.cpp, add juce_gui_extra, add BinaryData |
| `Source/WebUI/index.html` | Create | Main document — full layout structure |
| `Source/WebUI/styles.css` | Create | All neumorphic CSS from mockup v21 |
| `Source/WebUI/knob.js` | Create | Custom `<phantom-knob>` web component |
| `Source/WebUI/phantom.js` | Create | JUCE bridge, relay binding, data polling, mode switching |
| `Source/WebUI/recipe-wheel.js` | Create | Three.js holographic wheel + energy flow |
| `Source/WebUI/spectrum.js` | Create | Canvas2D spectrum analyzer (bar + line modes) |

Three.js will be loaded via importmap from a CDN during development, then bundled for production. We'll use the importmap approach for now.

---

## Phase 1 — C++ Shell + Resource Serving

### Task 1: Delete old UI files and update CMakeLists

**Files:**
- Delete: `Source/UI/` (entire directory — 20 files)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Delete the Source/UI directory**

```bash
rm -rf "Source/UI"
```

- [ ] **Step 2: Create the Source/WebUI directory with a placeholder index.html**

```bash
mkdir -p Source/WebUI
```

Create `Source/WebUI/index.html`:
```html
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: #06060c; color: #fff; font-family: 'Courier New', monospace;
         display: flex; align-items: center; justify-content: center; height: 100vh; }
  h1 { font-size: 24px; font-weight: 300; letter-spacing: 10px;
       text-shadow: 0 0 8px rgba(255,255,255,0.6); }
</style>
</head>
<body>
<h1>PHANTOM</h1>
</body>
</html>
```

- [ ] **Step 3: Rewrite CMakeLists.txt**

Replace the entire file with:

```cmake
cmake_minimum_required(VERSION 3.22)
project(KaigenPhantom VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(KaigenPhantom
    COMPANY_NAME              "Kaigen"
    PLUGIN_MANUFACTURER_CODE  Kgna
    PLUGIN_CODE               Kgph
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "Kaigen Phantom"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Fx" "Tools"
)

juce_generate_juce_header(KaigenPhantom)

target_sources(KaigenPhantom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engines/PitchTracker.cpp
    Source/Engines/HarmonicGenerator.cpp
    Source/Engines/BinauralStage.cpp
    Source/Engines/PerceptualOptimizer.cpp
    Source/Engines/CrossoverBlend.cpp
    Source/Engines/Deconfliction/PartitionStrategy.cpp
    Source/Engines/Deconfliction/SpectralLaneStrategy.cpp
    Source/Engines/Deconfliction/StaggerStrategy.cpp
    Source/Engines/Deconfliction/OddEvenStrategy.cpp
    Source/Engines/Deconfliction/ResidueStrategy.cpp
    Source/Engines/Deconfliction/BinauralStrategy.cpp
)

# Bundle WebUI files into BinaryData
juce_add_binary_data(PhantomWebUI SOURCES
    Source/WebUI/index.html
)

target_include_directories(KaigenPhantom PRIVATE Source)

target_compile_definitions(KaigenPhantom PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(KaigenPhantom
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
        PhantomWebUI
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_subdirectory(tests)
```

Key changes from the old CMakeLists:
- Removed all `Source/UI/*.cpp` entries
- Added `juce_add_binary_data(PhantomWebUI ...)` for bundling web files
- Changed `JUCE_WEB_BROWSER=0` to `JUCE_WEB_BROWSER=1`
- Added `JUCE_USE_WIN_WEBVIEW2=1`
- Added `juce::juce_gui_extra` to link libraries
- Added `PhantomWebUI` to link libraries

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "chore: delete old JUCE 2D UI, set up WebUI directory and BinaryData pipeline"
```

---

### Task 2: Rewrite PluginEditor as WebView shell

**Files:**
- Rewrite: `Source/PluginEditor.h`
- Rewrite: `Source/PluginEditor.cpp`

- [ ] **Step 1: Rewrite PluginEditor.h**

```cpp
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Parameters.h"

struct SinglePageBrowser : juce::WebBrowserComponent
{
    using WebBrowserComponent::WebBrowserComponent;
    bool pageAboutToLoad (const juce::String& newURL) override
    {
        return newURL == getResourceProviderRoot();
    }
};

class PhantomEditor : public juce::AudioProcessorEditor
{
public:
    explicit PhantomEditor (PhantomProcessor&);
    ~PhantomEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    PhantomProcessor& processor;

    SinglePageBrowser webView {
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder (
                        juce::File::getSpecialLocation (juce::File::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withResourceProvider ([this] (const auto& url) { return getResource (url); })
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhantomEditor)
};
```

- [ ] **Step 2: Rewrite PluginEditor.cpp**

```cpp
#include "PluginEditor.h"
#include "BinaryData.h"

static const char* getMimeForExtension (const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { "html", "text/html" },
        { "htm",  "text/html" },
        { "css",  "text/css" },
        { "js",   "text/javascript" },
        { "json", "application/json" },
        { "png",  "image/png" },
        { "jpg",  "image/jpeg" },
        { "svg",  "image/svg+xml" },
        { "woff2","font/woff2" },
    };

    if (const auto it = mimeMap.find (extension.toLowerCase()); it != mimeMap.end())
        return it->second;

    return "application/octet-stream";
}

PhantomEditor::PhantomEditor (PhantomProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (920, 620);
    addAndMakeVisible (webView);
    webView.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

void PhantomEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff06060c));
}

void PhantomEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource> PhantomEditor::getResource (const juce::String& url)
{
    const auto urlToRetrieve = url == "/" ? juce::String { "index.html" }
                                          : url.fromFirstOccurrenceOf ("/", false, false);

    // Map URL path to BinaryData resource name
    // BinaryData converts filenames: "index.html" -> "index_html", "styles.css" -> "styles_css"
    auto resourceName = urlToRetrieve.replace (".", "_").replace ("-", "_").replace ("/", "_");

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (juce::String (BinaryData::namedResourceList[i]) == resourceName)
        {
            int size = 0;
            const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);

            if (data != nullptr && size > 0)
            {
                auto extension = urlToRetrieve.fromLastOccurrenceOf (".", false, false);
                std::vector<std::byte> bytes (reinterpret_cast<const std::byte*> (data),
                                               reinterpret_cast<const std::byte*> (data) + size);
                return juce::WebBrowserComponent::Resource { std::move (bytes),
                                                              juce::String (getMimeForExtension (extension)) };
            }
        }
    }

    return std::nullopt;
}
```

- [ ] **Step 3: Build and verify**

```bash
cmake -B build -G "Visual Studio 17 2022" && cmake --build build --config Release
```

Expected: Plugin builds. Loading it shows "PHANTOM" text in a dark window — the placeholder HTML served through BinaryData via the WebView.

- [ ] **Step 4: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(webui): rewrite PluginEditor as WebView shell with ResourceProvider"
```

---

## Phase 2 — Static Layout + Knob Component

### Task 3: Build the complete static HTML layout and CSS

**Files:**
- Create: `Source/WebUI/index.html` (replace placeholder)
- Create: `Source/WebUI/styles.css`
- Modify: `CMakeLists.txt` (add styles.css to BinaryData)

- [ ] **Step 1: Create styles.css**

Create `Source/WebUI/styles.css` with all the neumorphic CSS from mockup v21. This file contains: reset styles, body background, header (stipple texture, etched text, glow seam), neumorphic panel class, knob volcano/OLED/ridge styles, glow seam dividers, preset strip, spectrum area, and the full grid layout.

The CSS should be extracted and adapted from the `<style>` block in `.superpowers/brainstorm/37941-1775788189/content/mockup-v21.html`. Key changes from the mockup:
- Remove any `html,body` sizing that conflicts with the webview container
- Add `body { margin:0; overflow:hidden; width:920px; height:620px; }` to match the fixed plugin window
- Keep all the neumorphic panel, knob, glow seam, header, and layout styles exactly as they are in the mockup

- [ ] **Step 2: Replace index.html with full layout structure**

Create `Source/WebUI/index.html` with the complete document structure matching the spec layout: header bar with PHANTOM/KAIGEN text and mode/bypass/settings buttons; left column with recipe wheel canvas and preset strip; right column with three rows of knob panels; bottom-right spectrum area with I/O meters. All knobs use `<phantom-knob>` custom elements (defined in knob.js, loaded later). The recipe wheel area has a `<canvas id="wheelCanvas">` placeholder. The spectrum area has `<canvas id="spectrumCanvas">`.

This should be adapted from the mockup v21 HTML structure, removing the Three.js script (that moves to recipe-wheel.js) and the inline spectrum JS (that moves to spectrum.js).

- [ ] **Step 3: Update CMakeLists.txt BinaryData**

```cmake
juce_add_binary_data(PhantomWebUI SOURCES
    Source/WebUI/index.html
    Source/WebUI/styles.css
)
```

- [ ] **Step 4: Build and verify**

Expected: Plugin shows the full static layout — dark panels, header with etched text, knob placeholders (empty circles), spectrum area. Nothing is interactive yet.

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/index.html Source/WebUI/styles.css CMakeLists.txt
git commit -m "feat(webui): add complete static HTML layout and neumorphic CSS"
```

---

### Task 4: Build the custom knob web component

**Files:**
- Create: `Source/WebUI/knob.js`
- Modify: `Source/WebUI/index.html` (add script tag)
- Modify: `CMakeLists.txt` (add to BinaryData)

- [ ] **Step 1: Create knob.js**

Create `Source/WebUI/knob.js` — a custom element `<phantom-knob>` that renders an SVG rotary knob with the neumorphic volcano style from the spec.

Attributes: `name`, `size` ("large"|"medium"), `value` (0-1 normalized), `display-value` (text shown), `label`, `default-value`.

The component renders:
1. SVG volcano circle with radial gradient (bright top-left, dark bottom-right)
2. Black OLED well circle inset from edge
3. Triple-ring lip/ridge (3 SVG circles: bright, dark gap, outer subtle)
4. Dim arc track (full sweep, 270° from 135° to 405°)
5. Bright value arc proportional to `value` attribute
6. Glow halo arc (wider stroke, lower opacity)
7. Value text with multi-layer text-shadow glow
8. Label text below

Interaction:
- `pointerdown` starts drag tracking, `pointermove` maps vertical delta to value change
- `dblclick` resets to `default-value`
- Dispatches `CustomEvent("knob-change", { detail: { name, value } })` on value changes

- [ ] **Step 2: Add script tag to index.html**

Add before `</body>`:
```html
<script type="module" src="/knob.js"></script>
```

- [ ] **Step 3: Update CMakeLists BinaryData**

Add `Source/WebUI/knob.js` to the `juce_add_binary_data` sources list.

- [ ] **Step 4: Build and verify**

Expected: Knobs render with the neumorphic volcano style. Click and drag vertically to change values. Double-click resets. No audio effect yet (not wired to parameters).

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/knob.js Source/WebUI/index.html CMakeLists.txt
git commit -m "feat(webui): add phantom-knob custom web component with SVG rendering"
```

---

## Phase 3 — Parameter Binding

### Task 5: Add relay objects and native functions to PluginEditor

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Add all 35 relay objects and parameter attachments to PluginEditor.h**

Add relay members for every parameter. Use `WebSliderRelay` for float params, `WebComboBoxRelay` for choice params. Add `WebSliderParameterAttachment` / `WebComboBoxParameterAttachment` for each. Add Timer inheritance for real-time data polling. Add the 3 native function registrations to the WebBrowserComponent Options chain.

The relays must be declared BEFORE the webView member (since they're passed to Options via `withOptionsFrom()`). The attachments must be declared AFTER the webView.

```cpp
// In private section, BEFORE webView:

// Slider relays (28 float params)
WebSliderRelay ghostRelay { "ghost" };
WebSliderRelay phantomThresholdRelay { "phantom_threshold" };
WebSliderRelay phantomStrengthRelay { "phantom_strength" };
WebSliderRelay outputGainRelay { "output_gain" };
WebSliderRelay recipeH2Relay { "recipe_h2" };
WebSliderRelay recipeH3Relay { "recipe_h3" };
WebSliderRelay recipeH4Relay { "recipe_h4" };
WebSliderRelay recipeH5Relay { "recipe_h5" };
WebSliderRelay recipeH6Relay { "recipe_h6" };
WebSliderRelay recipeH7Relay { "recipe_h7" };
WebSliderRelay recipeH8Relay { "recipe_h8" };
WebSliderRelay recipePhaseH2Relay { "recipe_phase_h2" };
WebSliderRelay recipePhaseH3Relay { "recipe_phase_h3" };
WebSliderRelay recipePhaseH4Relay { "recipe_phase_h4" };
WebSliderRelay recipePhaseH5Relay { "recipe_phase_h5" };
WebSliderRelay recipePhaseH6Relay { "recipe_phase_h6" };
WebSliderRelay recipePhaseH7Relay { "recipe_phase_h7" };
WebSliderRelay recipePhaseH8Relay { "recipe_phase_h8" };
WebSliderRelay recipeRotationRelay { "recipe_rotation" };
WebSliderRelay harmonicSaturationRelay { "harmonic_saturation" };
WebSliderRelay binauralWidthRelay { "binaural_width" };
WebSliderRelay trackingSensitivityRelay { "tracking_sensitivity" };
WebSliderRelay trackingGlideRelay { "tracking_glide" };
WebSliderRelay staggerDelayRelay { "stagger_delay" };
WebSliderRelay sidechainDuckAmountRelay { "sidechain_duck_amount" };
WebSliderRelay sidechainDuckAttackRelay { "sidechain_duck_attack" };
WebSliderRelay sidechainDuckReleaseRelay { "sidechain_duck_release" };
WebSliderRelay stereoWidthRelay { "stereo_width" };

// ComboBox relays (4 choice params)
WebComboBoxRelay modeRelay { "mode" };
WebComboBoxRelay ghostModeRelay { "ghost_mode" };
WebComboBoxRelay recipePresetRelay { "recipe_preset" };
WebComboBoxRelay deconflictionModeRelay { "deconfliction_mode" };

// Int param treated as slider
WebSliderRelay maxVoicesRelay { "max_voices" };
```

Update the webView Options chain to include `.withOptionsFrom(...)` for ALL relays, plus the 3 native functions:

```cpp
.withNativeFunction("getSpectrumData", [this](const auto&, auto complete) {
    juce::Array<juce::var> bins;
    if (processor.spectrumReady.exchange(false))
        for (int i = 0; i < PhantomProcessor::kSpectrumBins; ++i)
            bins.add(processor.spectrumData[(size_t)i]);
    else
        for (int i = 0; i < PhantomProcessor::kSpectrumBins; ++i)
            bins.add(0.0f);
    complete(bins);
})
.withNativeFunction("getPeakLevels", [this](const auto&, auto complete) {
    auto* obj = new juce::DynamicObject();
    obj->setProperty("inL", (double)processor.peakInL.load(std::memory_order_relaxed));
    obj->setProperty("inR", (double)processor.peakInR.load(std::memory_order_relaxed));
    obj->setProperty("outL", (double)processor.peakOutL.load(std::memory_order_relaxed));
    obj->setProperty("outR", (double)processor.peakOutR.load(std::memory_order_relaxed));
    complete(juce::var(obj));
})
.withNativeFunction("getPitchInfo", [this](const auto&, auto complete) {
    auto* obj = new juce::DynamicObject();
    float hz = processor.currentPitch.load(std::memory_order_relaxed);
    obj->setProperty("hz", (double)hz);
    // Note name calculation
    if (hz > 0.0f) {
        static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        int midi = juce::roundToInt(12.0f * std::log2(hz / 440.0f) + 69.0f);
        int note = ((midi % 12) + 12) % 12;
        int octave = (midi / 12) - 1;
        obj->setProperty("note", juce::String(noteNames[note]) + juce::String(octave));
    } else {
        obj->setProperty("note", "---");
    }
    int presetIdx = (int)processor.apvts.getRawParameterValue(ParamID::RECIPE_PRESET)->load();
    static const char* presetNames[] = {"Warm","Aggressive","Hollow","Dense","Custom"};
    obj->setProperty("preset", juce::String(presetNames[juce::jlimit(0, 4, presetIdx)]));
    complete(juce::var(obj));
})
```

Add parameter attachments AFTER the webView member:

```cpp
// Parameter attachments (connect relays to APVTS)
WebSliderParameterAttachment ghostAttach { *processor.apvts.getParameter(ParamID::GHOST), ghostRelay, nullptr };
// ... one for each of the 35 parameters
```

- [ ] **Step 2: Build and verify**

Expected: Build succeeds. The webview loads with all relays registered. No JS wiring yet.

- [ ] **Step 3: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(webui): add 35 parameter relays, 3 native functions to PluginEditor"
```

---

### Task 6: Build phantom.js — JUCE bridge and parameter wiring

**Files:**
- Create: `Source/WebUI/phantom.js`
- Modify: `Source/WebUI/index.html` (add script tag)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create phantom.js**

Create `Source/WebUI/phantom.js` — the central bridge module that:

1. Imports `getSliderState`, `getComboBoxState`, `getNativeFunction` from JUCE's built-in JS module (available at `window.__JUCE__`)
2. For each `<phantom-knob>` element, looks up its `data-param` attribute, gets the corresponding slider state, and wires bidirectional updates:
   - Relay change → update knob `value` and `display-value` attributes
   - Knob `knob-change` event → call `state.setNormalisedValue()`
3. Wires mode toggle buttons to the `mode` combo state
4. Wires preset strip buttons to `recipe_preset` combo state
5. Wires ghost mode toggle to `ghost_mode` combo state
6. Implements mode switching: listens to `mode` state changes, toggles `display:none` on pitch-tracker-panel vs deconfliction-panel
7. Starts the real-time data polling loop via `requestAnimationFrame`:
   - Calls `getSpectrumData()`, `getPeakLevels()`, `getPitchInfo()`
   - Dispatches custom events that spectrum.js and recipe-wheel.js listen to

- [ ] **Step 2: Add script tag and update BinaryData**

Add to index.html: `<script type="module" src="/phantom.js"></script>`

Add `Source/WebUI/phantom.js` to CMakeLists BinaryData.

- [ ] **Step 3: Build and verify**

Expected: Knobs are now functional — dragging them changes DSP parameters. Mode toggle switches visible panels. Preset buttons change the active preset label.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/phantom.js Source/WebUI/index.html CMakeLists.txt
git commit -m "feat(webui): add phantom.js bridge — parameter binding, mode switching, data polling"
```

---

## Phase 4 — Recipe Wheel

### Task 7: Build recipe-wheel.js with Three.js holographic scene

**Files:**
- Create: `Source/WebUI/recipe-wheel.js`
- Modify: `Source/WebUI/index.html` (add Three.js importmap + script)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create recipe-wheel.js**

Create `Source/WebUI/recipe-wheel.js` — initializes a Three.js scene on `#wheelCanvas` with:

1. **Setup:** OrthographicCamera, WebGLRenderer sized to canvas, ACESFilmicToneMapping
2. **Dark base:** PlaneGeometry with dark material
3. **Holographic rings:** 6 concentric RingGeometry meshes at radii 28-126, rotating at different speeds, low opacity white with iridescent color shifts
4. **Harmonic tank spokes:** 7 Line objects from center to perimeter (dim track), plus 7 bright fill lines (length updated from harmonic amplitude data)
5. **Harmonic nodes:** For each spoke — a large soft CircleGeometry glow halo + small bright core, dual-element v7 style
6. **Energy flow particles:** THREE.Points with 140 particles (20 per spoke). Each particle has a spoke index and progress (0-1). On each frame, progress advances by `speed * amplitude`. When progress >= 1, reset to 0. Position = lerp along spoke. Brightness = amplitude * (1 - progress). This creates streams of light flowing outward faster on high-amplitude harmonics.
7. **Scan line:** Rotating Line, very low opacity
8. **Center glow:** Two CircleGeometry meshes (soft outer + bright inner)
9. **Bloom:** EffectComposer + RenderPass + UnrealBloomPass (strength 3.5, radius 0.85, threshold 0.006) + OutputPass

Exports:
- `initWheel(canvas)` — creates the scene
- `updateHarmonics(amps)` — array of 7 floats (0-1), updates tank fills + node brightness + particle speeds
- `updatePitch(hz, note, preset)` — updates are emitted as DOM events for the CSS overlay to pick up

The CSS overlay for the center OLED display (text content) is handled in index.html — recipe-wheel.js just dispatches a `CustomEvent("pitch-update", ...)` on the canvas element.

- [ ] **Step 2: Add Three.js importmap to index.html**

Add to `<head>`:
```html
<script type="importmap">
{"imports":{
  "three":"https://unpkg.com/three@0.160.0/build/three.module.js",
  "three/addons/":"https://unpkg.com/three@0.160.0/examples/jsm/"
}}
</script>
```

Add recipe-wheel.js script tag.

- [ ] **Step 3: Update CMakeLists BinaryData**

Add `Source/WebUI/recipe-wheel.js`.

- [ ] **Step 4: Build and verify**

Expected: Recipe wheel renders with animated holographic rings, tank fills match the Warm preset, energy flow particles stream outward along spokes, bloom makes everything glow. Center OLED shows pitch info.

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/recipe-wheel.js Source/WebUI/index.html CMakeLists.txt
git commit -m "feat(webui): add Three.js recipe wheel with holographic rings and energy flow"
```

---

## Phase 5 — Spectrum + Meters

### Task 8: Build spectrum.js with bar and line modes

**Files:**
- Create: `Source/WebUI/spectrum.js`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create spectrum.js**

Create `Source/WebUI/spectrum.js` — manages the `#spectrumCanvas` and two `#meterIn` / `#meterOut` canvases.

Exports:
- `initSpectrum(specCanvas, meterInCanvas, meterOutCanvas)` — sets up canvases, starts render loop
- `updateSpectrum(bins)` — array of 80 floats (0-1), smooths and repaints
- `updatePeaks(inL, inR, outL, outR)` — updates meter levels
- `toggleMode()` — switches between bar and line rendering

Render modes:

**Bar mode:** For each of 80 bins, draw a vertical rect with gradient fill (50% white bottom → 4% top). 1px peak cap. 1px gaps between bars.

**Line mode:** Path connecting bin tops with `lineTo`. Fill underneath with gradient (20% white → transparent). Stroke at 1.5px, 60% white.

**Grid:** 5 horizontal dB lines at 2% white. Frequency labels at bottom. dB labels on right.

**Meters:** Vertical fill gradient (35% white bottom → 5% top), height proportional to peak. 1px peak hold line. "IN"/"OUT" label at bottom.

The spectrum listens for a `CustomEvent("spectrum-data")` dispatched by phantom.js, and meters listen for `CustomEvent("peak-data")`.

- [ ] **Step 2: Update CMakeLists and index.html**

Add `Source/WebUI/spectrum.js` to BinaryData. Add script tag to index.html.

- [ ] **Step 3: Build and verify**

Expected: Spectrum analyzer shows real-time frequency content. Meters respond to audio levels. Toggle button switches bar/line modes.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/spectrum.js Source/WebUI/index.html CMakeLists.txt
git commit -m "feat(webui): add Canvas2D spectrum analyzer with bar+line modes and I/O meters"
```

---

## Phase 6 — Settings Overlay + Polish

### Task 9: Add preset selection logic to PluginProcessor

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Add preset change listener to PluginProcessor**

Add `juce::AudioProcessorValueTreeState::Listener` to the Processor class. In `parameterChanged`, when `RECIPE_PRESET` changes, read the preset amplitude table and write values to `RECIPE_H2` through `RECIPE_H8`:

```cpp
// In PluginProcessor.h, add to class declaration:
class PhantomProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener

// Add override:
void parameterChanged(const juce::String& parameterID, float newValue) override;
```

```cpp
// In PluginProcessor.cpp constructor, after apvts init:
apvts.addParameterListener(ParamID::RECIPE_PRESET, this);

// In destructor:
apvts.removeParameterListener(ParamID::RECIPE_PRESET, this);

// Implementation:
void PhantomProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::RECIPE_PRESET)
    {
        int idx = juce::roundToInt(newValue);
        const float* tables[] = { kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps, nullptr };
        if (idx >= 0 && idx < 4 && tables[idx] != nullptr)
        {
            const char* hIds[] = {
                ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
                ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8
            };
            for (int i = 0; i < 7; ++i)
                if (auto* p = apvts.getParameter(hIds[i]))
                    p->setValueNotifyingHost(p->convertTo0to1(tables[idx][i] * 100.0f));
        }
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat: add preset change listener — writes harmonic amplitudes when preset changes"
```

---

### Task 10: Build settings overlay

**Files:**
- Modify: `Source/WebUI/index.html` (add overlay markup)
- Modify: `Source/WebUI/styles.css` (add overlay styles)
- Modify: `Source/WebUI/phantom.js` (wire overlay open/close + secondary param knobs)

- [ ] **Step 1: Add settings overlay HTML**

Add to index.html (before `</body>`):
```html
<div id="settings-overlay" class="settings-overlay hidden">
  <div class="settings-panel">
    <div class="settings-header">
      <span class="el">SETTINGS</span>
      <button id="settings-close" class="settings-close">&times;</button>
    </div>
    <div class="settings-content">
      <div class="settings-section">
        <div class="el">BINAURAL</div>
        <div class="settings-row">
          <select id="binaural-mode-select" data-param="binaural_mode">
            <option value="0">Off</option><option value="1">Spread</option><option value="2">Voice-Split</option>
          </select>
          <phantom-knob data-param="binaural_width" size="medium" label="Width" default-value="0.5"></phantom-knob>
        </div>
      </div>
      <div class="settings-section">
        <div class="el">RECIPE</div>
        <div class="settings-row">
          <phantom-knob data-param="recipe_rotation" size="medium" label="Rotation" default-value="0.5"></phantom-knob>
        </div>
        <div class="el" style="margin-top:8px">HARMONIC PHASES</div>
        <div class="settings-row phases-row">
          <phantom-knob data-param="recipe_phase_h2" size="medium" label="H2 Phase" default-value="0"></phantom-knob>
          <phantom-knob data-param="recipe_phase_h3" size="medium" label="H3 Phase" default-value="0"></phantom-knob>
          <phantom-knob data-param="recipe_phase_h4" size="medium" label="H4 Phase" default-value="0"></phantom-knob>
          <phantom-knob data-param="recipe_phase_h5" size="medium" label="H5 Phase" default-value="0"></phantom-knob>
          <phantom-knob data-param="recipe_phase_h6" size="medium" label="H6 Phase" default-value="0"></phantom-knob>
          <phantom-knob data-param="recipe_phase_h7" size="medium" label="H7 Phase" default-value="0"></phantom-knob>
          <phantom-knob data-param="recipe_phase_h8" size="medium" label="H8 Phase" default-value="0"></phantom-knob>
        </div>
      </div>
    </div>
  </div>
</div>
```

- [ ] **Step 2: Add overlay CSS to styles.css**

```css
.settings-overlay {
  position: absolute; inset: 0; z-index: 100;
  background: rgba(0,0,0,0.7);
  display: flex; justify-content: flex-end;
  transition: opacity 0.2s;
}
.settings-overlay.hidden { display: none; }
.settings-panel {
  width: 360px; height: 100%;
  background: rgba(8,8,16,0.96);
  border-left: 1px solid rgba(255,255,255,0.04);
  box-shadow: -10px 0 40px rgba(0,0,0,0.8);
  padding: 16px; overflow-y: auto;
}
.settings-close {
  background: none; border: none; color: rgba(255,255,255,0.4);
  font-size: 20px; cursor: pointer;
}
.settings-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; }
.settings-section { margin-bottom: 16px; }
.settings-row { display: flex; gap: 12px; flex-wrap: wrap; align-items: center; margin-top: 8px; }
.phases-row { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
```

- [ ] **Step 3: Wire overlay in phantom.js**

Add to phantom.js:
- Gear button click toggles `hidden` class on `#settings-overlay`
- Close button and overlay background click dismiss it
- All `<phantom-knob>` elements inside the overlay are wired to their relays the same way as main knobs

- [ ] **Step 4: Build and verify**

Expected: Gear icon opens settings overlay sliding from right. Contains Binaural mode/width, Recipe rotation, and 7 phase knobs. All functional.

- [ ] **Step 5: Commit**

```bash
git add Source/WebUI/index.html Source/WebUI/styles.css Source/WebUI/phantom.js
git commit -m "feat(webui): add settings overlay with binaural, rotation, and phase controls"
```

---

### Task 11: Final integration and cleanup

**Files:**
- Modify: `CMakeLists.txt` (final BinaryData list)
- Various minor tweaks

- [ ] **Step 1: Ensure CMakeLists has all WebUI files in BinaryData**

```cmake
juce_add_binary_data(PhantomWebUI SOURCES
    Source/WebUI/index.html
    Source/WebUI/styles.css
    Source/WebUI/knob.js
    Source/WebUI/phantom.js
    Source/WebUI/recipe-wheel.js
    Source/WebUI/spectrum.js
)
```

- [ ] **Step 2: Full build and test**

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: Build succeeds, 29/29 DSP tests pass, plugin loads with full WebView UI.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat(webui): complete WebView UI — all phases integrated"
```

---

## Summary

**11 tasks** across 6 phases:

| Phase | Tasks | What it delivers |
|---|---|---|
| 1. C++ Shell | 1-2 | WebView loads, serves HTML from BinaryData |
| 2. Static Layout | 3-4 | Full visual layout with styled knob components |
| 3. Parameter Binding | 5-6 | 35 params wired, knobs control audio |
| 4. Recipe Wheel | 7 | Three.js holographic wheel with energy flow |
| 5. Spectrum + Meters | 8 | Real-time spectrum and I/O meters |
| 6. Settings + Polish | 9-11 | Settings overlay, preset logic, final cleanup |

Each phase produces a buildable, testable increment. The mockup v21 HTML/CSS translates directly into the production UI — no rendering compromises.
