# Path B — Phase 2: Visualizers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the three visualizer components to the native editor — `IOMeter` (vertical dB-scaled bar, flanking the In/Out knobs in LevelsSection), `Oscilloscope` (waveform display), and `Spectrum` (frequency bars). All read existing audio-thread atomic snapshots via lock-free loads; no DSP changes.

**Architecture:** Pure `juce::Component` painting (no OpenGL for Phase 2 — a follow-up phase can swap to `juce::OpenGLContext` if profiling shows pure paint is too slow). Each visualizer owns a `juce::Timer` that triggers `repaint()` at 30 fps. The audio thread continues writing into the existing atomic snapshot arrays; the visualizers read those arrays on the message thread during `paint()`. No new threads, no new locks.

**Tech Stack:** JUCE 8.0.4 (`juce_graphics`, `juce_gui_basics`), C++20 (MSVC 17.14), existing audio-thread atomic snapshots in `PluginProcessor.h` and `PhantomEngine.h`.

**Branch:** continue on `feature/pr3b-macro-editor` (additive; default behavior unchanged for users without native editor enabled).

**Spec:** `docs/superpowers/specs/2026-05-07-path-b-native-ui-design.md`. Phase 0 plan (foundation) and Phase 1 plan (knobs) already complete.

**Visual fidelity note:** Phase 2 visualizers ship with simple paint code matching the spec's layout intent but not pixel-perfect to the WebView2 styling. Visual polish (color gradients, peak-hold timing, smoothing curves) is part of the broader pre-cutover refinement pass.

---

## Existing infrastructure (DO NOT modify)

The audio thread already feeds atomic snapshot arrays that the visualizers will consume. From `Source/PluginProcessor.h`:

```cpp
// Peak meters (lines 65-68)
std::atomic<float> peakInL  { 0.0f };
std::atomic<float> peakInR  { 0.0f };
std::atomic<float> peakOutL { 0.0f };
std::atomic<float> peakOutR { 0.0f };

// Spectrum bins (lines 70-81)
static constexpr int kSpectrumBins = 80;
std::array<float, kSpectrumBins> spectrumData {};       // input
std::array<float, kSpectrumBins> spectrumOutputData {}; // output
std::array<std::atomic<float>, kSpectrumBins> engineASpectrum {};
std::array<std::atomic<float>, kSpectrumBins> engineBSpectrum {};

// Oscilloscope ring buffers (lines 89-91)
static constexpr int kOscBufSize = PhantomEngine::kOscBufSize;  // = 2048
std::array<std::atomic<float>, kOscBufSize> oscInputBuf  {};
std::array<std::atomic<float>, kOscBufSize> oscOutputBuf {};
```

And in `Source/Engines/PhantomEngine.h:85-86`:
```cpp
static constexpr int kOscBufSize = 2048;
std::array<std::atomic<float>, kOscBufSize> oscSynthBuf {};  // phantom harmonics
```

The visualizers in this plan read these on the message thread via `relaxed` atomic loads — exactly the same pattern the WebView2 bindings already use.

---

## File Structure

| File | Status | Responsibility |
|------|--------|----------------|
| `Source/UI/widgets/IOMeter.h` | **Create** | `IOMeter` widget: vertical bar reading a peak atomic, drawing dB-scaled fill + peak-hold marker + clip indicator. Pure paint, 30fps Timer. |
| `Source/UI/widgets/IOMeter.cpp` | **Create** | Implementation. |
| `Source/UI/visualizers/Oscilloscope.h` | **Create** | `Oscilloscope` Component reading the synth oscillator ring buffer (`oscSynthBuf` from PhantomEngine A). Draws waveform path. 30fps Timer-driven repaint. |
| `Source/UI/visualizers/Oscilloscope.cpp` | **Create** | Implementation. |
| `Source/UI/visualizers/Spectrum.h` | **Create** | `Spectrum` Component reading `spectrumData` (input) + `spectrumOutputData` (output) and drawing a 2-layer overlay of frequency bars. 30fps Timer-driven repaint. Engine-split mode is deferred to a later phase. |
| `Source/UI/visualizers/Spectrum.cpp` | **Create** | Implementation. |
| `Source/UI/panels/RightPanel.h` | **Modify** | Add `IOMeter inMeter`, `IOMeter outMeter`, `Oscilloscope oscilloscope`, `Spectrum spectrum` members. |
| `Source/UI/panels/RightPanel.cpp` | **Modify** | Update constructor to initialize new members and `addAndMakeVisible` them. Update `paint()` to keep section headers (Levels meter labels not needed — meters are visually self-evident). Update `resized()` to flank In/Out knobs with meters in Levels section, and add Oscilloscope (~120px tall) + Spectrum (~280px tall) below the Advanced row. |
| `Source/UI/NativePluginEditor.h` | **No change** | `RightPanel` already lives there. |
| `Source/UI/NativePluginEditor.cpp` | **No change** | Same. |
| `CMakeLists.txt` | **Modify** | Add `Source/UI/widgets/IOMeter.cpp`, `Source/UI/visualizers/Oscilloscope.cpp`, `Source/UI/visualizers/Spectrum.cpp` to `target_sources(KaigenPhantom)`. |

