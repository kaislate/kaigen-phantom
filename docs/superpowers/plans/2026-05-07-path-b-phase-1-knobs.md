# Path B — Phase 1: Knob System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the core widget primitives (`PhantomKnob` in 3 sizes, `PhantomMiniKnob`, `ToggleGroup`, `LinkButton`) and drop them into the native editor's panels — replacing wireframe rectangles with real, functional, APVTS-attached controls for everything except the recipe wheel, visualizers, modulation panel, and preset system (those land in later phases).

**Architecture:** Widget classes live in `Source/UI/widgets/`. Each widget paints a baked SVG body (loaded from `Source/Assets/svg/` via `juce_add_binary_data` and `juce::Drawable::createFromSVG`) plus dynamic state (indicator angle, ring fill, depth labels) drawn programmatically with `juce::Graphics`. APVTS attachments are JUCE-stock (`SliderParameterAttachment`, `ButtonParameterAttachment`, `ComboBoxParameterAttachment`) — no custom relay layer. Integration uses two new panel components, `LeftPanel` and `RightPanel` (in `Source/UI/panels/`), that house their respective widget clusters and are added as children of `NativePluginEditor`.

**Tech Stack:** JUCE 8.0.4 (`juce_graphics`, `juce_gui_basics`, `juce_audio_processors`), C++20 (MSVC 17.14), SVG assets via `juce::Drawable`.

