# Synth Filter + Stable/Weird Presets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add user-controlled LPF/HPF on the synthesised harmonics signal inside PhantomEngine, and two new recipe wheel presets (Stable = even harmonics, Weird = odd harmonics).

**Architecture:** Two `juce::IIRFilter` pairs (LPF + HPF, one per stereo channel) are inserted into the per-sample PhantomEngine process loop after saturation but before envelope scaling. Two new `float` APVTS parameters drive them. Two new amplitude tables (`kStableAmps`, `kWeirdAmps`) extend the preset strip from 5 to 7 entries.

**Tech Stack:** JUCE 7 (IIRFilter, APVTS, WebSliderRelay), C++17, HTML/CSS/JS WebView UI

---

## File Map

| File | Change |
|---|---|
| `Source/Parameters.h` | Add `SYNTH_LPF_HZ`, `SYNTH_HPF_HZ` IDs; `kStableAmps`, `kWeirdAmps` tables; extend `RECIPE_PRESET`; add parameter definitions; update `getAllParameterIDs()` |
| `Source/Engines/PhantomEngine.h` | Add `lpfL/R`, `hpfL/R` IIRFilter members; `lastLPFHz`, `lastHPFHz`; declare `setSynthLPF()`, `setSynthHPF()` |
| `Source/Engines/PhantomEngine.cpp` | Implement setters; reset filters in `prepare()`/`reset()`; apply filters in process loop |
| `Source/PluginProcessor.cpp` | Add `syncParamsToEngine()` calls; extend `parameterChanged()` preset table |
| `Source/PluginEditor.h` | Add `synthLPFRelay`, `synthHPFRelay` WebSliderRelay members |
| `Source/PluginEditor.cpp` | Register relays in `buildWebViewOptions()`; add attachments in constructor; update `presetNames[]` + jlimit |
| `Source/WebUI/styles.css` | Add `panel-filter` sizing rule |
| `Source/WebUI/index.html` | Add Filter panel to Row 3; update preset strip; bump build tag to DSP-11 |

---

## Task 1: Parameters — IDs, preset tables, and layout

**Files:**
- Modify: `Source/Parameters.h`

- [ ] **Step 1: Add the two new parameter IDs to the `ParamID` namespace**

In `Source/Parameters.h`, add inside `namespace ParamID` after the `STEREO_WIDTH` line:

```cpp
    // ── Synth Filter ──────────────────────────────────────────────────
    /** Low-pass filter on synthesised harmonics. 200–20000 Hz. Default 20000 (transparent). */
    inline constexpr auto SYNTH_LPF_HZ      = "synth_lpf_hz";
    /** High-pass filter on synthesised harmonics. 20–2000 Hz. Default 20 (transparent). */
    inline constexpr auto SYNTH_HPF_HZ      = "synth_hpf_hz";
```

- [ ] **Step 2: Add the two new amplitude tables after the existing `kDenseAmps` line**

```cpp
inline constexpr float kStableAmps[7]     = { 1.00f, 0.00f, 0.70f, 0.00f, 0.50f, 0.00f, 0.30f };
inline constexpr float kWeirdAmps[7]      = { 0.00f, 1.00f, 0.00f, 0.80f, 0.00f, 0.60f, 0.00f };
```

- [ ] **Step 3: Extend `RECIPE_PRESET` StringArray from 5 to 7 choices**

Find this line in `createParameterLayout()`:
```cpp
    params.push_back(std::make_unique<APC>(
        ParamID::RECIPE_PRESET, "Recipe Preset",
        StringArray{ "Warm", "Aggressive", "Hollow", "Dense", "Custom" }, 0));
```

Replace with:
```cpp
    params.push_back(std::make_unique<APC>(
        ParamID::RECIPE_PRESET, "Recipe Preset",
        StringArray{ "Warm", "Aggressive", "Hollow", "Dense", "Stable", "Weird", "Custom" }, 0));
```

- [ ] **Step 4: Add the two new parameter definitions inside `createParameterLayout()` after `ENV_RELEASE_MS`**

```cpp
    // ── Synth Filter ──────────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_LPF_HZ, "Synth LPF",
        NormalisableRange<float>(200.0f, 20000.0f, 0.0f, 0.3f), 20000.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_HPF_HZ, "Synth HPF",
        NormalisableRange<float>(20.0f, 2000.0f, 0.0f, 0.3f), 20.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));
```

- [ ] **Step 5: Add the two new IDs to `getAllParameterIDs()` after `STEREO_WIDTH`**

```cpp
        ParamID::SYNTH_LPF_HZ,
        ParamID::SYNTH_HPF_HZ,
```

- [ ] **Step 6: Commit**