---

## Task 1: IOMeter widget class

**Files:**
- Create: `Source/UI/widgets/IOMeter.h`
- Create: `Source/UI/widgets/IOMeter.cpp`
- Modify: `CMakeLists.txt`

A vertical bar meter that reads a `std::atomic<float>` peak value (linear amplitude 0..1+) on the message thread, converts to dB, normalizes to a 0..1 display value via a `(dB + 60) / 60` mapping, and paints a fill from bottom up. Adds a peak-hold marker that decays slowly. Top edge flashes red when the input level exceeds 0 dBFS.

Audio thread is uninvolved — the meter just reads the existing `processor.peakInL/R/OutL/OutR` atomic floats that PhantomProcessor already maintains.

The class takes a reference to a `std::atomic<float>` — this avoids coupling to `PhantomProcessor` directly and lets the same widget render any peak source.

- [ ] **Step 1: Create Source/UI/widgets/IOMeter.h**

```bash
mkdir -p "Source/UI/widgets"
```

Then create the header:

```cpp
// Source/UI/widgets/IOMeter.h
#pragma once
#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaigen::phantom
{

/** Vertical bar meter reading a single std::atomic<float> peak source.
 *  Converts linear amplitude to dB, normalizes to 0..1 over -60..0 dB range,
 *  paints fill from bottom up. Includes peak-hold marker (decays at ~0.5 dB/s)
 *  and a clip indicator (top edge flashes red when raw amplitude >= 0.989,
 *  i.e. peak >= -0.1 dBFS).
 *  
 *  The peak source is referenced by pointer-to-atomic, so the same widget
 *  works for any std::atomic<float>. The meter polls the source at 30 fps
 *  via an internal Timer. */
class IOMeter : public juce::Component, private juce::Timer
{
public:
    explicit IOMeter(const std::atomic<float>& peakSource);
    ~IOMeter() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    const std::atomic<float>& peakSource;
    float currentLevel { 0.0f };  // smoothed (attack-fast, release-slow)
    float peakHold     { 0.0f };  // raw peak with decay
    static constexpr float kAttackCoef  = 0.5f;
    static constexpr float kReleaseCoef = 0.08f;
    static constexpr float kPeakHoldDecayPerTick = 0.003f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOMeter)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create Source/UI/widgets/IOMeter.cpp**

```cpp
// Source/UI/widgets/IOMeter.cpp
#include "IOMeter.h"
#include "../Theme.h"

namespace kaigen::phantom
{

namespace
{
    /** Convert linear amplitude to a normalized 0..1 display value over
     *  a -60..0 dB range. Returns 0 for any input <= 0. */
    float toDbNormalized(float lin)
    {
        if (lin <= 0.0f) return 0.0f;
        const float dB = 20.0f * std::log10(lin);
        return juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 60.0f);
    }
}

IOMeter::IOMeter(const std::atomic<float>& src)
    : peakSource(src)
{
    setSize(14, 90);
    startTimerHz(30);
}

IOMeter::~IOMeter()
{
    stopTimer();
}

void IOMeter::timerCallback()
{
    const float raw = peakSource.load(std::memory_order_relaxed);

    // Attack-fast / release-slow on the smoothed level.
    const float coef = (raw > currentLevel) ? kAttackCoef : kReleaseCoef;
    currentLevel += (raw - currentLevel) * coef;

    // Peak-hold: latch when smoothed exceeds it; decay otherwise.
    if (currentLevel > peakHold) peakHold = currentLevel;
    else                          peakHold = juce::jmax(0.0f, peakHold - kPeakHoldDecayPerTick);

    repaint();
}

