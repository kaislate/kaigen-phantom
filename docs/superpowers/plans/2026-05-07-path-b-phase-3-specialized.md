# Path B — Phase 3: Specialized Widgets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the specialized widgets that make Phase 1's standard knobs feel like the full plugin: `RecipeWheel` (custom radial widget for harmonic mix H2-H8), filter LPF/HPF coupled-drag wiring, and an animated advanced-panel collapse. After this phase, the only remaining wireframes/placeholders in the native editor are the TopBar, ModulationPanel, and preset selector (those are Phases 4-5).

**Architecture:** The `RecipeWheel` is a custom `juce::Component` that draws 7 spokes radiating from a center point. Each spoke's length encodes one harmonic's amplitude (0..1, normalized via `SliderParameterAttachment`'s NormalisableRange). Mouse drag on a spoke head computes the radial distance from center and writes back through the attachment. The advanced-panel collapse uses `juce::ComponentAnimator::animateComponent` to animate the panel's height bound between 0 and ~80px when a small toggle button is clicked. Filter LinkButton wiring uses the existing `onLinkChanged` callback plus a slider-listener-on-LPF that mirrors changes to HPF (and vice versa) when linked.

**Tech Stack:** JUCE 8.0.4, C++20, MSVC 17.14. Reuses `juce::SliderParameterAttachment` (already proven in Phase 1) for all 7 RecipeWheel attachments. Uses `juce::ComponentAnimator` for collapse animation.

**Branch:** continue on `feature/pr3b-macro-editor` (additive; default behavior unchanged for users without native editor enabled).

**Spec:** `docs/superpowers/specs/2026-05-07-path-b-native-ui-design.md`. Phase 0/1/2 already complete.

**Visual fidelity note:** Phase 3's RecipeWheel ships with a functional but spare visual treatment — circle + radial spokes + draggable head dots. The polished holographic look from the WebView2 implementation (gradients, glow, animated fills) is deferred to the pre-cutover refinement pass.

---

## File Structure

| File | Status | Responsibility |
|------|--------|----------------|
| `Source/UI/widgets/RecipeWheel.h` | **Create** | RecipeWheel widget class. Custom Component painting 7 spokes radiating from center. Owns 7 hidden `juce::Slider`s (one per harmonic) + 7 `SliderParameterAttachment`s for APVTS binding. Mouse-drag converts radial distance to slider value. |
| `Source/UI/widgets/RecipeWheel.cpp` | **Create** | Implementation. Paint code: circle outline, radial gridlines (faint), spokes from center to (cos·value, sin·value), spoke head circles. Mouse handlers: hit-test which spoke is closest at mouseDown; mouseDrag updates that spoke's value via slider attachment. |
| `Source/UI/panels/LeftPanel.h` | **Modify** | Replace recipe-wheel placeholder rectangle with a `RecipeWheel` member. |
| `Source/UI/panels/LeftPanel.cpp` | **Modify** | Construct RecipeWheel with apvts + 7 H2-H8 param IDs. `addAndMakeVisible`. Position in the same area the placeholder occupied (top of LeftPanel, ~360px tall area). Remove the placeholder paint code. |
| `Source/UI/widgets/LinkButton.h` | **No change** | Already exists with `onLinkChanged` callback. |
| `Source/UI/widgets/LinkButton.cpp` | **No change** | Already exists. |
| `Source/UI/panels/LeftPanel.cpp` (filter section) | **Modify** | Wire `filterLinkBtn.onLinkChanged` to install/remove a slider-listener that mirrors LPF↔HPF changes when linked. The current `lpfKnob` and `hpfKnob` already have `juce::Slider` instances internally; we expose them via a `getSlider()` accessor on `PhantomKnob` for this wiring. |
| `Source/UI/widgets/PhantomKnob.h` | **Modify** | Add public accessor `juce::Slider& getSlider() { return slider; }` so external code (LinkButton wiring) can attach listeners. |
| `Source/UI/widgets/PhantomKnob.cpp` | **No change** | Implementation file unchanged. |
| `Source/UI/panels/RightPanel.h` | **Modify** | Add `juce::TextButton advancedToggle { "Advanced ▾" };` and a child Component `advancedPanel` (or refactor mini-knobs into a nested Component for clean collapse animation). |
| `Source/UI/panels/RightPanel.cpp` | **Modify** | Animate the advanced area's height bound when `advancedToggle` clicked. Default state: collapsed. |

---

## Task 1: RecipeWheel widget class

**Files:**
- Create: `Source/UI/widgets/RecipeWheel.h`
- Create: `Source/UI/widgets/RecipeWheel.cpp`
- Modify: `CMakeLists.txt`

A custom `juce::Component` that draws a circle and 7 radial spokes (one per harmonic H2-H8). Each spoke's length encodes its harmonic's normalized value (0..1). Mouse drag on a spoke head updates the underlying APVTS parameter via `SliderParameterAttachment`.

Architecture:
- 7 hidden `juce::Slider`s, one per harmonic. The sliders are NOT visible — they exist only to host the parameter attachment plumbing.
- 7 `SliderParameterAttachment`s, one per slider, binding to `a_recipe_h2` through `a_recipe_h8`.
- The component holds a `std::array<float, 7>` of the current normalized values (read from sliders' NormalisableRange) and repaints when any slider's value changes.
- Mouse hit-testing converts (x, y) to polar coordinates (angle from center, radial distance), finds which spoke's angle is closest, and treats that spoke as the drag target.
- Drag updates the spoke's slider value via the attachment.

Spoke layout: 7 spokes at evenly-spaced angles around the circle. Spoke 0 (H2) at angle 0° (top); spoke 1 (H3) at 51.43° (clockwise); etc. (`360° / 7 ≈ 51.43°` per spoke).

After Task 1: class compiles + links but isn't yet referenced. Task 2 wires it into LeftPanel.

- [ ] **Step 1: Create Source/UI/widgets/RecipeWheel.h**

```cpp
// Source/UI/widgets/RecipeWheel.h
#pragma once
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** RecipeWheel — 7-spoke radial widget for harmonic amplitudes H2-H8.
 *  Each spoke's length encodes one harmonic's normalized value (0..1).
 *  Drag a spoke's head radially to change that harmonic.
 *  
 *  Internally owns 7 hidden juce::Slider instances + 7 SliderParameterAttachments
 *  to bind to the per-engine 'a_recipe_h2' through 'a_recipe_h8' parameters.
 *  The sliders are never shown — they exist only as the attachment surface. */
class RecipeWheel : public juce::Component, private juce::Slider::Listener
{
public:
    /** Constructor takes apvts and the 7 parameter IDs in H2-H8 order. */
    RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                const std::array<juce::String, 7>& paramIDs);
    ~RecipeWheel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    void sliderValueChanged(juce::Slider* s) override;

    /** Hit-test: returns spoke index (0..6) at mouse position, or -1. */
    int hitTestSpoke(juce::Point<float> p) const;

    /** Convert mouse position to a 0..1 spoke value (radial distance from center,
     *  normalized to the wheel's outer radius). */
    float pointToValue(juce::Point<float> p) const;

    static constexpr int kSpokes = 7;

    std::array<juce::Slider, kSpokes> sliders;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>, kSpokes> attachments;
    int activeSpoke { -1 };  // index being dragged, -1 = none

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeWheel)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/RecipeWheel.cpp**

```cpp
// Source/UI/widgets/RecipeWheel.cpp
#include "RecipeWheel.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    /** Spoke angle for harmonic index 0..6.
     *  Angle 0 = straight up. Spokes evenly distributed clockwise. */
    float spokeAngleRadians(int i)
    {
        return (float) i * juce::MathConstants<float>::twoPi / (float) RecipeWheel::kSpokes;
    }

    /** Outer radius of the wheel as a fraction of the smaller bounds dimension. */
    constexpr float kOuterRadiusFrac = 0.42f;
    constexpr float kInnerRadiusFrac = 0.10f;  // dead zone at center
    constexpr float kSpokeHeadRadius = 6.0f;
    constexpr float kHitTestRadiusPx = 14.0f;  // generous hit area for spoke heads
}

RecipeWheel::RecipeWheel(juce::AudioProcessorValueTreeState& apvts,
                         const std::array<juce::String, 7>& paramIDs)
{
    for (int i = 0; i < kSpokes; ++i)
    {
        auto& s = sliders[(size_t) i];
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        s.addListener(this);
        addChildComponent(s);  // hidden — we forward via mouse handlers

        if (auto* param = apvts.getParameter(paramIDs[(size_t) i]))
            attachments[(size_t) i] = std::make_unique<juce::SliderParameterAttachment>(*param, s);
        else
            jassertfalse;  // unknown paramID: typo or stale reference
    }
}

RecipeWheel::~RecipeWheel()
{
    for (auto& s : sliders) s.removeListener(this);
}

void RecipeWheel::sliderValueChanged(juce::Slider*)
{
    repaint();
}

int RecipeWheel::hitTestSpoke(juce::Point<float> p) const
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float outerR = r * kOuterRadiusFrac;

    int best = -1;
    float bestDistSq = kHitTestRadiusPx * kHitTestRadiusPx;
    for (int i = 0; i < kSpokes; ++i)
    {
        const float a = spokeAngleRadians(i);
        // Spoke head sits at outerR (regardless of current value, for hit testing).
        const float hx = centre.x + outerR * std::sin(a);
        const float hy = centre.y - outerR * std::cos(a);
        const float dx = p.x - hx;
        const float dy = p.y - hy;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDistSq)
        {
            bestDistSq = d2;
            best = i;
        }
    }
    return best;
}