**Branch:** continue on `feature/pr3b-macro-editor` for now (additive changes; default behavior unchanged for users who don't toggle the native editor on).

**Spec:** `docs/superpowers/specs/2026-05-07-path-b-native-ui-design.md`. Phase 0 plan: `docs/superpowers/plans/2026-05-07-path-b-phase-0-foundation.md` (already complete).

**Visual fidelity note:** Phase 1's SVG assets are functional placeholders — simple gradient circles with a notch indicator, not the polished neumorphic look from the WebView2 UI. Visual polish (matching the current CSS-rendered knob exactly) is a deferred refinement in a later phase. The goal here is widget *behavior* — drag, attachment, repaint — not pixel-perfect aesthetics.

---

## File Structure

| File | Status | Responsibility |
|------|--------|----------------|
| `Source/Assets/svg/knob_medium.svg` | **Create** | Baked body asset for medium-size knobs. Plain circle with a radial gradient. ~50×50. |
| `Source/Assets/svg/knob_large.svg` | **Create** | Same shape, 66×66. |
| `Source/Assets/svg/knob_small.svg` | **Create** | Same shape, 36×36. |
| `Source/Assets/svg/knob_mini.svg` | **Create** | Smaller flatter design for advanced-panel mini knobs. ~28×28. |
| `Source/Assets/svg/link_icon.svg` | **Create** | The chain-link SVG icon used by `LinkButton` (currently inline SVG in `index.html`). |
| `CMakeLists.txt` | **Modify** | Add a second `juce_add_binary_data` target `PhantomNativeAssets` for the SVG files; link to `KaigenPhantom`. |
| `Source/UI/widgets/PhantomKnob.h` | **Create** | Knob widget header. Supports Large/Medium/Small variants via constructor enum. Owns its own `juce::Slider` (hidden), `SliderParameterAttachment`, and a cached `juce::Drawable` for the baked body. |
| `Source/UI/widgets/PhantomKnob.cpp` | **Create** | Paints baked body + dynamic indicator line + ring overlay (optional) + value label below. Forwards mouse events to the internal slider. |
| `Source/UI/widgets/PhantomMiniKnob.h` | **Create** | Smaller variant. Same architecture as `PhantomKnob` but uses `knob_mini.svg` and a more compact label layout. |
| `Source/UI/widgets/PhantomMiniKnob.cpp` | **Create** | Implementation. |
| `Source/UI/widgets/ToggleGroup.h` | **Create** | Multi-button radio control. Constructor takes APVTS, the param ID (must be an `AudioParameterChoice`), and a list of label strings. Owns child `juce::TextButton`s plus a `ButtonParameterAttachment`-equivalent (we'll use a single `ComboBoxParameterAttachment` against a hidden `juce::ComboBox` — see implementation note in Task 8). |
| `Source/UI/widgets/ToggleGroup.cpp` | **Create** | Lays out child buttons in a row, paints the active one with `Theme::steelBlue` glow. |
| `Source/UI/widgets/LinkButton.h` | **Create** | Toggle button with chain-link icon. Owns a `juce::ToggleButton` and a `ButtonParameterAttachment` (or a `juce::Button::Listener`-based pattern if no APVTS param backs it — link state is UI-side only in the current design). |
| `Source/UI/widgets/LinkButton.cpp` | **Create** | Paints the chain-link SVG; tints when active. |
| `Source/UI/panels/RightPanel.h` | **Create** | Right-panel `juce::Component` housing HarmonicEngineSection (3 knobs), StereoSection (1 knob), LevelsSection (2 knobs + Auto button), AdvancedPanel (8 mini knobs). Owns the widget instances. |
| `Source/UI/panels/RightPanel.cpp` | **Create** | Implementation. Lays out children in flex/grid using the existing CSS proportions. |
| `Source/UI/panels/LeftPanel.h` | **Create** | Left-panel `juce::Component` housing GhostSection (3 knobs + ghost mode toggle group) and FilterSection (LPF, HPF, LinkButton, slope toggle group). Recipe wheel is a placeholder rectangle until Phase 3. |
| `Source/UI/panels/LeftPanel.cpp` | **Create** | Implementation. |
| `Source/UI/NativePluginEditor.h` | **Modify** | Add `LeftPanel` and `RightPanel` as members. |
| `Source/UI/NativePluginEditor.cpp` | **Modify** | In the constructor, instantiate panels and `addAndMakeVisible`. In `resized()`, lay them out where the wireframe rectangles were. In `paint()`, remove the now-redundant labeled rectangles for LeftPanel and RightPanel — leave only the TopBar and ModulationPanel placeholders. |

**Out of scope for Phase 1** (deferred to later phases):
- Recipe wheel (Phase 3)
- Spectrum / Oscilloscope / IOMeter visualizers (Phase 2)
- ModulationPanel slot row, MatrixView (Phase 5)
- Preset selector + advanced toggle (Phase 4 / Phase 3)
- Visual polish to match WebView2 CSS exactly (later phase, before Phase 6 cutover)

---

## Task 1: Asset CMake plumbing + placeholder SVG

**Files:**
- Create: `Source/Assets/svg/knob_medium.svg`
- Modify: `CMakeLists.txt`

A single SVG file to validate the binary-data pipeline before authoring the rest. We use a separate `juce_add_binary_data` target (`PhantomNativeAssets`) to keep native UI assets out of the WebView2 binary data namespace.

- [ ] **Step 1: Create the directory and a placeholder knob_medium.svg**

```bash
mkdir -p "Source/Assets/svg"
```

Create `Source/Assets/svg/knob_medium.svg` with this content:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 50 50" width="50" height="50">
  <defs>
    <radialGradient id="bodyGrad" cx="50%" cy="35%" r="60%">
      <stop offset="0%" stop-color="#6a7a8a"/>
      <stop offset="60%" stop-color="#3a414a"/>
      <stop offset="100%" stop-color="#1a1d22"/>
    </radialGradient>
  </defs>
  <circle cx="25" cy="25" r="22" fill="url(#bodyGrad)" stroke="#0a0c10" stroke-width="1"/>
  <circle cx="25" cy="25" r="20" fill="none" stroke="#ffffff10" stroke-width="1"/>
</svg>
```

This is a placeholder neumorphic-ish knob body — a dark gradient circle with a subtle inner ring. Phase 1 widgets paint a programmatic indicator on top of this; visual polish to match the WebView2 CSS look exactly is a later refinement.

- [ ] **Step 2: Add the binary-data target to CMakeLists.txt**

Open `CMakeLists.txt`. Find the existing `juce_add_binary_data(PhantomWebUI ...)` block (around lines 60-78). Add a new sibling block immediately AFTER it:

```cmake
juce_add_binary_data(PhantomNativeAssets SOURCES
    Source/Assets/svg/knob_medium.svg
)
```

Then find `target_link_libraries(KaigenPhantom`. The PhantomWebUI library is linked there. Add `PhantomNativeAssets` to the same link block. The exact location in CMakeLists.txt for `target_link_libraries` is near the bottom of the file. Add `PhantomNativeAssets` as a `PRIVATE` link target, mirroring how `PhantomWebUI` is linked. Read the existing block first to match its style.

- [ ] **Step 3: Build to verify the binary-data pipeline**

Run:
```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: build succeeds. The `PhantomNativeAssets` target produces a `BinaryData2.h` (or similarly numbered) header. The asset is bundled in the binary even though no code references it yet.

- [ ] **Step 4: Commit**

```bash
git add Source/Assets/svg/knob_medium.svg CMakeLists.txt
git commit -m "feat(path-b): asset CMake plumbing + placeholder knob_medium.svg"
```

---

## Task 2: Add knob_large.svg, knob_small.svg, knob_mini.svg, link_icon.svg

**Files:**
- Create: `Source/Assets/svg/knob_large.svg`
- Create: `Source/Assets/svg/knob_small.svg`
- Create: `Source/Assets/svg/knob_mini.svg`
- Create: `Source/Assets/svg/link_icon.svg`
- Modify: `CMakeLists.txt`

Same placeholder design at three more sizes, plus the link-icon for the filter LPF/HPF link button.

- [ ] **Step 1: Create knob_large.svg**

`Source/Assets/svg/knob_large.svg`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 66 66" width="66" height="66">
  <defs>
    <radialGradient id="bodyGrad" cx="50%" cy="35%" r="60%">
      <stop offset="0%" stop-color="#6a7a8a"/>
      <stop offset="60%" stop-color="#3a414a"/>
      <stop offset="100%" stop-color="#1a1d22"/>
    </radialGradient>
  </defs>
  <circle cx="33" cy="33" r="30" fill="url(#bodyGrad)" stroke="#0a0c10" stroke-width="1.2"/>
  <circle cx="33" cy="33" r="28" fill="none" stroke="#ffffff10" stroke-width="1"/>
</svg>
```

- [ ] **Step 2: Create knob_small.svg**

`Source/Assets/svg/knob_small.svg`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 36 36" width="36" height="36">
  <defs>
    <radialGradient id="bodyGrad" cx="50%" cy="35%" r="60%">
      <stop offset="0%" stop-color="#6a7a8a"/>
      <stop offset="60%" stop-color="#3a414a"/>
      <stop offset="100%" stop-color="#1a1d22"/>
    </radialGradient>
  </defs>
  <circle cx="18" cy="18" r="16" fill="url(#bodyGrad)" stroke="#0a0c10" stroke-width="0.8"/>
  <circle cx="18" cy="18" r="14" fill="none" stroke="#ffffff10" stroke-width="0.8"/>
</svg>
```

- [ ] **Step 3: Create knob_mini.svg**

`Source/Assets/svg/knob_mini.svg` (more flat — used for advanced panel mini knobs):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 28 28" width="28" height="28">
  <defs>
    <radialGradient id="bodyGrad" cx="50%" cy="35%" r="60%">
      <stop offset="0%" stop-color="#52606e"/>
      <stop offset="100%" stop-color="#272c33"/>
    </radialGradient>
  </defs>
  <circle cx="14" cy="14" r="12" fill="url(#bodyGrad)" stroke="#0a0c10" stroke-width="0.6"/>
</svg>
```

- [ ] **Step 4: Create link_icon.svg**

`Source/Assets/svg/link_icon.svg` (chain-link icon, ~16×16 vector, traced from the inline SVG in `Source/WebUI/index.html` at the `filterLinkBtn` element):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" width="16" height="16">
  <path d="M6.5 9.5 L9.5 6.5 M5 11 L3.5 11 A2.5 2.5 0 1 1 3.5 6 L5 6 M11 5 L12.5 5 A2.5 2.5 0 1 1 12.5 10 L11 10"
        fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
```

- [ ] **Step 5: Add the four new files to CMakeLists.txt**

In `CMakeLists.txt`, find the `juce_add_binary_data(PhantomNativeAssets SOURCES` block from Task 1. Update it to:

```cmake
juce_add_binary_data(PhantomNativeAssets SOURCES
    Source/Assets/svg/knob_large.svg
    Source/Assets/svg/knob_medium.svg
    Source/Assets/svg/knob_small.svg
    Source/Assets/svg/knob_mini.svg
    Source/Assets/svg/link_icon.svg
)
```

- [ ] **Step 6: Build to verify**

Run:
```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: build succeeds. All five assets bundled.

- [ ] **Step 7: Commit**

```bash
git add Source/Assets/svg/knob_large.svg Source/Assets/svg/knob_small.svg Source/Assets/svg/knob_mini.svg Source/Assets/svg/link_icon.svg CMakeLists.txt
git commit -m "feat(path-b): add knob_large/small/mini + link_icon SVG assets"
```

---

## Task 3: PhantomKnob class — medium variant

**Files:**
- Create: `Source/UI/widgets/PhantomKnob.h`
- Create: `Source/UI/widgets/PhantomKnob.cpp`
- Modify: `CMakeLists.txt`

Initial `PhantomKnob` class supporting only the Medium size. Owns a hidden `juce::Slider` for value handling and a `SliderParameterAttachment` for APVTS binding. `paint()` draws the baked body SVG plus a dynamic indicator line at the angle corresponding to the current normalized value.

- [ ] **Step 1: Create Source/UI/widgets/PhantomKnob.h**

```cpp
// Source/UI/widgets/PhantomKnob.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Native rotary knob widget for the Phantom plugin. Wraps a juce::Slider
 *  (hidden) and a SliderParameterAttachment for APVTS binding. Paints a
 *  baked-SVG body plus a dynamic indicator line. Mouse events forward to
 *  the internal slider. */
class PhantomKnob : public juce::Component, private juce::Slider::Listener
{
public:
    enum class Size { Large, Medium, Small };

    PhantomKnob(juce::AudioProcessorValueTreeState& apvts,
                juce::StringRef paramID,
                Size size,
                const juce::String& label = {});
    ~PhantomKnob() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void sliderValueChanged(juce::Slider* s) override;

    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    Size size;
    juce::String label;
    std::unique_ptr<juce::Drawable> bodyDrawable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomKnob)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/PhantomKnob.cpp**

```cpp
// Source/UI/widgets/PhantomKnob.cpp
#include "PhantomKnob.h"
#include "../Theme.h"
#include "BinaryData.h"

namespace kaigen::phantom
{

namespace
{
    // Indicator angle range: -135° (min, 7-o'clock) to +135° (max, 5-o'clock).
    // Total sweep = 270°. Matches typical synth knob convention.
    constexpr float kAngleMinRadians = -2.356194f;  // -135 * PI / 180
    constexpr float kAngleMaxRadians =  2.356194f;  //  135 * PI / 180

    float normalizedToAngle(float n)
    {
        return kAngleMinRadians + (kAngleMaxRadians - kAngleMinRadians) * juce::jlimit(0.0f, 1.0f, n);
    }

    juce::Drawable* loadKnobSvg(PhantomKnob::Size size)
    {
        const char* data = nullptr;
        int dataSize = 0;
        switch (size)
        {
            case PhantomKnob::Size::Large:
                data = BinaryData::knob_large_svg;
                dataSize = BinaryData::knob_large_svgSize;
                break;
            case PhantomKnob::Size::Medium:
                data = BinaryData::knob_medium_svg;
                dataSize = BinaryData::knob_medium_svgSize;
                break;
            case PhantomKnob::Size::Small:
                data = BinaryData::knob_small_svg;
                dataSize = BinaryData::knob_small_svgSize;
                break;
        }
        if (data == nullptr) return nullptr;
        auto drawable = juce::Drawable::createFromImageData(data, (size_t) dataSize);
        return drawable.release();
    }

    int sizePixels(PhantomKnob::Size s)
    {
        switch (s)
        {
            case PhantomKnob::Size::Large:  return 66;
            case PhantomKnob::Size::Medium: return 50;
            case PhantomKnob::Size::Small:  return 36;
        }
        return 50;
    }
}

PhantomKnob::PhantomKnob(juce::AudioProcessorValueTreeState& apvts,
                         juce::StringRef paramID,
                         Size sz,
                         const juce::String& lbl)
    : size(sz), label(lbl)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.setRange(0.0, 1.0, 0.0);
    slider.addListener(this);
    addChildComponent(slider);  // hidden — we forward events manually

    if (auto* param = apvts.getParameter(paramID))
        attachment = std::make_unique<juce::SliderParameterAttachment>(*param, slider);

    bodyDrawable.reset(loadKnobSvg(size));

    // Total widget height = body + label space underneath.
    const int body = sizePixels(size);
    const int labelHeight = label.isNotEmpty() ? 14 : 0;
    setSize(body + 8, body + labelHeight + 4);
}

PhantomKnob::~PhantomKnob()
{
    slider.removeListener(this);
}

void PhantomKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const int body = sizePixels(size);
    auto bodyArea = juce::Rectangle<float>((float) (getWidth() - body) / 2.0f,
                                           4.0f,
                                           (float) body,
                                           (float) body);

    if (bodyDrawable != nullptr)
    {
        bodyDrawable->setTransformToFit(bodyArea, juce::RectanglePlacement::stretchToFit);
        bodyDrawable->draw(g, 1.0f);
    }
    else
    {
        // Fallback if SVG load failed.
        g.setColour(Theme::panelBg);
        g.fillEllipse(bodyArea);
    }

    // Indicator line — drawn from center outward at the current value's angle.
    const auto centre = bodyArea.getCentre();
    const float radius = bodyArea.getWidth() * 0.42f;
    const float angle = normalizedToAngle((float) slider.getValue());
    const float endX = centre.x + radius * std::sin(angle);
    const float endY = centre.y - radius * std::cos(angle);
    g.setColour(Theme::steelBlue);
    g.drawLine(centre.x, centre.y, endX, endY, 2.0f);

    // Label below body.
    if (label.isNotEmpty())
    {
        g.setColour(Theme::textSecondary);
        g.setFont(juce::FontOptions("Space Grotesk", 10.0f, juce::Font::plain));
        auto labelArea = juce::Rectangle<float>(0.0f,
                                                (float) (body + 4),
                                                (float) getWidth(),
                                                14.0f);
        g.drawText(label, labelArea, juce::Justification::centred, false);
    }
}

void PhantomKnob::resized()
{
    // Slider is hidden — sized to match the body area for event hit-testing.
    slider.setBounds(getLocalBounds());
}

void PhantomKnob::mouseDown(const juce::MouseEvent& e)        { slider.mouseDown(e); }
void PhantomKnob::mouseDrag(const juce::MouseEvent& e)        { slider.mouseDrag(e); }
void PhantomKnob::mouseUp(const juce::MouseEvent& e)          { slider.mouseUp(e); }
void PhantomKnob::mouseDoubleClick(const juce::MouseEvent& e) { slider.mouseDoubleClick(e); }

void PhantomKnob::sliderValueChanged(juce::Slider*)
{
    repaint();
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, find the `target_sources(KaigenPhantom PRIVATE` block. Add `Source/UI/widgets/PhantomKnob.cpp`:

```cmake
target_sources(KaigenPhantom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/NativePluginEditor.cpp
    Source/UI/widgets/PhantomKnob.cpp
    Source/Engines/BinauralStage.cpp
    ...
)
```

(Keep all other entries.)

- [ ] **Step 4: Build to verify compile**

Run:
```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: build succeeds. The `BinaryData::knob_medium_svg` symbol is available because the `PhantomNativeAssets` target produced it from Task 1.

If you get a linker error about `BinaryData::knob_medium_svg` not being found: the `target_link_libraries` step from Task 1 wasn't done correctly. Re-check that `PhantomNativeAssets` is linked to `KaigenPhantom`.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/widgets/PhantomKnob.h Source/UI/widgets/PhantomKnob.cpp CMakeLists.txt
git commit -m "feat(path-b): PhantomKnob widget (medium variant; baked SVG + indicator)"
```

---

## Task 4: Drop HarmonicEngineSection knobs into RightPanel

**Files:**
- Create: `Source/UI/panels/RightPanel.h`
- Create: `Source/UI/panels/RightPanel.cpp`
- Modify: `Source/UI/NativePluginEditor.h`
- Modify: `Source/UI/NativePluginEditor.cpp`
- Modify: `CMakeLists.txt`

End-to-end integration test: drop three real `PhantomKnob`s (Saturation, Shape, Skip) into the native editor's right-panel area. Validates the asset → widget → APVTS attachment → repaint chain works.

`RightPanel` will eventually hold all the right-side controls (HarmonicEngine, Stereo, Levels, Advanced, Visualizers). For Task 4, only HarmonicEngine is populated; the rest are still wireframe. We extend in later tasks.

- [ ] **Step 1: Create Source/UI/panels/RightPanel.h**

```cpp
// Source/UI/panels/RightPanel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"

namespace kaigen::phantom
{

class RightPanel : public juce::Component
{
public:
    explicit RightPanel(juce::AudioProcessorValueTreeState& apvts);
    ~RightPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Harmonic Engine section (Task 4): per current per-engine 'a_' prefix.
    PhantomKnob saturationKnob;
    PhantomKnob shapeKnob;
    PhantomKnob skipKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RightPanel)
};

} // namespace kaigen::phantom
```

(Note: parameter IDs use `a_` prefix because the editor displays Engine A's controls by default. Engine focus switching — A vs B — is a Phase 1B / Phase 2 concern; for Phase 1 we hardcode `a_` and skip LINK mode. The wireframe-replacing knobs will reflect Engine A only until later phases wire in the engine-tab.)

- [ ] **Step 2: Create Source/UI/panels/RightPanel.cpp**

```cpp
// Source/UI/panels/RightPanel.cpp
#include "RightPanel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        g.setColour(Theme::textSecondary);
        g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::bold));
        g.drawText(title, bounds.toFloat(), juce::Justification::centredLeft, false);
    }
}