void IOMeter::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background.
    g.setColour(Theme::matrixBg);
    g.fillRect(bounds);

    // Fill — clipped to dB-normalized current level.
    const float fillNorm = toDbNormalized(currentLevel);
    if (fillNorm > 0.0f)
    {
        const float fillH = fillNorm * h;
        auto grad = juce::ColourGradient(juce::Colour(0xffeaf3ff),
                                          0.0f, h - fillH,
                                          juce::Colour(0xffd4e5f5).withAlpha(0.95f),
                                          0.0f, h,
                                          false);
        g.setGradientFill(grad);
        g.fillRect(juce::Rectangle<float>(1.0f, h - fillH, w - 2.0f, fillH));
    }

    // Peak-hold marker — single thin line at the held peak's normalized y.
    const float holdNorm = toDbNormalized(peakHold);
    if (holdNorm > 0.0f)
    {
        const float py = h - holdNorm * h;
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRect(juce::Rectangle<float>(1.0f, py - 0.5f, w - 2.0f, 1.0f));
    }

    // Clip indicator — top 2px red when raw signal at or above -0.1 dBFS.
    if (peakSource.load(std::memory_order_relaxed) >= 0.989f)
    {
        g.setColour(Theme::clipRed);
        g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, w, 2.0f));
    }

    // Subtle border.
    g.setColour(Theme::panelBorder);
    g.drawRect(bounds, 1.0f);
}

} // namespace kaigen::phantom
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/widgets/IOMeter.cpp` after `Source/UI/widgets/LinkButton.cpp`:

```cmake
target_sources(KaigenPhantom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/NativePluginEditor.cpp
    Source/UI/widgets/PhantomKnob.cpp
    Source/UI/widgets/PhantomMiniKnob.cpp
    Source/UI/widgets/ToggleGroup.cpp
    Source/UI/widgets/LinkButton.cpp
    Source/UI/widgets/IOMeter.cpp
    Source/UI/panels/RightPanel.cpp
    Source/UI/panels/LeftPanel.cpp
    ...
)
```

- [ ] **Step 4: Build to verify compile**

Run:
```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. The `IOMeter` class compiles + links but isn't yet instantiated — Task 2 wires it into RightPanel.

- [ ] **Step 5: Commit**

```bash
git add Source/UI/widgets/IOMeter.h Source/UI/widgets/IOMeter.cpp CMakeLists.txt
git commit -m "feat(path-b): IOMeter widget (dB-scaled, peak-hold, clip indicator)"
```

(Exact commit message — no Claude trailer.)

---

## Task 2: Drop two IOMeters into RightPanel's LevelsSection

**Files:**
- Modify: `Source/UI/panels/RightPanel.h`
- Modify: `Source/UI/panels/RightPanel.cpp`

The current LevelsSection has [In knob, Out knob] in the top row plus an Auto button at the section header position. This task adds [In meter, In knob, Out knob, Out meter] — meters flanking the knobs as agreed during the WebView2 UI evolution.

The IOMeter constructor takes a `const std::atomic<float>&`. The processor exposes `peakInL` / `peakOutL` — but they're private. We need accessors.

Two options:
1. Add getter accessors to `PhantomProcessor` (`getPeakInL()` returning `const std::atomic<float>&`).
2. Add new public members or use existing getters if any exist.

Read `Source/PluginProcessor.h` to see what's currently public/private. The peak atomics may already be public — if so, just access directly. If private, add accessors.

If accessors are needed, add them to `Source/PluginProcessor.h` in the public section:

```cpp
const std::atomic<float>& getPeakInL()  const { return peakInL;  }
const std::atomic<float>& getPeakOutL() const { return peakOutL; }
```

(Use the L channel for now — stereo display can be added later if the visualizer feels asymmetric. The current WebView2 meter uses max(L, R), but L alone is acceptable for Phase 2.)

- [ ] **Step 1: Verify peak-atomic access (read-only)**

Open `Source/PluginProcessor.h`. Find the `peakInL`/`peakOutL` declarations (around line 65). Confirm whether they're public or private.

If private: add getter accessors in the public section:

```cpp
public:
    const std::atomic<float>& getPeakInL()  const { return peakInL;  }
    const std::atomic<float>& getPeakOutL() const { return peakOutL; }
```

If public: no accessor changes needed; proceed to step 2.

- [ ] **Step 2: Update RightPanel.h**

Add `#include "../widgets/IOMeter.h"` and add two IOMeter members. The header becomes:

```cpp
// Source/UI/panels/RightPanel.h
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../widgets/PhantomKnob.h"
#include "../widgets/PhantomMiniKnob.h"
#include "../widgets/IOMeter.h"

class PhantomProcessor;

namespace kaigen::phantom
{

class RightPanel : public juce::Component
{
public:
    RightPanel(juce::AudioProcessorValueTreeState& apvts, PhantomProcessor& processor);
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

    // Levels section auto-gain toggle
    juce::TextButton autoGainButton { "Auto" };
    std::unique_ptr<juce::ButtonParameterAttachment> autoGainAttachment;

    // Levels section meters (flanking In/Out knobs)
    IOMeter inMeter;
    IOMeter outMeter;

    // Advanced panel — 14 mini knobs, all per-engine 'a_' prefix.
    std::array<std::unique_ptr<PhantomMiniKnob>, 14> miniKnobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RightPanel)
};

} // namespace kaigen::phantom
```

(Note: constructor signature changed to take a `PhantomProcessor&` so we can pass peak atomic references to IOMeter constructors. The forward declaration of `PhantomProcessor` is at the top.)

- [ ] **Step 3: Update RightPanel.cpp**

Update the constructor signature and member initializers. The full constructor:

```cpp
RightPanel::RightPanel(juce::AudioProcessorValueTreeState& a, PhantomProcessor& p)
    : apvts(a),
      saturationKnob(apvts, "a_harmonic_saturation", PhantomKnob::Size::Medium, "Saturation"),
      shapeKnob     (apvts, "a_synth_step",          PhantomKnob::Size::Medium, "Shape"),
      skipKnob      (apvts, "a_synth_skip",          PhantomKnob::Size::Medium, "Skip"),
      widthKnob     (apvts, "a_stereo_width",        PhantomKnob::Size::Medium, "Width"),
      inGainKnob    (apvts, "input_gain",            PhantomKnob::Size::Medium, "In"),
      outGainKnob   (apvts, "a_output_gain",         PhantomKnob::Size::Medium, "Out"),
      inMeter       (p.getPeakInL()),
      outMeter      (p.getPeakOutL())
{
    addAndMakeVisible(saturationKnob);
    addAndMakeVisible(shapeKnob);
    addAndMakeVisible(skipKnob);
    addAndMakeVisible(widthKnob);
    addAndMakeVisible(inGainKnob);
    addAndMakeVisible(outGainKnob);

    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);

    autoGainButton.setClickingTogglesState(true);
    addAndMakeVisible(autoGainButton);
    if (auto* param = apvts.getParameter("input_gain_auto"))
        autoGainAttachment = std::make_unique<juce::ButtonParameterAttachment>(*param, autoGainButton);
    else
        jassertfalse;

    // Existing mini-knob array initialization stays unchanged.
    // ... (the existing miniDefs table + std::make_unique<PhantomMiniKnob>(...) loop)
}
```

(The mini knob array setup is unchanged — keep the existing block. Only the new init-list entries and the two `addAndMakeVisible(inMeter/outMeter)` calls are added.)

Also include the processor header at the top of RightPanel.cpp:

```cpp
#include "../../PluginProcessor.h"
```

(Path is `../../PluginProcessor.h` because `RightPanel.cpp` is at `Source/UI/panels/` and `PluginProcessor.h` is at `Source/`.)

Update `resized()` to flank the In/Out knobs with the meters. The Levels section currently is [In knob][Out knob] occupying ~168px. New layout: [InMeter, InKnob, OutKnob, OutMeter]. Meter is 14px wide. Add ~6px gap between meter and adjacent knob.

Replace the Levels-section portion of `resized()`:

```cpp
// Existing resized() ... up through Stereo section ...

knobRow.removeFromLeft(sectionGap);

// Levels section: meter, In knob, Out knob, meter
inMeter.setBounds(knobRow.removeFromLeft(14));
knobRow.removeFromLeft(6);
layoutKnob(inGainKnob);
layoutKnob(outGainKnob);
knobRow.removeFromLeft(6);
outMeter.setBounds(knobRow.removeFromLeft(14));

// Advanced mini-knob row stays the same below this.
```

Wait — that requires reordering: `layoutKnob` adds an 8px gap after each knob. After the second `layoutKnob(outGainKnob)`, we'd have an 8px gap built in, then we add 6px more. Let me clean this up by directly calling setBounds without the helper:

Actually the simpler approach: in the layout, keep `layoutKnob` for In/Out, and the gaps work out. The meter widths slot naturally into the available row area:

```cpp
// In resized(), replace the existing Levels section block:
knobRow.removeFromLeft(sectionGap);

// In meter
inMeter.setBounds(knobRow.removeFromLeft(14));
knobRow.removeFromLeft(6);

// In + Out knobs (using existing layoutKnob lambda)
layoutKnob(inGainKnob);
layoutKnob(outGainKnob);

// Out meter (the trailing 8px gap from layoutKnob is fine)
outMeter.setBounds(knobRow.removeFromLeft(14));
```

The full updated `resized()` body (carefully matching what's already there for top knob row + Advanced mini row):

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

    // Harmonic Engine
    layoutKnob(saturationKnob);
    layoutKnob(shapeKnob);
    layoutKnob(skipKnob);

    knobRow.removeFromLeft(sectionGap);

    // Stereo
    layoutKnob(widthKnob);

    knobRow.removeFromLeft(sectionGap);

    // Levels: meter, In knob, Out knob, meter
    inMeter.setBounds(knobRow.removeFromLeft(14));
    knobRow.removeFromLeft(6);
    layoutKnob(inGainKnob);
    layoutKnob(outGainKnob);
    outMeter.setBounds(knobRow.removeFromLeft(14));

    // Advanced mini-knob row
    area.removeFromTop(20);
    auto miniRow = area.removeFromTop(60).reduced(12, 0);
    const int miniWidth = 36;
    const int miniGap = 4;
    for (auto& mk : miniKnobs)
    {
        mk->setBounds(miniRow.removeFromLeft(miniWidth));
        miniRow.removeFromLeft(miniGap);
    }

    // Auto button (positioned at fixed coordinate next to "Levels" header).
    autoGainButton.setBounds(580, 6, 40, 18);
}
```

(The Auto button bounds are unchanged — it's set from the existing code.)

- [ ] **Step 4: Update NativePluginEditor to pass processor to RightPanel**

The constructor signature changed. In `Source/UI/NativePluginEditor.cpp`, find the RightPanel construction. Currently it's `rightPanel(a)`. Update to `rightPanel(a, p)` (where `p` is the constructor's processor parameter).

(Read NativePluginEditor.cpp to find the exact constructor — it should already be receiving the processor as the first ctor argument.)

The native editor's constructor body has access to `processor` (the `PhantomProcessor&` member). The init-list change should be straightforward.

- [ ] **Step 5: Build and verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. If you toggle to native UI in Live and play audio, the In meter on the left of the Levels section should fill green-white from the bottom, and the Out meter on the right should follow.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp Source/UI/NativePluginEditor.cpp Source/PluginProcessor.h
git commit -m "feat(path-b): IOMeters in LevelsSection (flanking In/Out knobs)"
```