```bash
git add Source/Parameters.h
git commit -m "feat: add SYNTH_LPF_HZ, SYNTH_HPF_HZ params + Stable/Weird amplitude tables"
```

---

## Task 2: PhantomEngine — filter members and setter declarations

**Files:**
- Modify: `Source/Engines/PhantomEngine.h`

- [ ] **Step 1: Add filter members and setters to `PhantomEngine.h`**

In the `private:` section of `PhantomEngine`, after the `stereoWidth` / `sampleRate` / `crossoverHz` block, add:

```cpp
    // IIR filters on the synthesised harmonics signal
    juce::IIRFilter lpfL, lpfR;
    juce::IIRFilter hpfL, hpfR;
    float lastLPFHz = 20000.0f;
    float lastHPFHz = 20.0f;
```

In the `public:` parameter setters section, add after `setStereoWidth`:

```cpp
    void setSynthLPF(float hz);            // 200–20000, default 20000 (transparent)
    void setSynthHPF(float hz);            // 20–2000,   default 20   (transparent)
```

- [ ] **Step 2: Commit**

```bash
git add Source/Engines/PhantomEngine.h
git commit -m "feat: declare LPF/HPF filter members and setters in PhantomEngine"
```

---

## Task 3: PhantomEngine — filter implementation

**Files:**
- Modify: `Source/Engines/PhantomEngine.cpp`

- [ ] **Step 1: Reset filters in `prepare()`**

Find the end of `PhantomEngine::prepare()`, just before the closing brace (after `highBuf.setSize(...)`). Add:

```cpp
    const auto lpfCoeff = juce::IIRCoefficients::makeLowPass(sr, 20000.0);
    const auto hpfCoeff = juce::IIRCoefficients::makeHighPass(sr, 20.0);
    lpfL.setCoefficients(lpfCoeff); lpfR.setCoefficients(lpfCoeff);
    hpfL.setCoefficients(hpfCoeff); hpfR.setCoefficients(hpfCoeff);
    lpfL.reset(); lpfR.reset();
    hpfL.reset(); hpfR.reset();
    lastLPFHz = 20000.0f;
    lastHPFHz = 20.0f;
```

- [ ] **Step 2: Reset filters in `reset()`**

Find `PhantomEngine::reset()`. After `lowBuf.clear(); highBuf.clear();`, add:

```cpp
    lpfL.reset(); lpfR.reset();
    hpfL.reset(); hpfR.reset();
```

- [ ] **Step 3: Implement `setSynthLPF()`**

Add this function to `PhantomEngine.cpp` after the existing `setStereoWidth` setter:

```cpp
void PhantomEngine::setSynthLPF(float hz)
{
    hz = juce::jlimit(200.0f, 20000.0f, hz);
    if (hz == lastLPFHz) return;
    lastLPFHz = hz;
    const auto coeff = juce::IIRCoefficients::makeLowPass(sampleRate, (double) hz);
    lpfL.setCoefficients(coeff);
    lpfR.setCoefficients(coeff);
}
```

- [ ] **Step 4: Implement `setSynthHPF()`**

```cpp
void PhantomEngine::setSynthHPF(float hz)
{
    hz = juce::jlimit(20.0f, 2000.0f, hz);
    if (hz == lastHPFHz) return;
    lastHPFHz = hz;
    const auto coeff = juce::IIRCoefficients::makeHighPass(sampleRate, (double) hz);
    hpfL.setCoefficients(coeff);
    hpfR.setCoefficients(coeff);
}
```

- [ ] **Step 5: Add filter references at top of outer channel loop in `process()`**

Find the outer channel loop in `process()`. It begins:
```cpp
    int oscWp = oscSynthWrPos.load(std::memory_order_relaxed);
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto& env  = (ch == 0) ? envelopeL  : envelopeR;
        auto& syn  = (ch == 0) ? synthL     : synthR;
        auto& sat  = (ch == 0) ? smoothSatL : smoothSatR;
```

Add two more `auto&` lines after the existing three:
```cpp
        auto& lpf  = (ch == 0) ? lpfL       : lpfR;
        auto& hpf  = (ch == 0) ? hpfL       : hpfR;
```

- [ ] **Step 6: Apply filters in inner loop after saturation**

Find this block in the inner `for (int i = 0; i < n; ++i)` loop:
```cpp
            const float saturation = sat.getNextValue();
            if (saturation > 0.0f)
            {
                const float satCurve = std::tanh(phantomSample * 3.0f);
                phantomSample = phantomSample * (1.0f - saturation * 0.5f)
                              + satCurve * saturation;
            }

            // Oscilloscope capture (left channel only)
```

Insert between the closing saturation brace and the oscilloscope capture comment:

```cpp
            // Synth filter (LPF + HPF applied to harmonics only, before envelope scale)
            phantomSample = lpf.processSingleSampleRaw(phantomSample);
            phantomSample = hpf.processSingleSampleRaw(phantomSample);

```

- [ ] **Step 7: Commit**

```bash
git add Source/Engines/PhantomEngine.cpp
git commit -m "feat: implement per-sample LPF/HPF on synthesised harmonics in PhantomEngine"
```

---

## Task 4: PluginProcessor — wire parameters and extend presets

**Files:**
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Add filter parameter sync in `syncParamsToEngine()`**

Find `syncParamsToEngine()`. After the `engine.setStereoWidth(...)` call, add:

```cpp
    engine.setSynthLPF(apvts.getRawParameterValue(ParamID::SYNTH_LPF_HZ)->load());
    engine.setSynthHPF(apvts.getRawParameterValue(ParamID::SYNTH_HPF_HZ)->load());
```

- [ ] **Step 2: Extend preset tables in `parameterChanged()`**

Find `parameterChanged()`. The current tables array is:
```cpp
        const float* tables[] = { kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps, nullptr };

        if (idx >= 0 && idx < 4 && tables[idx] != nullptr)
```

Replace both lines with:
```cpp
        const float* tables[] = {
            kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps,
            kStableAmps, kWeirdAmps,
            nullptr   // Custom (index 6)
        };

        if (idx >= 0 && idx < 6 && tables[idx] != nullptr)
```

- [ ] **Step 3: Commit**

```bash
git add Source/PluginProcessor.cpp
git commit -m "feat: wire SYNTH_LPF/HPF to engine; extend preset table for Stable/Weird"
```

---

## Task 5: PluginEditor — relay registration and preset name update

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Add two relay members to `PluginEditor.h`**

Find the slider relays block. After `stereoWidthRelay`, add:

```cpp
    juce::WebSliderRelay synthLPFRelay         { "synth_lpf_hz" };
    juce::WebSliderRelay synthHPFRelay         { "synth_hpf_hz" };
```

- [ ] **Step 2: Register relays in `buildWebViewOptions()` sliderRelays array**

In `PluginEditor.cpp`, find the `sliderRelays[]` array. After `&self.stereoWidthRelay`, add:

```cpp
        &self.synthLPFRelay, &self.synthHPFRelay
```

- [ ] **Step 3: Add parameter attachments in the constructor `sliderBindings[]` array**

Find the `sliderBindings[]` array in the constructor. After the `STEREO_WIDTH` entry, add:

```cpp
        { ParamID::SYNTH_LPF_HZ, synthLPFRelay },
        { ParamID::SYNTH_HPF_HZ, synthHPFRelay },
```

- [ ] **Step 4: Update `presetNames[]` and `jlimit` in `getPitchInfo` native function**

Find in `buildWebViewOptions()`:
```cpp
                static const char* presetNames[] = { "Warm","Aggressive","Hollow","Dense","Custom" };
                obj->setProperty("preset", juce::String(presetNames[juce::jlimit(0, 4, presetIdx)]));
```

Replace with:
```cpp
                static const char* presetNames[] = { "Warm","Aggressive","Hollow","Dense","Stable","Weird","Custom" };
                obj->setProperty("preset", juce::String(presetNames[juce::jlimit(0, 6, presetIdx)]));
```

- [ ] **Step 5: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat: register LPF/HPF relays; update preset names array to 7 entries"
```

---

## Task 6: WebUI — Filter panel and preset strip

**Files:**
- Modify: `Source/WebUI/styles.css`
- Modify: `Source/WebUI/index.html`

- [ ] **Step 1: Add `panel-filter` sizing to `styles.css`**

Find the panel flex sizing comment block:
```css
/* Panel flex sizing — Row 3 */
.panel-pitch{flex:0.45}
.panel-decon{flex:0.45}
.panel-sidechain{flex:0.55}
```

Add after it:
```css
.panel-envelope{flex:0.6}
.panel-filter{flex:0.4;align-items:center}
```

- [ ] **Step 2: Add the Filter panel to Row 3 in `index.html`**

Find the Envelope Follower row:
```html
        <!-- Row 3: Envelope follower -->
        <div class="ctrl-row">
          <div class="candy-inner panel-envelope">
            <div class="el">Envelope Follower</div>
            <div class="knob-row">
              <phantom-knob data-param="env_attack_ms" size="medium" label="Attack" default-value="0.05"></phantom-knob>
              <phantom-knob data-param="env_release_ms" size="medium" label="Release" default-value="0.1"></phantom-knob>
            </div>
          </div>
        </div>