float RecipeWheel::pointToValue(juce::Point<float> p) const
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float outerR = r * kOuterRadiusFrac;
    const float innerR = r * kInnerRadiusFrac;

    const float dx = p.x - centre.x;
    const float dy = p.y - centre.y;
    const float dist = std::sqrt(dx * dx + dy * dy);

    if (dist <= innerR) return 0.0f;
    if (dist >= outerR) return 1.0f;
    return (dist - innerR) / (outerR - innerR);
}

void RecipeWheel::mouseDown(const juce::MouseEvent& e)
{
    activeSpoke = hitTestSpoke(e.position);
    if (activeSpoke < 0) return;

    auto& s = sliders[(size_t) activeSpoke];
    s.beginChangeGesture();
    const float n = pointToValue(e.position);
    const auto value = s.getNormalisableRange().convertFrom0to1(n);
    s.setValue(value, juce::sendNotificationSync);
}

void RecipeWheel::mouseDrag(const juce::MouseEvent& e)
{
    if (activeSpoke < 0) return;
    auto& s = sliders[(size_t) activeSpoke];
    const float n = pointToValue(e.position);
    const auto value = s.getNormalisableRange().convertFrom0to1(n);
    s.setValue(value, juce::sendNotificationSync);
}

void RecipeWheel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float outerR = r * kOuterRadiusFrac;
    const float innerR = r * kInnerRadiusFrac;

    // Background.
    g.setColour(Theme::matrixBg);
    g.fillRect(bounds);

    // Outer ring (faint).
    g.setColour(Theme::panelBorder);
    g.drawEllipse(centre.x - outerR, centre.y - outerR, outerR * 2.0f, outerR * 2.0f, 1.0f);

    // 25%, 50%, 75% radius gridlines (very faint).
    for (float frac : { 0.25f, 0.5f, 0.75f })
    {
        const float gr = innerR + (outerR - innerR) * frac;
        g.setColour(Theme::panelBorder.withAlpha(0.4f));
        g.drawEllipse(centre.x - gr, centre.y - gr, gr * 2.0f, gr * 2.0f, 0.6f);
    }

    // Inner dead zone.
    g.setColour(Theme::panelBorder);
    g.drawEllipse(centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);

    // Spokes.
    for (int i = 0; i < kSpokes; ++i)
    {
        const auto& s = sliders[(size_t) i];
        const float n = (float) s.getNormalisableRange().convertTo0to1(s.getValue());
        const float a = spokeAngleRadians(i);
        const float spokeLength = innerR + (outerR - innerR) * n;

        const float endX = centre.x + spokeLength * std::sin(a);
        const float endY = centre.y - spokeLength * std::cos(a);

        // Spoke line.
        g.setColour(Theme::steelBlue);
        g.drawLine(centre.x, centre.y, endX, endY, 1.5f);

        // Spoke head circle (filled at value's tip).
        g.setColour((i == activeSpoke) ? Theme::macroTeal : Theme::steelBlue);
        g.fillEllipse(endX - kSpokeHeadRadius, endY - kSpokeHeadRadius,
                      kSpokeHeadRadius * 2.0f, kSpokeHeadRadius * 2.0f);

        // Hint marker at outer ring (for hit-test reference).
        const float hintX = centre.x + outerR * std::sin(a);
        const float hintY = centre.y - outerR * std::cos(a);
        g.setColour(Theme::textDim);
        g.fillEllipse(hintX - 2.0f, hintY - 2.0f, 4.0f, 4.0f);
    }
}