(Stage `Source/PluginProcessor.h` only if you added the getPeakInL/OutL accessors. If the atomics were already public, just stage the panel/editor files.)

(Exact commit message — no Claude trailer.)

---

## Task 3: Oscilloscope component

**Files:**
- Create: `Source/UI/visualizers/Oscilloscope.h`
- Create: `Source/UI/visualizers/Oscilloscope.cpp`
- Modify: `CMakeLists.txt`

A `juce::Component` that draws a waveform from PhantomEngine A's `oscSynthBuf` (the synth oscillator's ring buffer). 30fps Timer-driven repaint reads the latest 1024 samples from the 2048-sample ring and draws a `juce::Path` connecting them across the component's width.

The processor's `getEngineA()` (or similar) accessor exposes the engine; PhantomEngine has `oscSynthBuf` as a public member. Read `Source/PluginProcessor.h` and `Source/Engines/PhantomEngine.h` to find the exact path. If no public accessor exists, add one.

The `oscSynthBuf` is a `std::array<std::atomic<float>, kOscBufSize>` — `kOscBufSize = 2048`. Audio thread writes; we read.

Audio-thread write semantics: the audio thread fills the ring continuously. We don't have an explicit write-position atomic exposed to read — the WebView2 binding takes the most recent `OSC_DISPLAY_SAMPLES` samples by reading the buffer in order from index 0 to 2047 each tick. This is acceptable because the audio thread writes faster than 30 Hz and any tearing is invisible at the visualization rate.

Actually — looking more carefully, there IS a write-position atomic somewhere if the WebView2 oscilloscope works correctly. Read `Source/PluginProcessor.cpp`'s `getOscilloscopeData` binding to see how it reads the ring (it should reference the write position). For Phase 2, mirror exactly what that binding does.

If the binding reads samples linearly (no position tracking), do the same in the native Oscilloscope.

- [ ] **Step 1: Read existing oscilloscope binding for reference**

Open `Source/PluginEditor.cpp` and find the `getOscilloscopeData` `withNativeFunction` block. Read it to understand how oscilloscope samples are pulled from the atomic ring. Note:
- Whether there's a write-position atomic
- Which buffer is the primary source (input, output, synth)
- The display window size
- Any smoothing or downsampling

Mirror that read pattern in the native Oscilloscope.

- [ ] **Step 2: Create Source/UI/visualizers/Oscilloscope.h**

```bash
mkdir -p "Source/UI/visualizers"
```

```cpp
// Source/UI/visualizers/Oscilloscope.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Native oscilloscope visualizer. Reads PhantomEngine A's oscSynthBuf
 *  (atomic float ring buffer, 2048 samples) and draws the most recent
 *  1024 samples as a continuous waveform path across the component's
 *  width. 30fps Timer-driven repaint. */
class Oscilloscope : public juce::Component, private juce::Timer
{
public:
    explicit Oscilloscope(PhantomProcessor& processor);
    ~Oscilloscope() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    PhantomProcessor& processor;
    static constexpr int kDisplaySamples = 1024;
    std::array<float, kDisplaySamples> snapshot {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Oscilloscope)
};

} // namespace kaigen::phantom
```

- [ ] **Step 3: Create Source/UI/visualizers/Oscilloscope.cpp**

The implementation reads from the engine's `oscSynthBuf`. Engine access path depends on the existing accessors — read `Source/PluginProcessor.h` for `getEngineA()` or similar, and `Source/Engines/PhantomEngine.h` for the public `oscSynthBuf` member.

If no engine accessor exists, you may need to add one. For now, assume `processor.getEngineA().oscSynthBuf` is the path. If different, use the actual path.