RightPanel::RightPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      saturationKnob(apvts, "a_harmonic_saturation", PhantomKnob::Size::Medium, "Saturation"),
      shapeKnob(apvts, "a_synth_step", PhantomKnob::Size::Medium, "Shape"),
      skipKnob(apvts, "a_synth_skip", PhantomKnob::Size::Medium, "Skip")
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(shapeKnob);
    addAndMakeVisible(skipKnob);
}

RightPanel::~RightPanel() = default;

void RightPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    // Section header above the knob row.
    auto sectionHeader = juce::Rectangle<int>(12, 8, 200, 16);
    drawSectionHeader(g, sectionHeader, "Harmonic Engine");
}

void RightPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(28);  // space for section header

    // Three medium knobs in a row at the top.
    auto knobRow = area.removeFromTop(80).reduced(12, 0);
    const int knobWidth = 80;
    auto layoutKnob = [&](PhantomKnob& k) {
        k.setBounds(knobRow.removeFromLeft(knobWidth));
        knobRow.removeFromLeft(8);
    };
    layoutKnob(saturationKnob);
    layoutKnob(shapeKnob);
    layoutKnob(skipKnob);
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add the panel to NativePluginEditor**

Modify `Source/UI/NativePluginEditor.h`:

```cpp
// Source/UI/NativePluginEditor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "panels/RightPanel.h"

class PhantomProcessor;

namespace kaigen::phantom
{

class NativePluginEditor : public juce::AudioProcessorEditor,
                           private juce::Button::Listener
{
public:
    NativePluginEditor(PhantomProcessor& processor, juce::AudioProcessorValueTreeState& apvts);
    ~NativePluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* b) override;

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton backToWebViewButton { "<- WebView2" };
    RightPanel rightPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativePluginEditor)
};

} // namespace kaigen::phantom
```

Modify `Source/UI/NativePluginEditor.cpp`. In the constructor, add `addAndMakeVisible(rightPanel);` and pass `apvts` to its initializer:

```cpp
NativePluginEditor::NativePluginEditor(PhantomProcessor& p,
                                       juce::AudioProcessorValueTreeState& a)
    : juce::AudioProcessorEditor(&p),
      processor(p),
      apvts(a),
      rightPanel(a)
{
    setSize(editorWidth, editorHeight);

    backToWebViewButton.addListener(this);
    addAndMakeVisible(backToWebViewButton);

    addAndMakeVisible(rightPanel);
}
```

In `resized()`, the back-to-WebView2 button's bounds are unchanged. The right panel takes the position of the right-half wireframe rectangle. Update the `paint()` function: skip drawing the labeled "RightPanel" rectangle since the real `RightPanel` Component now occupies that area.

Replace the `paint()` function body with:

```cpp
void NativePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::deepBg);

    auto area = getLocalBounds();

    // TopBar across the very top.
    auto topBar = area.removeFromTop(topBarHeight);
    drawLabeledPanel(g, topBar, "TopBar (preset selector + advanced toggle)");

    // ModulationPanel across the very bottom.
    auto modPanel = area.removeFromBottom(modPanelHeight);
    drawLabeledPanel(g, modPanel, "ModulationPanel (mode bar + slot row + engine labels)");

    // MainArea split into LeftPanel + RightPanel.
    auto leftPanel = area.removeFromLeft(leftPanelWidth);
    drawLabeledPanel(g, leftPanel, "LeftPanel (recipe wheel + ghost + filter)");
    // RightPanel: real Component now (rendered as a child), so just leave the
    // remaining area transparent — the child Component fills it.
}
```

In `resized()`, set the right panel's bounds to match the area:

```cpp
void NativePluginEditor::resized()
{
    backToWebViewButton.setBounds(getWidth() - 110, 10, 100, 26);

    auto area = getLocalBounds();
    area.removeFromTop(topBarHeight);
    area.removeFromBottom(modPanelHeight);
    area.removeFromLeft(leftPanelWidth);
    rightPanel.setBounds(area);
}
```

- [ ] **Step 4: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/panels/RightPanel.cpp`:

```cmake
target_sources(KaigenPhantom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/NativePluginEditor.cpp
    Source/UI/widgets/PhantomKnob.cpp
    Source/UI/panels/RightPanel.cpp
    ...
)
```

- [ ] **Step 5: Build and manually verify**

Run:
```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: builds clean. Open Standalone or load Phantom in Live with native UI toggled on. Expect:
- TopBar / LeftPanel / ModulationPanel still show wireframe rectangles
- RightPanel area shows three real knobs labeled "Saturation", "Shape", "Skip" in a row
- Drag a knob → indicator rotates
- The corresponding APVTS parameter changes (verify by switching back to WebView2, observing the matching knob there has the same value)

- [ ] **Step 6: Commit**

```bash
git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp CMakeLists.txt
git commit -m "feat(path-b): RightPanel with HarmonicEngineSection knobs (Saturation/Shape/Skip)"
```

---

## Task 5: PhantomMiniKnob class

**Files:**
- Create: `Source/UI/widgets/PhantomMiniKnob.h`
- Create: `Source/UI/widgets/PhantomMiniKnob.cpp`
- Modify: `CMakeLists.txt`

A smaller, flatter knob variant for the advanced-panel mini knobs. Same architecture as `PhantomKnob` but uses `knob_mini.svg` and a smaller label. We could subclass `PhantomKnob`, but a separate class keeps each focused and avoids size-conditional branches in paint code.

- [ ] **Step 1: Create Source/UI/widgets/PhantomMiniKnob.h**

```cpp
// Source/UI/widgets/PhantomMiniKnob.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

class PhantomMiniKnob : public juce::Component, private juce::Slider::Listener
{
public:
    PhantomMiniKnob(juce::AudioProcessorValueTreeState& apvts,
                    juce::StringRef paramID,
                    const juce::String& label = {});
    ~PhantomMiniKnob() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void sliderValueChanged(juce::Slider* s) override;

    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    juce::String label;
    std::unique_ptr<juce::Drawable> bodyDrawable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomMiniKnob)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/PhantomMiniKnob.cpp**

```cpp
// Source/UI/widgets/PhantomMiniKnob.cpp
#include "PhantomMiniKnob.h"
#include "../Theme.h"
#include "BinaryData.h"

namespace kaigen::phantom
{

namespace
{
    constexpr float kAngleMinRadians = -2.356194f;
    constexpr float kAngleMaxRadians =  2.356194f;
    constexpr int   kBodySize        = 28;

    float normalizedToAngle(float n)
    {
        return kAngleMinRadians + (kAngleMaxRadians - kAngleMinRadians) * juce::jlimit(0.0f, 1.0f, n);
    }
}

PhantomMiniKnob::PhantomMiniKnob(juce::AudioProcessorValueTreeState& apvts,
                                  juce::StringRef paramID,
                                  const juce::String& lbl)
    : label(lbl)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.setRange(0.0, 1.0, 0.0);
    slider.addListener(this);
    addChildComponent(slider);

    if (auto* param = apvts.getParameter(paramID))
        attachment = std::make_unique<juce::SliderParameterAttachment>(*param, slider);

    bodyDrawable = juce::Drawable::createFromImageData(BinaryData::knob_mini_svg, BinaryData::knob_mini_svgSize);

    const int labelHeight = label.isNotEmpty() ? 11 : 0;
    setSize(kBodySize + 6, kBodySize + labelHeight + 2);
}

PhantomMiniKnob::~PhantomMiniKnob()
{
    slider.removeListener(this);
}

void PhantomMiniKnob::paint(juce::Graphics& g)
{
    auto bodyArea = juce::Rectangle<float>((float) (getWidth() - kBodySize) / 2.0f,
                                           2.0f,
                                           (float) kBodySize,
                                           (float) kBodySize);

    if (bodyDrawable != nullptr)
    {
        bodyDrawable->setTransformToFit(bodyArea, juce::RectanglePlacement::stretchToFit);
        bodyDrawable->draw(g, 1.0f);
    }

    const auto centre = bodyArea.getCentre();
    const float radius = bodyArea.getWidth() * 0.40f;
    const float angle = normalizedToAngle((float) slider.getValue());
    const float endX = centre.x + radius * std::sin(angle);
    const float endY = centre.y - radius * std::cos(angle);
    g.setColour(Theme::steelBlue);
    g.drawLine(centre.x, centre.y, endX, endY, 1.5f);

    if (label.isNotEmpty())
    {
        g.setColour(Theme::textSecondary);
        g.setFont(juce::FontOptions("Space Grotesk", 8.0f, juce::Font::plain));
        auto labelArea = juce::Rectangle<float>(0.0f,
                                                (float) (kBodySize + 2),
                                                (float) getWidth(),
                                                11.0f);
        g.drawText(label, labelArea, juce::Justification::centred, false);
    }
}

void PhantomMiniKnob::resized()
{
    slider.setBounds(getLocalBounds());
}

void PhantomMiniKnob::mouseDown(const juce::MouseEvent& e)        { slider.mouseDown(e); }
void PhantomMiniKnob::mouseDrag(const juce::MouseEvent& e)        { slider.mouseDrag(e); }
void PhantomMiniKnob::mouseUp(const juce::MouseEvent& e)          { slider.mouseUp(e); }
void PhantomMiniKnob::mouseDoubleClick(const juce::MouseEvent& e) { slider.mouseDoubleClick(e); }

void PhantomMiniKnob::sliderValueChanged(juce::Slider*) { repaint(); }

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `Source/UI/widgets/PhantomMiniKnob.cpp` to `target_sources(KaigenPhantom PRIVATE`.

- [ ] **Step 4: Build to verify compile**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. PhantomMiniKnob class compiles but isn't yet used.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/widgets/PhantomMiniKnob.h Source/UI/widgets/PhantomMiniKnob.cpp CMakeLists.txt
git commit -m "feat(path-b): PhantomMiniKnob widget for advanced-panel mini knobs"
```

---

## Task 6: Drop StereoSection (Width) + LevelsSection (In, Out) into RightPanel

**Files:**
- Modify: `Source/UI/panels/RightPanel.h`
- Modify: `Source/UI/panels/RightPanel.cpp`

Add the remaining medium knobs to the RightPanel: Width (Stereo), In + Out (Levels). The Auto-gain button is deferred to Task 9 since it requires a different button widget. The IOMeter is deferred to Phase 2.

- [ ] **Step 1: Update RightPanel.h**

Add the new knob members:

```cpp
// Source/UI/panels/RightPanel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"

namespace kaigen::phantom
{

class RightPanel : public juce::Component
{
public:
    explicit RightPanel(juce::AudioProcessorValueTreeState& apvts);
    ~RightPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Harmonic Engine section
    PhantomKnob saturationKnob;
    PhantomKnob shapeKnob;
    PhantomKnob skipKnob;

    // Stereo section
    PhantomKnob widthKnob;