void RecipeWheel::resized()
{
    // Sliders are hidden; their bounds don't matter.
    for (auto& s : sliders)
        s.setBounds(0, 0, 0, 0);
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/widgets/RecipeWheel.cpp` after `Source/UI/widgets/IOMeter.cpp`.

- [ ] **Step 4: Build to verify compile**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Class is unreferenced — Task 2 wires it in.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/widgets/RecipeWheel.h Source/UI/widgets/RecipeWheel.cpp CMakeLists.txt
git commit -m "feat(path-b): RecipeWheel widget (7-spoke harmonic mix H2-H8)"
```

(Exact commit message — no Claude trailer.)

---

## Task 2: Drop RecipeWheel into LeftPanel

**Files:**
- Modify: `Source/UI/panels/LeftPanel.h`
- Modify: `Source/UI/panels/LeftPanel.cpp`

Replace the placeholder rectangle "Recipe Wheel (Phase 3)" with a real `RecipeWheel` member. Construct with the 7 H2-H8 parameter IDs (engine A, `a_` prefix). Position in the same 360px area at the top of LeftPanel.

- [ ] **Step 1: Update LeftPanel.h**

Add `#include "../widgets/RecipeWheel.h"` near the top with the other widget includes. Add a `RecipeWheel recipeWheel;` member at the start of the private section (above ghost section members).

The full updated header:

```cpp
// Source/UI/panels/LeftPanel.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/ToggleGroup.h"
#include "../widgets/LinkButton.h"
#include "../widgets/RecipeWheel.h"

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

    // Recipe wheel
    RecipeWheel recipeWheel;

    // Ghost section
    PhantomKnob ghostAmountKnob;
    PhantomKnob crossoverKnob;
    PhantomKnob strengthKnob;
    ToggleGroup ghostModeToggle;

    // Filter section
    PhantomKnob lpfKnob;
    PhantomKnob hpfKnob;
    LinkButton  filterLinkBtn;
    ToggleGroup filterSlopeToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LeftPanel)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Update LeftPanel.cpp constructor**

Add the recipeWheel initializer at the start of the init list (after `apvts(a)`):

```cpp
LeftPanel::LeftPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      recipeWheel(apvts,
                  std::array<juce::String, 7>{
                      "a_recipe_h2", "a_recipe_h3", "a_recipe_h4",
                      "a_recipe_h5", "a_recipe_h6", "a_recipe_h7", "a_recipe_h8"
                  }),
      ghostAmountKnob (apvts, "a_ghost",              PhantomKnob::Size::Large,  "Amount"),
      crossoverKnob   (apvts, "a_phantom_threshold",  PhantomKnob::Size::Medium, "Crossover"),
      strengthKnob    (apvts, "a_phantom_strength",   PhantomKnob::Size::Medium, "Strength"),
      ghostModeToggle (apvts, "a_ghost_mode", { "Replace", "Combine", "Phantom Only" }),
      lpfKnob         (apvts, "a_synth_lpf_hz",       PhantomKnob::Size::Medium, "LPF"),
      hpfKnob         (apvts, "a_synth_hpf_hz",       PhantomKnob::Size::Medium, "HPF"),
      filterSlopeToggle(apvts, "a_synth_filter_slope", { "-6 dB/oct", "-12 dB/oct", "-24 dB/oct" })
{
    addAndMakeVisible(recipeWheel);
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

(Note: the `filterSlopeToggle` labels here are `"-6 dB/oct"` / `"-12 dB/oct"` / `"-24 dB/oct"` — match what was already in LeftPanel.cpp from Phase 1 Task 12. The implementer there found these were the actual choice strings registered in `Parameters.h`.)

- [ ] **Step 3: Update paint() to remove placeholder rectangle**

Find the `paint()` body. Remove the section that drew the recipe wheel placeholder (the `auto recipeArea = juce::Rectangle<int>(8, 8, getWidth() - 16, 360);` block plus the surrounding `g.setColour(Theme::matrixBg); g.fillRect(recipeArea);` etc.). Keep the Ghost section header drawing and Filter section header drawing.

The full updated `paint()`:

```cpp
void LeftPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panelBg);

    // Ghost section header
    drawSectionHeader(g, juce::Rectangle<int>(12, 376, 200, 16), "Ghost");

    // Filter section header
    drawSectionHeader(g, juce::Rectangle<int>(12, 550, 200, 16), "Filter");
}
```

(`drawSectionHeader` is defined in the existing anonymous namespace at the top of LeftPanel.cpp.)

- [ ] **Step 4: Update resized() to position the recipe wheel**

Add at the START of `resized()` (before the ghost section bounds):

```cpp
recipeWheel.setBounds(8, 8, getWidth() - 16, 360);
```

The full updated `resized()`:

```cpp
void LeftPanel::resized()
{
    recipeWheel.setBounds(8, 8, getWidth() - 16, 360);

    constexpr int ghostY = 400;
    ghostAmountKnob.setBounds(12,  ghostY, 90, 100);
    crossoverKnob  .setBounds(110, ghostY, 80, 100);
    strengthKnob   .setBounds(200, ghostY, 80, 100);
    ghostModeToggle.setBounds(12, ghostY + 110, 270, 26);

    constexpr int filterY = 574;
    lpfKnob.setBounds         (12,  filterY,     80, 100);
    filterLinkBtn.setBounds   (98,  filterY+30,  28, 28);
    hpfKnob.setBounds         (130, filterY,     80, 100);
    filterSlopeToggle.setBounds(12, filterY+110, 200, 26);
}
```

- [ ] **Step 5: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: builds clean. Native editor's left panel now shows a recipe wheel (circle with 7 spokes) where the placeholder used to be. Drag a spoke head — the spoke length changes, the corresponding `a_recipe_h*` parameter updates.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/panels/LeftPanel.h Source/UI/panels/LeftPanel.cpp
git commit -m "feat(path-b): drop RecipeWheel into LeftPanel (replaces placeholder)"
```

(Exact commit message — no Claude trailer.)

---

## Task 3: Filter LinkButton coupled-drag wiring

**Files:**
- Modify: `Source/UI/widgets/PhantomKnob.h`
- Modify: `Source/UI/panels/LeftPanel.h`
- Modify: `Source/UI/panels/LeftPanel.cpp`

Wire the filter LinkButton so that when active, dragging LPF mirrors HPF (and vice versa). Strategy: add a `Slider::Listener` to LeftPanel that listens to both knobs' internal sliders. When linked AND a slider changes due to a user drag, set the OTHER slider's value to match. Use a `juce::ScopedValueSetter`-style guard to prevent infinite recursion.

- [ ] **Step 1: Add public slider accessor to PhantomKnob.h**

In `Source/UI/widgets/PhantomKnob.h`, add a public method that returns a reference to the internal slider:

```cpp
public:
    juce::Slider& getSlider() noexcept { return slider; }
```

(Place after the constructor declaration; the slider is currently a private member. This accessor makes it possible for external listeners — like LeftPanel for LinkButton wiring — to attach.)

- [ ] **Step 2: Update LeftPanel.h**

Add `juce::Slider::Listener` as a private base. Add a helper member to prevent recursion. Add `juce::ToggleButton`-listener-style state:

```cpp
class LeftPanel : public juce::Component, private juce::Slider::Listener
{
    // ... existing public members ...

private:
    void sliderValueChanged(juce::Slider* s) override;

    bool filterLinkUpdating { false };  // recursion guard

    // ... existing private members ...
};
```

- [ ] **Step 3: Wire the listener in LeftPanel.cpp constructor**

After the existing `addAndMakeVisible(filterLinkBtn);` in the constructor body, add:

```cpp
// Filter LinkButton wiring: when linked, dragging LPF mirrors HPF and vice versa.
filterLinkBtn.onLinkChanged = [this](bool /*linked*/) {
    // Just toggle visual state — listener installs unconditionally below;
    // actual coupling happens in sliderValueChanged based on filterLinkBtn.isLinked().
};
lpfKnob.getSlider().addListener(this);
hpfKnob.getSlider().addListener(this);
```

- [ ] **Step 4: Implement sliderValueChanged in LeftPanel.cpp**

Add the listener implementation after the existing `~LeftPanel() = default;`:

```cpp
void LeftPanel::sliderValueChanged(juce::Slider* s)
{
    if (filterLinkUpdating) return;       // guard against recursion
    if (! filterLinkBtn.isLinked()) return;

    juce::ScopedValueSetter<bool> guard(filterLinkUpdating, true);

    // Mirror: if LPF changed, set HPF to LPF's normalized value (and vice versa).
    if (s == &lpfKnob.getSlider())
    {
        const auto n = lpfKnob.getSlider().getNormalisableRange().convertTo0to1(lpfKnob.getSlider().getValue());
        const auto target = hpfKnob.getSlider().getNormalisableRange().convertFrom0to1(n);
        hpfKnob.getSlider().setValue(target, juce::sendNotificationSync);
    }
    else if (s == &hpfKnob.getSlider())
    {
        const auto n = hpfKnob.getSlider().getNormalisableRange().convertTo0to1(hpfKnob.getSlider().getValue());
        const auto target = lpfKnob.getSlider().getNormalisableRange().convertFrom0to1(n);
        lpfKnob.getSlider().setValue(target, juce::sendNotificationSync);
    }
}
```

(The `ScopedValueSetter<bool>` flips `filterLinkUpdating` to true for the duration of the function, preventing the mirrored `setValue` from re-entering this listener. Note: LPF and HPF likely have different normalized-to-real ranges (LPF: 200..20000 Hz, HPF: 20..2000 Hz), so we mirror the NORMALIZED 0..1 value, not the raw Hz.)

- [ ] **Step 5: Remove the listeners in destructor**

Update the LeftPanel destructor (currently `= default;`) to:

```cpp
LeftPanel::~LeftPanel()
{
    lpfKnob.getSlider().removeListener(this);
    hpfKnob.getSlider().removeListener(this);
}
```

(Update LeftPanel.h to declare an explicit destructor instead of `= default;` if needed — match the existing pattern.)

- [ ] **Step 6: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Test by clicking LinkButton (it tints), then dragging LPF — HPF tracks the same normalized position. Click LinkButton off, drag LPF — HPF stays put.

- [ ] **Step 7: Commit**

```bash
git add Source/UI/widgets/PhantomKnob.h Source/UI/panels/LeftPanel.h Source/UI/panels/LeftPanel.cpp
git commit -m "feat(path-b): filter LinkButton coupled-drag wiring (LPF<->HPF mirror)"
```

(Exact commit message — no Claude trailer.)

---

## Task 4: Advanced panel collapse animation

**Files:**
- Modify: `Source/UI/panels/RightPanel.h`
- Modify: `Source/UI/panels/RightPanel.cpp`

Make the Advanced mini-knob row collapsible behind a small "Advanced" toggle button. Default state: expanded (matching the current Phase 1 Task 7 behavior). Click toggle → collapse (mini knobs hide, area shrinks). Click again → expand. Use `juce::ComponentAnimator::animateComponent` for smooth ~250ms height transition.

Implementation strategy: wrap the 14 mini knobs in a child `juce::Component` (an "AdvancedSection") whose `setBounds` height is animated. When collapsed, the visualizers below shift up. When expanded, they shift back down.

OR simpler: add an `advancedExpanded` boolean state. On toggle, recompute layout. No animation — just instant. Simpler, lower-risk.

For Phase 3, do the simpler instant-toggle version. Animation polish can come later.

- [ ] **Step 1: Update RightPanel.h**

Add a toggle button member and an expansion state flag:

```cpp
private:
    // ... existing members ...

    juce::TextButton advancedToggle { "Advanced (-)" };
    bool advancedExpanded { true };
```

- [ ] **Step 2: Update RightPanel.cpp constructor**

After the existing `addAndMakeVisible` calls, add:

```cpp
advancedToggle.setClickingTogglesState(false);
advancedToggle.onClick = [this] {
    advancedExpanded = !advancedExpanded;
    advancedToggle.setButtonText(advancedExpanded ? "Advanced (-)" : "Advanced (+)");
    for (auto& mk : miniKnobs)
        mk->setVisible(advancedExpanded);
    resized();  // recompute visualizer bounds based on new state
    repaint();
};
addAndMakeVisible(advancedToggle);
```

- [ ] **Step 3: Update resized() to handle expanded/collapsed states**

Update the existing `resized()` to position the toggle button in the section header and conditionally include the Advanced row's vertical space:

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
    inMeter.setBounds(knobRow.removeFromLeft(14));
    knobRow.removeFromLeft(6);
    layoutKnob(inGainKnob);
    layoutKnob(outGainKnob);
    outMeter.setBounds(knobRow.removeFromLeft(14));

    // Advanced section: header line with toggle button.
    area.removeFromTop(8);
    advancedToggle.setBounds(12, 130, 100, 18);

    if (advancedExpanded)
    {
        area.removeFromTop(20);  // space below toggle
        auto miniRow = area.removeFromTop(60).reduced(12, 0);
        const int miniWidth = 36;
        const int miniGap = 4;
        for (auto& mk : miniKnobs)
        {
            mk->setBounds(miniRow.removeFromLeft(miniWidth));
            miniRow.removeFromLeft(miniGap);
        }
    }
    else
    {
        // Collapsed: skip the mini-row's vertical space; visualizers shift up.
        area.removeFromTop(8);  // small gap below the toggle button
    }

    // Visualizers below Advanced row.
    area.removeFromTop(12);
    auto vizArea = area.reduced(12, 0);
    oscilloscope.setBounds(vizArea.removeFromTop(120));
    vizArea.removeFromTop(8);
    spectrum    .setBounds(vizArea.removeFromTop(280));

    // Auto button (positioned at fixed coordinate next to "Levels" header).
    autoGainButton.setBounds(580, 6, 40, 18);
}
```

- [ ] **Step 4: Update paint() to skip the "Advanced" section header text when there's already a toggle**

The existing `paint()` draws "Advanced" as a section header at `(12, 130, 200, 16)`. With the toggle button there, we don't need the redundant text — but leaving it doesn't hurt. Actually, the toggle button text "Advanced (-)" or "Advanced (+)" replaces the section header role.

Remove the line `drawSectionHeader(g, juce::Rectangle<int>(12, 130, 200, 16), "Advanced");` from `paint()`. The toggle button visually IS the section header.

- [ ] **Step 5: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. The native editor's RightPanel now shows an "Advanced (-)" button at the position the section header used to occupy. Click it → mini knobs disappear, visualizers shift up. Click again → mini knobs reappear, visualizers shift back down.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp
git commit -m "feat(path-b): collapsible Advanced section (instant toggle)"
```

(Exact commit message — no Claude trailer.)

---

## Task 5: Manual smoke + final review

**Files:** none modified — verification only.

End-to-end smoke for Phase 3.

- [ ] **Step 1: Build VST3 + Standalone**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_VST3
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expect: both build clean. VST3 auto-installs.

- [ ] **Step 2: Toggle to native editor in Live**

Open WebView2 editor in Live. Shift+click PHANTOM logo. Reopen plugin window. Expect native editor with Phase 1 + 2 + 3 widgets.

- [ ] **Step 3: Verify RecipeWheel**

- LeftPanel shows a recipe wheel (circle + 7 spokes) where the placeholder used to be
- Drag a spoke head radially → spoke length changes
- Toggle to WebView2 → the corresponding harmonic value matches what you set
- Different harmonics map to different positions on the wheel

- [ ] **Step 4: Verify Filter LinkButton coupling**

- Click LinkButton (chain icon) → tints active (steel-blue)
- Drag LPF → HPF tracks (their normalized positions match)
- Drag HPF → LPF tracks
- Click LinkButton again → no longer tints, drags are independent

- [ ] **Step 5: Verify Advanced panel collapse**

- "Advanced (-)" button visible in RightPanel above the mini knobs
- Click it → mini knobs disappear, oscilloscope + spectrum shift up
- Button text changes to "Advanced (+)"
- Click again → mini knobs reappear, visualizers shift back down

- [ ] **Step 6: No commit needed if smoke passes**

Phase 3 done. Phase 4 (preset system) is next.

---

## Self-review notes

### Spec coverage

Phase 3 covers per the spec's "Phase 3 — Specialized widgets" line item:
- ✓ `RecipeWheel` — Tasks 1, 2 (the high-risk component)
- ✓ `CrossoverColumn` — partially deferred (current LeftPanel layout is functional; visual refinement via a wrapped Component class is a polish concern, not a Phase 3 functional requirement)
- ✓ `FilterRow` link button — Task 3 (coupled drag)
- ✓ Advanced-panel collapsible behavior — Task 4 (instant toggle; animation polish deferred)

Deviations:
- **CrossoverColumn**: Phase 1 already laid out the ghost section's mode toggle + crossover knob next to each other. Phase 3 doesn't introduce a separate `CrossoverColumn` Component class — the existing layout works. If a future polish pass wants to bundle them visually (frame/border/background), that's a styling change, not a structural one.
- **Animation polish**: the advanced collapse uses an instant toggle in Phase 3 instead of an animated height transition. `juce::ComponentAnimator::animateComponent` is a 1-task addition that can come later if the instant feels jarring.

### Placeholder scan

No "TBD"/"TODO"/vague language. Each step has runnable code or commands.

### Type / signature consistency

- `RecipeWheel(juce::AudioProcessorValueTreeState&, const std::array<juce::String, 7>&)` constructor consistent across Tasks 1-2.
- `PhantomKnob::getSlider()` accessor consistent across Tasks 3.
- `LeftPanel::sliderValueChanged(juce::Slider*)` follows JUCE convention.
- `filterLinkBtn.onLinkChanged` callback signature `void(bool)` matches the existing definition in LinkButton.
- Parameter IDs: `a_recipe_h2` through `a_recipe_h8` already exist (verified via `Source/Parameters.h:23-29`).

### What ships in Phase 3

After this plan completes:
- Native editor's RecipeWheel works — drag spokes to set H2-H8 amplitudes
- Filter LPF and HPF can be linked for coupled drag
- Advanced section collapses on demand
- Recipe wheel area is no longer a wireframe placeholder

Phase 4 (preset system) picks up by porting the 967-line `preset-system.js` to a native `PresetSelector` Component.