```cpp
// Source/UI/visualizers/Oscilloscope.cpp
#include "Oscilloscope.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"
#include "../../Engines/PhantomEngine.h"

namespace kaigen::phantom
{

Oscilloscope::Oscilloscope(PhantomProcessor& p)
    : processor(p)
{
    setSize(800, 120);
    startTimerHz(30);
}

Oscilloscope::~Oscilloscope()
{
    stopTimer();
}

void Oscilloscope::timerCallback()
{
    // Snapshot the most recent kDisplaySamples from the engine's synth ring.
    // The audio thread writes continuously; we read with relaxed semantics.
    // Tearing at the visualization rate is invisible — same pattern as the
    // existing WebView2 oscilloscope binding.
    auto& engineA = processor.getEngineA();
    const auto& ring = engineA.oscSynthBuf;
    const int total = (int) ring.size();
    const int start = total - kDisplaySamples;  // last 1024 samples
    for (int i = 0; i < kDisplaySamples; ++i)
        snapshot[(size_t) i] = ring[(size_t) (start + i)].load(std::memory_order_relaxed);
    repaint();
}

void Oscilloscope::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float midY = h * 0.5f;

    // Background.
    g.setColour(Theme::matrixBg);
    g.fillRect(bounds);

    // Center line.
    g.setColour(Theme::panelBorder);
    g.drawHorizontalLine((int) midY, 0.0f, w);

    // Waveform path.
    juce::Path path;
    if (kDisplaySamples > 0)
    {
        path.startNewSubPath(0.0f, midY - snapshot[0] * (h * 0.45f));
        for (int i = 1; i < kDisplaySamples; ++i)
        {
            const float x = (float) i / (float) (kDisplaySamples - 1) * w;
            const float y = midY - snapshot[(size_t) i] * (h * 0.45f);
            path.lineTo(x, y);
        }
    }
    g.setColour(Theme::steelBlue);
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

} // namespace kaigen::phantom
```

- [ ] **Step 4: Verify engine accessor exists**

If `processor.getEngineA()` doesn't exist, add it to `PhantomProcessor`. Look for `dualEngineHost` or similar member that holds the engines. Likely path: `processor.dualEngineHost.engineA()` or `processor.getDualEngineHost().getEngineA()`.

Use whatever the actual structure is. If you need to add a public accessor, add it to `PluginProcessor.h`:

```cpp
public:
    PhantomEngine& getEngineA() { return /* actual path to engine A */; }
```

If the engine reference path is more deeply nested (like `dualEngineHost.engineA`), expose that exact path via the accessor.

- [ ] **Step 5: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/visualizers/Oscilloscope.cpp`:

```cmake
target_sources(KaigenPhantom PRIVATE
    ...existing entries...
    Source/UI/widgets/IOMeter.cpp
    Source/UI/visualizers/Oscilloscope.cpp
    ...
)
```

- [ ] **Step 6: Build to verify compile**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean. Class is unreferenced — Task 5 wires it in.

- [ ] **Step 7: Commit**

```bash
git add Source/UI/visualizers/Oscilloscope.h Source/UI/visualizers/Oscilloscope.cpp CMakeLists.txt Source/PluginProcessor.h
git commit -m "feat(path-b): Oscilloscope visualizer (synth ring buffer waveform)"
```

(Stage `PluginProcessor.h` only if you added an accessor.)

---

## Task 4: Spectrum component

**Files:**
- Create: `Source/UI/visualizers/Spectrum.h`
- Create: `Source/UI/visualizers/Spectrum.cpp`
- Modify: `CMakeLists.txt`

A `juce::Component` that draws frequency-bin bars from `processor.spectrumData` (input, plain `float[80]`) and `processor.spectrumOutputData` (output, plain `float[80]`) overlaid as two layers. 30fps Timer-driven repaint reads both arrays each tick.

For Phase 2: input + output overlay only. Engine-split mode (showing engineA + engineB separately) is deferred — the spectrum is functional without it for testing purposes.

The processor's `spectrumData`/`spectrumOutputData` are private `std::array<float, kSpectrumBins>` members. Add public accessors that return `const std::array<float, kSpectrumBins>&` references.

- [ ] **Step 1: Add spectrum accessors to PluginProcessor.h (if needed)**

Read `Source/PluginProcessor.h`. Find `spectrumData` and `spectrumOutputData` declarations. If they're private, add public accessors:

```cpp
public:
    static constexpr int kSpectrumBins = 80;  // already exists; just confirming
    const std::array<float, kSpectrumBins>& getInputSpectrum()  const { return spectrumData; }
    const std::array<float, kSpectrumBins>& getOutputSpectrum() const { return spectrumOutputData; }
```

(If the constant `kSpectrumBins` is already public, don't redeclare. If it's static private, also keep it accessible as `PhantomProcessor::kSpectrumBins`.)

- [ ] **Step 2: Create Source/UI/visualizers/Spectrum.h**

```cpp
// Source/UI/visualizers/Spectrum.h
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Native spectrum analyzer. Reads input + output spectrum bins from the
 *  processor (precomputed on the audio thread, ~5.86 Hz update rate) and
 *  draws them as overlaid bar graphs. 30fps Timer-driven repaint. */