```

Replace with:
```html
        <!-- Row 3: Envelope follower + Filter -->
        <div class="ctrl-row">
          <div class="candy-inner panel-envelope">
            <div class="el">Envelope Follower</div>
            <div class="knob-row">
              <phantom-knob data-param="env_attack_ms" size="medium" label="Attack" default-value="0.05"></phantom-knob>
              <phantom-knob data-param="env_release_ms" size="medium" label="Release" default-value="0.1"></phantom-knob>
            </div>
          </div>
          <div class="candy-inner panel-filter">
            <div class="el">Filter</div>
            <div class="knob-row">
              <phantom-knob data-param="synth_lpf_hz" size="medium" label="LPF" default-value="1"></phantom-knob>
              <phantom-knob data-param="synth_hpf_hz" size="medium" label="HPF" default-value="0"></phantom-knob>
            </div>
          </div>
        </div>
```

- [ ] **Step 3: Update the preset strip — add Stable/Weird, move Custom to index 6**

Find:
```html
          <div class="lps">
            <span class="lw active" data-preset="0">Warm</span>
            <span class="lw" data-preset="1">Aggr</span>
            <span class="lw" data-preset="2">Hollow</span>
            <span class="lw" data-preset="3">Dense</span>
            <span class="lw" data-preset="4">Custom</span>
          </div>
```

Replace with:
```html
          <div class="lps">
            <span class="lw active" data-preset="0">Warm</span>
            <span class="lw" data-preset="1">Aggr</span>
            <span class="lw" data-preset="2">Hollow</span>
            <span class="lw" data-preset="3">Dense</span>
            <span class="lw" data-preset="4">Stable</span>
            <span class="lw" data-preset="5">Weird</span>
            <span class="lw" data-preset="6">Custom</span>
          </div>
```

- [ ] **Step 4: Bump build tag from DSP-10 to DSP-11**

Find:
```html
  <div style="position:fixed;top:2px;right:2px;z-index:99999;background:#0a0;color:#fff;font:bold 10px monospace;padding:2px 6px;border-radius:3px">DSP-10</div>
```

Replace with:
```html
  <div style="position:fixed;top:2px;right:2px;z-index:99999;background:#0a0;color:#fff;font:bold 10px monospace;padding:2px 6px;border-radius:3px">DSP-11</div>
```

- [ ] **Step 5: Reduce preset strip gap if labels overflow**

After building, if the preset strip wraps or overflows, find in `styles.css`:
```css
.lps{
  display:flex;gap:14px;padding:3px 0 0;z-index:2;
```

Change `gap:14px` to `gap:8px`.

- [ ] **Step 6: Commit**

```bash
git add Source/WebUI/styles.css Source/WebUI/index.html
git commit -m "feat: add Filter panel (LPF/HPF knobs) to Row 3; add Stable/Weird to preset strip (DSP-11)"
```

---

## Task 7: Build verification

- [ ] **Step 1: Build the plugin**

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\Documents\NEw project\Kaigen Phantom\build" --config Debug 2>&1 | tail -20
```

Expected: `Build succeeded` with 0 errors.

- [ ] **Step 2: Verify in DAW**

Load the plugin in Ableton Live 12. Check:

1. Build tag reads **DSP-11** (top-right corner of UI)
2. **Filter panel** appears in Row 3 to the right of Envelope Follower, with LPF and HPF knobs
3. **LPF knob** at max (20 kHz) = no audible filtering. Reduce to 500 Hz on a bass note — high harmonics should roll off smoothly
4. **HPF knob** at min (20 Hz) = no audible filtering. Increase to 300 Hz — low harmonics should thin out
5. **Preset strip** shows: Warm · Aggr · Hollow · Dense · Stable · Weird · Custom
6. Click **Stable** — wheel should show H2/H4/H6/H8 lit, H3/H5/H7 dark. Sound should be cleaner/more octave-like
7. Click **Weird** — wheel should show H3/H5/H7 lit, others dark. Sound should be more dissonant/complex
8. Click **Custom** (index 6) — wheel should stay at current position (no table applied)
9. **Oscilloscope** (toggle button) still works and shows the filtered harmonics in the amber SYNTH layer

- [ ] **Step 3: If build fails — common fixes**

- `IIRFilter` not found: ensure `#include <JuceHeader.h>` is present in `PhantomEngine.h` (it already is)
- `IIRCoefficients::makeLowPass` not found: this is in `juce_audio_basics` — verify `CMakeLists.txt` links `juce::juce_audio_basics` (it should already via `juce_audio_processors`)
- Preset index out of bounds: verify the `jlimit(0, 6, ...)` edit in `PluginEditor.cpp` was applied

- [ ] **Step 4: Final commit if any fixes were needed**

```bash
git add -p
git commit -m "fix: resolve build issues for DSP-11 filter + presets"
```
