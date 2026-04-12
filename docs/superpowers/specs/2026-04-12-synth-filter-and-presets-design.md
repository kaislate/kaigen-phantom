# Synth Filter + Stable/Weird Presets — Design Spec

**Date:** 2026-04-12
**Status:** Approved
**Scope:** LPF/HPF on synthesized harmonics signal + two new recipe wheel presets

---

## 1. Overview

Two features are added together since they touch overlapping files:

1. **Synth Filter** — user-controlled LPF and HPF applied to the phantom harmonics signal inside `PhantomEngine`, before the ghost mix. Matches the LPF/HPF controls in MIYA (dear-reality).
2. **Stable/Weird presets** — two new recipe wheel presets targeting even-only and odd-only harmonics, inserted before Custom in the existing preset strip.

---

## 2. Synth Filter

### 2.1 Signal Chain Placement

Filters are applied to `phantomSample` **after** post-synthesis saturation and **before** envelope scaling and ghost mix:

```
normIn
  → ZCS.process()
  → tanh saturation (existing)
  → LPF (new)
  → HPF (new)
  → × inLvl × phantomStrength  [envelope scale]
  → ghost mix
```

The dry bass signal and high-pass pass-through are never filtered — only the generated harmonics are shaped.

### 2.2 Filter Type

JUCE `juce::IIRFilter` (legacy per-sample API), Butterworth response, 12 dB/oct (2-pole). Coefficients recalculated once per block via `setCoefficients()`. Per-sample processing via `processSingleSampleRaw()`.

Coefficients are only recomputed when the cutoff value actually changes (last-set value stored in `PhantomEngine`, compared each block). This avoids unnecessary trig computation at block rate.

If block-rate coefficient switching causes audible artifacts at extreme knob speeds, fall back to a direct SVF implementation (2-integrator, 4 state variables per channel).

### 2.3 Parameters

| Parameter ID | Range | Default | Label | Notes |
|---|---|---|---|---|
| `synth_lpf_hz` | 200–20000 Hz | 20000 | Hz | At max = transparent (no filtering) |
| `synth_hpf_hz` | 20–2000 Hz | 20 | Hz | At min = transparent (no filtering) |

Both use `NormalisableRange<float>` with skew factor for log-ish feel (skewFactorFromMidPoint at ~2 kHz for LPF, ~200 Hz for HPF).

### 2.4 PhantomEngine Changes

**New members:**
```cpp
juce::IIRFilter lpfL, lpfR, hpfL, hpfR;
float lastLPFHz = 20000.0f;
float lastHPFHz = 20.0f;
```

**New setters:**
```cpp
void setSynthLPF(float hz);   // recalculates if hz != lastLPFHz
void setSynthHPF(float hz);   // recalculates if hz != lastHPFHz
```

**reset():** Calls `lpfL.reset()`, `lpfR.reset()`, `hpfL.reset()`, `hpfR.reset()`.

**process() inner loop** (after saturation, inside `for (int ch = 0; ch < nCh; ++ch)`):
```cpp
// lpf/hpf selected per-channel at top of outer loop:
//   auto& lpf = (ch == 0) ? lpfL : lpfR;
//   auto& hpf = (ch == 0) ? hpfL : hpfR;
phantomSample = lpf.processSingleSampleRaw(phantomSample);
phantomSample = hpf.processSingleSampleRaw(phantomSample);
```

### 2.5 PluginProcessor Changes

`syncParamsToEngine()` additions:
```cpp
engine.setSynthLPF(apvts.getRawParameterValue(ParamID::SYNTH_LPF_HZ)->load());
engine.setSynthHPF(apvts.getRawParameterValue(ParamID::SYNTH_HPF_HZ)->load());
```

### 2.6 PluginEditor Changes

Two new `WebSliderRelay` members, added to `buildWebViewOptions()` sliderRelays array, and `WebSliderParameterAttachment` bindings — identical pattern to all existing slider parameters.

### 2.7 UI — Filter Panel

New panel added to Row 3, to the right of the Envelope Follower panel:

```
┌─────────────────────────────┬──────────────────────────┐
│  Envelope Follower           │  Filter                  │
│  [Attack]  [Release]         │  [LPF Hz]  [HPF Hz]      │
└─────────────────────────────┴──────────────────────────┘
```

- Panel CSS class: `panel-filter`
- Both knobs: `size="medium"`, labels "LPF" and "HPF"
- LPF left, HPF right (frequency ordering)
- HTML parameter attributes: `data-param="synth_lpf_hz"` and `data-param="synth_hpf_hz"`

---

## 3. Stable/Weird Presets

### 3.1 Preset Index Assignment

Custom slides from index 4 to index 6. Final order:

| Index | Name |
|---|---|
| 0 | Warm |
| 1 | Aggressive |
| 2 | Hollow |
| 3 | Dense |
| 4 | Stable |
| 5 | Weird |
| 6 | Custom |

### 3.2 Amplitude Tables

**Stable** (even harmonics H2/H4/H6/H8 — octave-related, consonant):

| H2 | H3 | H4 | H5 | H6 | H7 | H8 |
|---|---|---|---|---|---|---|
| 1.0 | 0.0 | 0.7 | 0.0 | 0.5 | 0.0 | 0.3 |

**Weird** (odd harmonics H3/H5/H7 — fifth/seventh-based, complex):

| H2 | H3 | H4 | H5 | H6 | H7 | H8 |
|---|---|---|---|---|---|---|
| 0.0 | 1.0 | 0.0 | 0.8 | 0.0 | 0.6 | 0.0 |

### 3.3 C++ Changes

**Parameters.h** — `RECIPE_PRESET` choice count: 5 → 7.

**PluginProcessor.h** — add `kStableAmps[7]` and `kWeirdAmps[7]` constant arrays alongside existing `kWarmAmps` etc.

**PluginProcessor.cpp — `parameterChanged()`:**
```cpp
const float* tables[] = {
    kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps,
    kStableAmps, kWeirdAmps,
    nullptr   // Custom (index 6)
};
```
Guard: `if (idx >= 0 && idx < 6 && tables[idx] != nullptr)` — unchanged logic, just wider range.

**PluginEditor.cpp — `getPitchInfo` native function:**
```cpp
static const char* presetNames[] = {
    "Warm", "Aggressive", "Hollow", "Dense", "Stable", "Weird", "Custom"
};
// jlimit upper bound: 0 → 6
```

### 3.4 UI Changes

**index.html** — preset strip: add `Stable` (data-preset="4") and `Weird` (data-preset="5"), update `Custom` to `data-preset="6"`.

**phantom.js** — no changes needed (preset wiring is generic via `data-preset` attribute and `presetState.setChoiceIndex()`).

---

## 4. Build Tag

DSP-11 (LPF/HPF + Stable/Weird presets).

---

## 5. Out of Scope

- Variable filter slope
- Per-band filtering
- Gate threshold user control (future)
- Min-length user control (future)