class Spectrum : public juce::Component, private juce::Timer
{
public:
    explicit Spectrum(PhantomProcessor& processor);
    ~Spectrum() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    PhantomProcessor& processor;
    static constexpr int kBins = 80;
    std::array<float, kBins> smoothedInput  {};
    std::array<float, kBins> smoothedOutput {};
    static constexpr float kSmoothUp   = 0.6f;
    static constexpr float kSmoothDown = 0.12f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Spectrum)
};

} // namespace kaigen::phantom
```

- [ ] **Step 3: Create Source/UI/visualizers/Spectrum.cpp**

```cpp
// Source/UI/visualizers/Spectrum.cpp
#include "Spectrum.h"
#include "../Theme.h"
#include "../../PluginProcessor.h"

namespace kaigen::phantom
{

Spectrum::Spectrum(PhantomProcessor& p)
    : processor(p)
{
    setSize(800, 280);
    startTimerHz(30);
}

Spectrum::~Spectrum()
{
    stopTimer();
}

void Spectrum::timerCallback()
{
    const auto& inSrc  = processor.getInputSpectrum();
    const auto& outSrc = processor.getOutputSpectrum();

    // Asymmetric smoothing: fast attack, slow release.
    for (int i = 0; i < kBins; ++i)
    {
        const float coefIn  = (inSrc [(size_t) i] > smoothedInput [(size_t) i]) ? kSmoothUp : kSmoothDown;
        smoothedInput [(size_t) i] += (inSrc [(size_t) i] - smoothedInput [(size_t) i]) * coefIn;
        const float coefOut = (outSrc[(size_t) i] > smoothedOutput[(size_t) i]) ? kSmoothUp : kSmoothDown;
        smoothedOutput[(size_t) i] += (outSrc[(size_t) i] - smoothedOutput[(size_t) i]) * coefOut;
    }
    repaint();
}

void Spectrum::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background.
    g.setColour(Theme::matrixBg);
    g.fillRect(bounds);

    // Bars: input as a filled silhouette in dim white, output overlaid in steel blue.
    const float binWidth = w / (float) kBins;

    auto drawBars = [&](const std::array<float, kBins>& values, juce::Colour fillColour)
    {
        g.setColour(fillColour);
        for (int i = 0; i < kBins; ++i)
        {
            const float v = juce::jlimit(0.0f, 1.0f, values[(size_t) i]);
            if (v <= 0.0f) continue;
            const float barH = v * h;
            const float x = (float) i * binWidth;
            g.fillRect(juce::Rectangle<float>(x + 0.5f, h - barH, binWidth - 1.0f, barH));
        }
    };

    drawBars(smoothedInput,  Theme::textDim);
    drawBars(smoothedOutput, Theme::steelBlue.withAlpha(0.85f));