    // Levels section
    PhantomKnob inGainKnob;
    PhantomKnob outGainKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RightPanel)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Update RightPanel.cpp**

Update the constructor to initialize the new knobs and `addAndMakeVisible` them. Update `paint()` to draw section headers for Stereo and Levels. Update `resized()` to lay out the new knobs.

```cpp
// Source/UI/panels/RightPanel.cpp
#include "RightPanel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        g.setColour(Theme::textSecondary);
        g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::bold));
        g.drawText(title, bounds.toFloat(), juce::Justification::centredLeft, false);
    }
}

RightPanel::RightPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      saturationKnob(apvts, "a_harmonic_saturation", PhantomKnob::Size::Medium, "Saturation"),
      shapeKnob     (apvts, "a_synth_step",          PhantomKnob::Size::Medium, "Shape"),
      skipKnob      (apvts, "a_synth_skip",          PhantomKnob::Size::Medium, "Skip"),
      widthKnob     (apvts, "a_stereo_width",        PhantomKnob::Size::Medium, "Width"),
      inGainKnob    (apvts, "input_gain",            PhantomKnob::Size::Medium, "In"),
      outGainKnob   (apvts, "a_output_gain",         PhantomKnob::Size::Medium, "Out")
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(shapeKnob);
    addAndMakeVisible(skipKnob);
    addAndMakeVisible(widthKnob);
    addAndMakeVisible(inGainKnob);
    addAndMakeVisible(outGainKnob);
}

RightPanel::~RightPanel() = default;

void RightPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    drawSectionHeader(g, juce::Rectangle<int>(12,   8, 200, 16), "Harmonic Engine");
    drawSectionHeader(g, juce::Rectangle<int>(310,  8, 100, 16), "Stereo");
    drawSectionHeader(g, juce::Rectangle<int>(420,  8, 200, 16), "Levels");
}

void RightPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(28);

    auto knobRow = area.removeFromTop(80).reduced(12, 0);
    const int knobWidth = 80;
    const int gap       = 8;
    const int sectionGap = 24;

    auto layoutKnob = [&](PhantomKnob& k) {
        k.setBounds(knobRow.removeFromLeft(knobWidth));
        knobRow.removeFromLeft(gap);
    };

    // Harmonic Engine
    layoutKnob(saturationKnob);
    layoutKnob(shapeKnob);
    layoutKnob(skipKnob);

    knobRow.removeFromLeft(sectionGap);

    // Stereo
    layoutKnob(widthKnob);

    knobRow.removeFromLeft(sectionGap);

    // Levels
    layoutKnob(inGainKnob);
    layoutKnob(outGainKnob);
}

} // namespace kaigen::phantom
```

(Note: parameter ID for `In` is `input_gain` (no `a_` prefix — it's a global parameter, not per-engine). `Out` is `a_output_gain` (per-engine). Same for `b_output_gain` when engine focus switches in a later phase. Reference `Source/Parameters.h` for the full parameter list if uncertain.)

- [ ] **Step 3: Build and manually verify**

Run:
```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Native editor's right panel now shows 6 medium knobs in a row (Saturation, Shape, Skip, [gap], Width, [gap], In, Out) with their section headers above. Drag each → APVTS parameter updates.

- [ ] **Step 4: Commit**

```bash
git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp
git commit -m "feat(path-b): drop StereoSection (Width) + LevelsSection (In/Out) knobs"
```

---

## Task 7: AdvancedPanel mini-knob row in RightPanel

**Files:**
- Modify: `Source/UI/panels/RightPanel.h`
- Modify: `Source/UI/panels/RightPanel.cpp`

Add the 8 mini knobs from the existing advanced panel (Wavelet sub-section: Push, H1, Sub, Length, Gate, Min, Max, Track. Punch & Envelope sub-section: Amount, Threshold, Boost, Attack, Release. Binaural sub-section: Width). For Phase 1, we render all 14 mini knobs in a single row labeled "Advanced". Sub-section grouping (Wavelet / Punch & Envelope / Binaural separators + Mode select) is deferred to a later refinement — Phase 1 just gets the parameters bound.

The mini knobs map to per-engine `a_` prefixed parameters. The complete list:
- a_synth_duty (Push)
- a_synth_h1 (H1)
- a_synth_sub (Sub)
- a_synth_wavelet_length (Length)
- a_synth_gate_threshold (Gate)
- a_synth_min_samples (Min)
- a_synth_max_samples (Max)
- a_tracking_speed (Track)
- a_punch_amount (Amount)
- a_synth_boost_threshold (Threshold)
- a_synth_boost_amount (Boost)
- a_env_attack_ms (Attack)
- a_env_release_ms (Release)
- a_binaural_width (Width)

The Binaural Mode select (combo box with Off/Spread/Voice-Split) is deferred — `juce::ComboBox` integration is its own widget pattern. We'll handle it in Task 11.

- [ ] **Step 1: Update RightPanel.h to add mini-knob members**

```cpp
// Source/UI/panels/RightPanel.h
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/PhantomMiniKnob.h"

namespace kaigen::phantom
{

class RightPanel : public juce::Component
{
public:
    explicit RightPanel(juce::AudioProcessorValueTreeState& apvts);
    ~RightPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Top row knobs
    PhantomKnob saturationKnob;
    PhantomKnob shapeKnob;
    PhantomKnob skipKnob;
    PhantomKnob widthKnob;
    PhantomKnob inGainKnob;
    PhantomKnob outGainKnob;

    // Advanced panel — 14 mini knobs, all per-engine 'a_' prefix.
    std::array<std::unique_ptr<PhantomMiniKnob>, 14> miniKnobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RightPanel)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Update RightPanel.cpp**

Replace the constructor and `resized()` to include the mini knobs. Build the array using the param/label list.

```cpp
RightPanel::RightPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      saturationKnob(apvts, "a_harmonic_saturation", PhantomKnob::Size::Medium, "Saturation"),
      shapeKnob     (apvts, "a_synth_step",          PhantomKnob::Size::Medium, "Shape"),
      skipKnob      (apvts, "a_synth_skip",          PhantomKnob::Size::Medium, "Skip"),
      widthKnob     (apvts, "a_stereo_width",        PhantomKnob::Size::Medium, "Width"),
      inGainKnob    (apvts, "input_gain",            PhantomKnob::Size::Medium, "In"),
      outGainKnob   (apvts, "a_output_gain",         PhantomKnob::Size::Medium, "Out")
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(shapeKnob);
    addAndMakeVisible(skipKnob);
    addAndMakeVisible(widthKnob);
    addAndMakeVisible(inGainKnob);
    addAndMakeVisible(outGainKnob);

    // Advanced mini knobs.
    static constexpr struct { const char* paramID; const char* label; } miniDefs[] = {
        { "a_synth_duty",            "Push"      },
        { "a_synth_h1",              "H1"        },
        { "a_synth_sub",             "Sub"       },
        { "a_synth_wavelet_length",  "Length"    },
        { "a_synth_gate_threshold",  "Gate"      },
        { "a_synth_min_samples",     "Min"       },
        { "a_synth_max_samples",     "Max"       },
        { "a_tracking_speed",        "Track"     },
        { "a_punch_amount",          "Amount"    },
        { "a_synth_boost_threshold", "Threshold" },
        { "a_synth_boost_amount",    "Boost"     },
        { "a_env_attack_ms",         "Attack"    },
        { "a_env_release_ms",        "Release"   },
        { "a_binaural_width",        "Width"     },
    };
    static_assert(sizeof(miniDefs) / sizeof(miniDefs[0]) == 14, "14 mini knobs expected");

    for (size_t i = 0; i < miniKnobs.size(); ++i)
    {
        miniKnobs[i] = std::make_unique<PhantomMiniKnob>(apvts, miniDefs[i].paramID, miniDefs[i].label);
        addAndMakeVisible(*miniKnobs[i]);
    }
}
```

Update `paint()` to add a section header for "Advanced":

```cpp
void RightPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    drawSectionHeader(g, juce::Rectangle<int>(12,   8, 200, 16), "Harmonic Engine");
    drawSectionHeader(g, juce::Rectangle<int>(310,  8, 100, 16), "Stereo");
    drawSectionHeader(g, juce::Rectangle<int>(420,  8, 200, 16), "Levels");
    drawSectionHeader(g, juce::Rectangle<int>(12, 130, 200, 16), "Advanced");
}
```

Update `resized()` to lay out the mini knobs in a row below the top knob row:

```cpp
void RightPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(28);

    // Top knob row (medium knobs)
    auto knobRow = area.removeFromTop(80).reduced(12, 0);
    const int knobWidth = 80;
    const int gap = 8;
    const int sectionGap = 24;

    auto layoutKnob = [&](PhantomKnob& k) {
        k.setBounds(knobRow.removeFromLeft(knobWidth));
        knobRow.removeFromLeft(gap);
    };

    layoutKnob(saturationKnob);
    layoutKnob(shapeKnob);
    layoutKnob(skipKnob);
    knobRow.removeFromLeft(sectionGap);
    layoutKnob(widthKnob);
    knobRow.removeFromLeft(sectionGap);
    layoutKnob(inGainKnob);
    layoutKnob(outGainKnob);

    // Advanced mini-knob row
    area.removeFromTop(20);  // section header space
    auto miniRow = area.removeFromTop(60).reduced(12, 0);
    const int miniWidth = 36;
    const int miniGap = 4;
    for (auto& mk : miniKnobs)
    {
        mk->setBounds(miniRow.removeFromLeft(miniWidth));
        miniRow.removeFromLeft(miniGap);
    }
}
```

- [ ] **Step 3: Build and manually verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: 14 small mini-knobs appear in a row below the top knob row, each labeled. Drag each → APVTS parameter updates. (Some param IDs may not exist exactly as listed — if you get a `getParameter()` returning null and the knob doesn't bind, double-check `Source/Parameters.h` for the right name.)

- [ ] **Step 4: Commit**

```bash
git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp
git commit -m "feat(path-b): AdvancedPanel mini-knob row in RightPanel (14 knobs)"
```

---

## Task 8: ToggleGroup widget

**Files:**
- Create: `Source/UI/widgets/ToggleGroup.h`
- Create: `Source/UI/widgets/ToggleGroup.cpp`
- Modify: `CMakeLists.txt`

Multi-button radio control. The current `index.html` uses these for ghost mode (Replace / Combine / Phantom Only), filter slope (-6 / -12 / -24), and binaural mode (Off / Spread / Voice-Split). All three back onto `juce::AudioParameterChoice` parameters.

Implementation strategy: own a hidden `juce::ComboBox` with the same items as the visible buttons; the buttons toggle the combo's selectedItemIndex; a `ComboBoxParameterAttachment` keeps the combo synced with APVTS.

- [ ] **Step 1: Create Source/UI/widgets/ToggleGroup.h**

```cpp
// Source/UI/widgets/ToggleGroup.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

class ToggleGroup : public juce::Component, private juce::ComboBox::Listener
{
public:
    ToggleGroup(juce::AudioProcessorValueTreeState& apvts,
                juce::StringRef paramID,
                const juce::StringArray& labels);
    ~ToggleGroup() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void comboBoxChanged(juce::ComboBox* c) override;
    void buttonClicked(int index);

    juce::ComboBox combo;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;
    juce::OwnedArray<juce::TextButton> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToggleGroup)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/ToggleGroup.cpp**

```cpp
// Source/UI/widgets/ToggleGroup.cpp
#include "ToggleGroup.h"
#include "../Theme.h"

namespace kaigen::phantom
{

ToggleGroup::ToggleGroup(juce::AudioProcessorValueTreeState& apvts,
                         juce::StringRef paramID,
                         const juce::StringArray& labels)
{
    combo.setVisible(false);
    addChildComponent(combo);
    for (int i = 0; i < labels.size(); ++i)
        combo.addItem(labels[i], i + 1);  // ComboBox item IDs are 1-indexed
    combo.addListener(this);

    if (auto* param = apvts.getParameter(paramID))
        attachment = std::make_unique<juce::ComboBoxParameterAttachment>(*param, combo);

    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = new juce::TextButton(labels[i]);
        b->setClickingTogglesState(false);
        b->onClick = [this, i] { buttonClicked(i); };
        addAndMakeVisible(*b);
        buttons.add(b);
    }
}

ToggleGroup::~ToggleGroup()
{
    combo.removeListener(this);
}

void ToggleGroup::paint(juce::Graphics& g)
{
    // Highlight the active button. Active = combo.getSelectedItemIndex().
    const int active = combo.getSelectedItemIndex();
    for (int i = 0; i < buttons.size(); ++i)
    {
        auto bounds = buttons[i]->getBounds().toFloat();
        if (i == active)
        {
            g.setColour(Theme::activeGlow);
            g.fillRoundedRectangle(bounds.expanded(2.0f), 4.0f);
        }
    }
}

void ToggleGroup::resized()
{
    auto area = getLocalBounds();
    if (buttons.isEmpty()) return;
    const int btnWidth = area.getWidth() / buttons.size();
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds(area.removeFromLeft(btnWidth));
}

void ToggleGroup::buttonClicked(int index)
{
    combo.setSelectedItemIndex(index, juce::sendNotificationSync);
}

void ToggleGroup::comboBoxChanged(juce::ComboBox*)
{
    repaint();  // active highlight follows the combo
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `Source/UI/widgets/ToggleGroup.cpp` to `target_sources(KaigenPhantom PRIVATE`.

- [ ] **Step 4: Build to verify compile**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/widgets/ToggleGroup.h Source/UI/widgets/ToggleGroup.cpp CMakeLists.txt
git commit -m "feat(path-b): ToggleGroup widget (radio buttons + APVTS choice attachment)"
```

---

## Task 9: LinkButton widget

**Files:**
- Create: `Source/UI/widgets/LinkButton.h`
- Create: `Source/UI/widgets/LinkButton.cpp`
- Modify: `CMakeLists.txt`

Toggle button rendering the chain-link SVG icon. The current WebView2 implementation is a UI-side toggle (filter LPF/HPF link state stored in JS) with no APVTS backing — the link causes coupled drag behavior in JS but doesn't persist. We'll preserve that pattern: `LinkButton` is a stateful toggle whose state is held in a public boolean (or callback signal) rather than in APVTS.

- [ ] **Step 1: Create Source/UI/widgets/LinkButton.h**

```cpp
// Source/UI/widgets/LinkButton.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace kaigen::phantom
{

class LinkButton : public juce::Component
{
public:
    LinkButton();
    ~LinkButton() override;

    /** Whether the link is currently active. Defaults false. */
    bool isLinked() const { return linked; }
    void setLinked(bool shouldBeLinked);

    /** Called on click toggle. */
    std::function<void(bool)> onLinkChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    bool linked { false };
    std::unique_ptr<juce::Drawable> icon;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinkButton)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/LinkButton.cpp**

```cpp
// Source/UI/widgets/LinkButton.cpp
#include "LinkButton.h"
#include "../Theme.h"
#include "BinaryData.h"

namespace kaigen::phantom
{

LinkButton::LinkButton()
{
    icon = juce::Drawable::createFromImageData(BinaryData::link_icon_svg, BinaryData::link_icon_svgSize);
    setSize(28, 28);
}

LinkButton::~LinkButton() = default;

void LinkButton::setLinked(bool shouldBeLinked)
{
    if (linked != shouldBeLinked)
    {
        linked = shouldBeLinked;
        if (onLinkChanged) onLinkChanged(linked);
        repaint();
    }
}

void LinkButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Background — tinted when active.
    g.setColour(linked ? Theme::activeGlow : Theme::panelBg.withAlpha(0.4f));
    g.fillRoundedRectangle(bounds, 4.0f);

    if (icon != nullptr)
    {
        // Icon tint follows linked state.
        icon->replaceColour(juce::Colours::black, linked ? Theme::steelBlue : Theme::textSecondary);
        icon->setTransformToFit(bounds.reduced(4.0f), juce::RectanglePlacement::centred);
        icon->draw(g, 1.0f);
    }
}

void LinkButton::resized() {}

void LinkButton::mouseDown(const juce::MouseEvent&)
{
    setLinked(!linked);
}

} // namespace kaigen::phantom
```

(Note: SVG fill colour replacement uses `juce::Drawable::replaceColour`. The `link_icon.svg` in Task 2 uses `stroke="currentColor"` — JUCE's SVG renderer treats `currentColor` as falling back to the foreground colour. If `replaceColour` doesn't have the desired effect, fall back to drawing the icon with `g.setColour()` followed by `icon->drawAt(...)`.)

- [ ] **Step 3: Add to CMakeLists.txt**

Add `Source/UI/widgets/LinkButton.cpp` to `target_sources(KaigenPhantom PRIVATE`.

- [ ] **Step 4: Build to verify compile**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/widgets/LinkButton.h Source/UI/widgets/LinkButton.cpp CMakeLists.txt
git commit -m "feat(path-b): LinkButton widget for filter LPF/HPF link"
```

---

## Task 10: Auto-gain button + drop into LevelsSection

**Files:**
- Modify: `Source/UI/panels/RightPanel.h`
- Modify: `Source/UI/panels/RightPanel.cpp`

The Auto-gain button is a simple toggle backed by the `input_gain_auto` boolean APVTS parameter. We can use a plain `juce::TextButton` with `juce::ButtonParameterAttachment` — no custom widget needed. Place it in the Levels section header area.

- [ ] **Step 1: Update RightPanel.h**

Add the Auto button member:

```cpp
// In the private section of RightPanel:
juce::TextButton autoGainButton { "Auto" };
std::unique_ptr<juce::ButtonParameterAttachment> autoGainAttachment;
```

- [ ] **Step 2: Update RightPanel.cpp constructor**

Add to the constructor body (after existing addAndMakeVisible calls, before the mini knob loop):

```cpp
autoGainButton.setClickingTogglesState(true);
addAndMakeVisible(autoGainButton);
if (auto* param = apvts.getParameter("input_gain_auto"))
    autoGainAttachment = std::make_unique<juce::ButtonParameterAttachment>(*param, autoGainButton);
```

- [ ] **Step 3: Update resized() to position the Auto button**

Add at the end of `resized()`:

```cpp
// Auto button — top-right of Levels section header.
autoGainButton.setBounds(580, 6, 40, 18);
```

(Adjust X coordinate to match the actual Levels-section position from your section-header code. The button sits to the right of the "Levels" label.)

- [ ] **Step 4: Build and manually verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: a small "Auto" button appears next to the Levels section. Click → button toggles state, the `input_gain_auto` parameter follows, the In knob may stop responding to manual changes (existing audio behavior).

- [ ] **Step 5: Commit**

```bash
git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp
git commit -m "feat(path-b): Auto-gain button in LevelsSection"
```

---

## Task 11: LeftPanel + GhostSection (Amount, Crossover, Strength + ghost mode toggle)

**Files:**
- Create: `Source/UI/panels/LeftPanel.h`
- Create: `Source/UI/panels/LeftPanel.cpp`
- Modify: `Source/UI/NativePluginEditor.h`
- Modify: `Source/UI/NativePluginEditor.cpp`
- Modify: `CMakeLists.txt`

`LeftPanel` houses the GhostSection and FilterSection. Phase 1 doesn't include the recipe wheel (that's Phase 3) — its area stays as a labeled placeholder rectangle. This task adds the GhostSection: Amount knob (large), Crossover knob (medium), Strength knob (medium), and the ghost mode toggle group (Replace / Combine / Phantom Only).

- [ ] **Step 1: Create Source/UI/panels/LeftPanel.h**

```cpp
// Source/UI/panels/LeftPanel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/ToggleGroup.h"

namespace kaigen::phantom
{

class LeftPanel : public juce::Component
{
public:
    explicit LeftPanel(juce::AudioProcessorValueTreeState& apvts);
    ~LeftPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Recipe wheel area — placeholder until Phase 3.
    // Ghost section
    PhantomKnob ghostAmountKnob;
    PhantomKnob crossoverKnob;
    PhantomKnob strengthKnob;
    ToggleGroup ghostModeToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LeftPanel)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/panels/LeftPanel.cpp**

```cpp
// Source/UI/panels/LeftPanel.cpp
#include "LeftPanel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        g.setColour(Theme::textSecondary);
        g.setFont(juce::FontOptions("Space Grotesk", 11.0f, juce::Font::bold));
        g.drawText(title, bounds.toFloat(), juce::Justification::centredLeft, false);
    }
}

LeftPanel::LeftPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      ghostAmountKnob(apvts, "a_ghost",              PhantomKnob::Size::Large,  "Amount"),
      crossoverKnob  (apvts, "a_phantom_threshold",  PhantomKnob::Size::Medium, "Crossover"),
      strengthKnob   (apvts, "a_phantom_strength",   PhantomKnob::Size::Medium, "Strength"),
      ghostModeToggle(apvts, "a_ghost_mode", { "Replace", "Combine", "Phantom Only" })
{
    addAndMakeVisible(ghostAmountKnob);
    addAndMakeVisible(crossoverKnob);
    addAndMakeVisible(strengthKnob);
    addAndMakeVisible(ghostModeToggle);
}

LeftPanel::~LeftPanel() = default;

void LeftPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    // Recipe wheel placeholder (top half) — until Phase 3.
    auto recipeArea = juce::Rectangle<int>(8, 8, getWidth() - 16, 360);
    g.setColour(Theme::matrixBg);
    g.fillRect(recipeArea);
    g.setColour(Theme::panelBorder);
    g.drawRect(recipeArea, 1);
    g.setColour(Theme::textDim);
    g.setFont(juce::FontOptions("Space Grotesk", 12.0f, juce::Font::plain));
    g.drawText("Recipe Wheel (Phase 3)", recipeArea.toFloat(), juce::Justification::centred, false);

    // Ghost section header
    drawSectionHeader(g, juce::Rectangle<int>(12, 376, 200, 16), "Ghost");
}

void LeftPanel::resized()
{
    // Recipe wheel placeholder occupies top 360px.
    // Ghost section starts at y=400.
    constexpr int ghostY = 400;
    ghostAmountKnob.setBounds(12, ghostY, 90, 100);
    crossoverKnob  .setBounds(110, ghostY, 80, 100);
    strengthKnob   .setBounds(200, ghostY, 80, 100);
    ghostModeToggle.setBounds(12, ghostY + 110, 270, 26);
}

} // namespace kaigen::phantom
```

(Note: parameter ID `a_ghost_mode` should be a `juce::AudioParameterChoice`. Verify in `Source/Parameters.h`. If it's a different name (`a_ghostMode`, etc.), use the actual ID. The toggle group needs the param to be a Choice — if it's something else, the attachment will fail silently and the group won't bind.)

- [ ] **Step 3: Add LeftPanel to NativePluginEditor**

Modify `Source/UI/NativePluginEditor.h` to include `panels/LeftPanel.h` and add a `LeftPanel leftPanel;` member.

Modify `Source/UI/NativePluginEditor.cpp`:
- Constructor initializer adds `, leftPanel(a)` after `rightPanel(a)`.
- Constructor body adds `addAndMakeVisible(leftPanel);`.
- `resized()` sets `leftPanel.setBounds(<left half of MainArea>)` — same area as the wireframe `leftPanel` rectangle.
- `paint()` removes the labeled "LeftPanel" rectangle since the real Component now occupies that area.

```cpp
void NativePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::deepBg);

    auto area = getLocalBounds();

    auto topBar = area.removeFromTop(topBarHeight);
    drawLabeledPanel(g, topBar, "TopBar (preset selector + advanced toggle)");

    auto modPanel = area.removeFromBottom(modPanelHeight);
    drawLabeledPanel(g, modPanel, "ModulationPanel (mode bar + slot row + engine labels)");

    // LeftPanel and RightPanel are real Components now; no wireframe rectangles for them.
}

void NativePluginEditor::resized()
{
    backToWebViewButton.setBounds(getWidth() - 110, 10, 100, 26);

    auto area = getLocalBounds();
    area.removeFromTop(topBarHeight);
    area.removeFromBottom(modPanelHeight);

    auto leftBounds = area.removeFromLeft(leftPanelWidth);
    leftPanel.setBounds(leftBounds);
    rightPanel.setBounds(area);
}
```

- [ ] **Step 4: Add to CMakeLists.txt**

Add `Source/UI/panels/LeftPanel.cpp` to `target_sources(KaigenPhantom PRIVATE`.

- [ ] **Step 5: Build and manually verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected:
- LeftPanel area (left 420px) shows a placeholder "Recipe Wheel (Phase 3)" rectangle on top
- Below it, "Ghost" section header
- Three knobs in a row: Amount (large), Crossover (medium), Strength (medium)
- Below the knobs, three toggle buttons: Replace / Combine / Phantom Only
- Click a toggle → corresponding `a_ghost_mode` parameter updates, active highlight shifts

- [ ] **Step 6: Commit**

```bash
git add Source/UI/panels/LeftPanel.h Source/UI/panels/LeftPanel.cpp Source/UI/NativePluginEditor.h Source/UI/NativePluginEditor.cpp CMakeLists.txt
git commit -m "feat(path-b): LeftPanel + GhostSection (Amount/Crossover/Strength + mode toggle)"
```

---

## Task 12: FilterSection in LeftPanel (LPF, HPF, LinkButton, slope toggle)

**Files:**
- Modify: `Source/UI/panels/LeftPanel.h`
- Modify: `Source/UI/panels/LeftPanel.cpp`

Add the filter section: LPF + HPF medium knobs flanking the LinkButton, with a slope ToggleGroup below.

- [ ] **Step 1: Update LeftPanel.h**

Add filter members:

```cpp
// Source/UI/panels/LeftPanel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/ToggleGroup.h"
#include "../widgets/LinkButton.h"

namespace kaigen::phantom
{

class LeftPanel : public juce::Component
{
public:
    explicit LeftPanel(juce::AudioProcessorValueTreeState& apvts);
    ~LeftPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    PhantomKnob ghostAmountKnob;
    PhantomKnob crossoverKnob;
    PhantomKnob strengthKnob;
    ToggleGroup ghostModeToggle;

    PhantomKnob lpfKnob;
    PhantomKnob hpfKnob;
    LinkButton  filterLinkBtn;
    ToggleGroup filterSlopeToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LeftPanel)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Update LeftPanel.cpp constructor**

Add the new member initializers and `addAndMakeVisible` calls:

```cpp
LeftPanel::LeftPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      ghostAmountKnob (apvts, "a_ghost",              PhantomKnob::Size::Large,  "Amount"),
      crossoverKnob   (apvts, "a_phantom_threshold",  PhantomKnob::Size::Medium, "Crossover"),
      strengthKnob    (apvts, "a_phantom_strength",   PhantomKnob::Size::Medium, "Strength"),
      ghostModeToggle (apvts, "a_ghost_mode", { "Replace", "Combine", "Phantom Only" }),
      lpfKnob         (apvts, "a_synth_lpf_hz",       PhantomKnob::Size::Medium, "LPF"),
      hpfKnob         (apvts, "a_synth_hpf_hz",       PhantomKnob::Size::Medium, "HPF"),
      filterSlopeToggle(apvts, "a_synth_filter_slope", { "-6", "-12", "-24" })
{
    addAndMakeVisible(ghostAmountKnob);
    addAndMakeVisible(crossoverKnob);
    addAndMakeVisible(strengthKnob);
    addAndMakeVisible(ghostModeToggle);

    addAndMakeVisible(lpfKnob);
    addAndMakeVisible(hpfKnob);
    addAndMakeVisible(filterLinkBtn);
    addAndMakeVisible(filterSlopeToggle);
}
```

- [ ] **Step 3: Update paint() to add Filter section header**

Add to `paint()`:

```cpp
// Filter section header — below ghost section (~y=550).
drawSectionHeader(g, juce::Rectangle<int>(12, 550, 200, 16), "Filter");
```

- [ ] **Step 4: Update resized() to lay out filter widgets**

Add at the end of `resized()`:

```cpp
constexpr int filterY = 574;
lpfKnob.setBounds         (12,  filterY,     80, 100);
filterLinkBtn.setBounds   (98,  filterY+30,  28, 28);
hpfKnob.setBounds         (130, filterY,     80, 100);
filterSlopeToggle.setBounds(12, filterY+110, 200, 26);
```

- [ ] **Step 5: Build and manually verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected:
- Below the Ghost section, a "Filter" section header
- LPF knob, link-icon button, HPF knob in a row
- Slope toggle below: -6 / -12 / -24
- Click LinkButton → tints; click slope toggle → updates `a_synth_filter_slope`

(LinkButton's `onLinkChanged` callback is unwired in this task — it's a UI-only toggle for now. Phase 2 or later wires the coupled-drag behavior between LPF and HPF; the visual toggle just records intent.)

- [ ] **Step 6: Commit**

```bash
git add Source/UI/panels/LeftPanel.h Source/UI/panels/LeftPanel.cpp
git commit -m "feat(path-b): FilterSection in LeftPanel (LPF/HPF + LinkButton + slope toggle)"
```

---

## Task 13: Manual smoke + final review

**Files:** none modified — verification only.

End-to-end smoke for Phase 1. Confirm everything works by manually exercising the native editor.

- [ ] **Step 1: Build VST3 + Standalone**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_VST3
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expect: both build clean. VST3 auto-installs.

- [ ] **Step 2: Toggle to native editor**

Open the WebView2 editor (Standalone or VST3 in Live). Shift+click PHANTOM logo → alert. Reopen plugin window → native editor appears.

- [ ] **Step 3: Verify all knobs respond and bind**

For each knob:
1. Drag → indicator rotates
2. Switch back to WebView2 (click "← WebView2" → reopen) → matching knob shows the same value
3. Switch back to native → value preserved

Knobs to verify:
- Ghost: Amount, Crossover, Strength
- Harmonic Engine: Saturation, Shape, Skip
- Filter: LPF, HPF
- Stereo: Width
- Levels: In, Out
- Advanced mini knobs: Push, H1, Sub, Length, Gate, Min, Max, Track, Amount, Threshold, Boost, Attack, Release, Width

- [ ] **Step 4: Verify toggle groups and buttons**

- Ghost mode toggle: click each option, verify active highlight moves and the WebView2 toggle reflects the same selection on toggle-back.
- Filter slope toggle: same, with -6/-12/-24.
- Auto button: click, verify toggle state, In-knob behavior changes (existing audio coupling).
- Link button: click, verify tint changes (functional behavior is UI-only this phase).

- [ ] **Step 5: Verify what's still wireframe vs. real**

Confirm the following remain wireframe placeholders (these are deferred to later phases):
- TopBar (preset selector + advanced toggle): wireframe rectangle still
- ModulationPanel (slot row): wireframe rectangle still
- Recipe wheel area inside LeftPanel: "Recipe Wheel (Phase 3)" placeholder
- No Spectrum, Oscilloscope, or IOMeter (Phase 2)
- No matrix view (Phase 5)

- [ ] **Step 6: If everything works, no commit needed**

This is verification only. Phase 1 is done. Phase 2 (visualizers) is next.

---

## Self-review notes

### Spec coverage

Phase 1 covers per the spec's "Phase 1 — Knob system" line item:
- ✓ `PhantomKnob` (large/medium/small variants) — Tasks 3 + 4
- ✓ `PhantomMiniKnob` — Task 5
- ✓ `ToggleGroup` — Task 8
- ✓ `LinkButton` — Task 9
- ✓ Baked knob/button assets — Tasks 1, 2
- ✓ APVTS attachments — Tasks 3, 5, 8 (built into each widget)
- ✓ Drop the new knobs into the foundation panels — Tasks 4, 6, 7, 10, 11, 12
- ✓ Most main-editor controls become functional — yes, except the recipe wheel (Phase 3) and visualizers (Phase 2)

### Placeholder scan

No "TBD" / "TODO" / vague-language. Each step has runnable code or commands.

### Type / signature consistency

- `PhantomKnob::Size` enum has 3 values used consistently across Tasks 3, 4, 6, 11.
- Constructor signature `PhantomKnob(apvts, paramID, Size, label)` consistent across all use sites.
- `PhantomMiniKnob(apvts, paramID, label)` consistent.
- `ToggleGroup(apvts, paramID, juce::StringArray)` consistent.
- `LinkButton()` default constructor consistent.
- Param ID prefixes (`a_*` for per-engine, plain `input_gain` for global) consistent. Param IDs in tasks should match the actual ones in `Source/Parameters.h` — implementer should verify against that file when binding doesn't work.

### Visual fidelity

Phase 1 ships placeholder SVG knobs (simple gradient circles). Visual polish to match the WebView2 CSS look is deferred to a later refinement task (before Phase 6 cutover). This was an explicit scoping choice — the spec said "Visual polish costs" should be measured against effort budget, and chasing pixel parity on knobs in Phase 1 would significantly extend the timeline.

### What ships in Phase 1

After this plan completes:
- Native editor's right panel: 6 medium knobs + Auto button + 14 mini knobs, all functional
- Native editor's left panel: 3 ghost knobs + ghost mode toggle + 2 filter knobs + LinkButton + filter slope toggle, all functional
- Recipe wheel area, TopBar, ModulationPanel still wireframes
- Spectrum / Oscilloscope / IOMeter still missing (Phase 2)

Engine-tab switching (A/B) is NOT wired — native editor displays Engine A only. LINK mode (writes to both engines on knob drag) is NOT wired. Both are deferred to a later phase.

Phase 2 picks up by adding the OpenGL-backed Spectrum and Oscilloscope, plus the IOMeter widget into the Levels section.