    // Border.
    g.setColour(Theme::panelBorder);
    g.drawRect(bounds, 1.0f);
}

} // namespace kaigen::phantom
```

- [ ] **Step 4: Add to CMakeLists.txt**

In `target_sources(KaigenPhantom PRIVATE`, add `Source/UI/visualizers/Spectrum.cpp` after `Oscilloscope.cpp`:

```cmake
target_sources(KaigenPhantom PRIVATE
    ...existing entries...
    Source/UI/widgets/IOMeter.cpp
    Source/UI/visualizers/Oscilloscope.cpp
    Source/UI/visualizers/Spectrum.cpp
    ...
)
```

- [ ] **Step 5: Build to verify compile**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expected: builds clean.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/visualizers/Spectrum.h Source/UI/visualizers/Spectrum.cpp CMakeLists.txt Source/PluginProcessor.h
git commit -m "feat(path-b): Spectrum visualizer (input + output overlay bars)"
```

(Stage `PluginProcessor.h` only if you added accessors.)

---

## Task 5: Drop Oscilloscope + Spectrum into RightPanel

**Files:**
- Modify: `Source/UI/panels/RightPanel.h`
- Modify: `Source/UI/panels/RightPanel.cpp`

Add `Oscilloscope` and `Spectrum` as members of `RightPanel`, position them below the Advanced mini-knob row.

- [ ] **Step 1: Update RightPanel.h**

Add the includes:

```cpp
#include "../visualizers/Oscilloscope.h"
#include "../visualizers/Spectrum.h"
```

Add members in the private section after the mini-knob array:

```cpp
// Visualizers (below Advanced row)
Oscilloscope oscilloscope;
Spectrum spectrum;
```

- [ ] **Step 2: Update RightPanel.cpp constructor**

Add to the init list (after `outMeter(p.getPeakOutL())`):

```cpp
,
oscilloscope(p),
spectrum(p)
```

Add to the constructor body (after the existing `addAndMakeVisible` calls):

```cpp
addAndMakeVisible(oscilloscope);
addAndMakeVisible(spectrum);
```

- [ ] **Step 3: Update resized() to position visualizers**

Add at the end of `resized()`, after the Advanced mini-knob row layout:

```cpp
// Visualizers below Advanced row.
area.removeFromTop(12);  // gap before oscilloscope
auto vizArea = area.reduced(12, 0);
oscilloscope.setBounds(vizArea.removeFromTop(120));
vizArea.removeFromTop(8);
spectrum    .setBounds(vizArea.removeFromTop(280));
```

- [ ] **Step 4: Build and manually verify**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_Standalone
cmake --build build --config Debug --target KaigenPhantom_VST3
```

Expected: builds clean. VST3 reinstalls.

If you load Phantom in Live with native UI active and play audio:
- The oscilloscope shows a moving waveform of the synth output
- The spectrum shows two overlaid bar graphs (input dim, output steel-blue)
- Both update at 30fps

- [ ] **Step 5: Commit**

```bash
git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp
git commit -m "feat(path-b): Oscilloscope + Spectrum visualizers in RightPanel"
```

(Exact commit message — no Claude trailer.)

---

## Task 6: Manual smoke + final review

**Files:** none modified — verification only.

End-to-end smoke for Phase 2.

- [ ] **Step 1: Build VST3 + Standalone**

```
cmake -S . -B build
cmake --build build --config Debug --target KaigenPhantom_VST3
cmake --build build --config Debug --target KaigenPhantom_Standalone
```

Expect: both build clean. VST3 auto-installs.

- [ ] **Step 2: Toggle to native editor in Live**

Open WebView2 editor in Live. Shift+click PHANTOM logo. Reopen plugin window. Expect native editor with Phase 1 + 2 widgets.

- [ ] **Step 3: Verify IOMeters in LevelsSection**

- Play audio through the plugin
- In meter (left of In knob) fills white-ish from bottom proportional to input level
- Out meter (right of Out knob) fills similarly
- Stop audio: meters drop to silence within ~1 second (release time)
- Push level past 0 dBFS (use a clipping signal): top edge of the affected meter flashes red

- [ ] **Step 4: Verify Oscilloscope**

- The oscilloscope sits below the Advanced mini-knob row
- With audio playing through the plugin's synth path (e.g., MIDI notes triggering), the waveform animates
- Without audio: shows a flat center line

- [ ] **Step 5: Verify Spectrum**

- Below the oscilloscope, the spectrum shows frequency bars
- Input layer (dim) shows the input signal's spectrum
- Output layer (steel-blue) shows the post-processing spectrum
- Both update at ~30fps

- [ ] **Step 6: No commit if everything works**

Phase 2 done. Phase 3 (specialized widgets — RecipeWheel, refined panels) is next.

---

## Self-review notes

### Spec coverage

Phase 2 covers per the spec's "Phase 2 — Visualizers":
- ✓ `Spectrum` — Tasks 4, 5
- ✓ `Oscilloscope` — Tasks 3, 5
- ✓ `IOMeter` — Tasks 1, 2
- ✓ Atomic-snapshot reads (lock-free; existing infrastructure) — used in Tasks 1, 3, 4

Deviations from spec:
- **OpenGL deferred.** Spec recommended OpenGL for spectrum/scope. Phase 2 ships pure-Graphics painting at 30fps for simplicity. If profiling shows the 30fps repaints are too expensive, a follow-up phase swaps to `juce::OpenGLContext`. The atomic snapshot architecture is unchanged either way; only the rendering layer differs.
- **Engine-split spectrum mode deferred.** Spec mentioned engineA/engineB-split layout. Phase 2 ships input+output overlay only. Engine-split is a refinement before Phase 6.

### Placeholder scan

No "TBD"/"TODO"/vague language. Each step has runnable code or commands.

### Type / signature consistency

- `IOMeter(const std::atomic<float>&)` constructor consistent across Tasks 1-2.
- `Oscilloscope(PhantomProcessor&)` and `Spectrum(PhantomProcessor&)` constructors consistent across Tasks 3-5.
- `kSpectrumBins = 80` referenced by both Spectrum (Task 4) and the existing processor (already in `Source/PluginProcessor.h`).
- `RightPanel::RightPanel(juce::AudioProcessorValueTreeState&, PhantomProcessor&)` signature change consistent across Tasks 2 and 5.
- `NativePluginEditor` calls `RightPanel(a, p)` with the processor passed through.
- Atomic peak accessor names: `getPeakInL()` / `getPeakOutL()` — used in Task 2.
- Spectrum data accessors: `getInputSpectrum()` / `getOutputSpectrum()` — used in Task 4.
- Engine accessor: `getEngineA()` — used in Task 3 (verify exact name when reading PluginProcessor.h; if it's different, use the actual name and update Task 3's cpp accordingly).

### What ships in Phase 2

After this plan completes:
- Native editor's Levels section has flanking meters that respond to audio
- Below the Advanced row, the Oscilloscope and Spectrum visualizers animate at 30fps
- The plugin's WebView2 visualizers continue to work in WebView2 mode (no changes)
- Frame-rate is 30fps via JUCE Timer; if the user wants 60fps OpenGL, that's a follow-up

Phase 3 (specialized widgets: RecipeWheel, CrossoverColumn, FilterRow refinement) is next.
