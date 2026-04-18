# Wavelet Synth Enhancements: Min Freq, Sub-Sample Interpolation, Threshold & Boost

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add three enhancements to the wavelet resynthesis engine that bring it closer to Miya-quality DSP: a user-controllable minimum tracking frequency, sub-sample zero-crossing interpolation for better phase coherence, and per-wavelet upward expansion (Threshold & Boost).

**Architecture:** All three features modify the existing WaveletSynth and ZeroCrossingSynth engines. No new files are created. The parameter layer, relay wiring, and UI each get small additions following established patterns.

**Tech Stack:** JUCE 8, C++17, WebView2 UI with WebSliderRelay bindings.

---

## 1. Min Frequency ("Lo") Knob

### Problem

The minimum trackable frequency is hardcoded at 16Hz (`maxPeriodSamples = sampleRate / 16.0`). This wastes the lower end of the tracking range — real sub-bass content can sit at 8–15Hz — and offers no user control over the floor.

### Design

New parameter `SYNTH_MIN_FREQ_HZ`:

| Field | Value |
|---|---|
| ID | `synth_min_freq_hz` |
| Label | "Lo" |
| Range | 8–200 Hz |
| Default | 8 Hz |
| Skew | 0.25 (logarithmic, matching Max Freq) |
| Unit | Hz |

The existing hardcoded `maxPeriodSamples = sampleRate / 16.0` in both `WaveletSynth::prepare()` and `ZeroCrossingSynth::prepare()` is replaced with a setter:

```cpp
void setMinFreqHz(float hz) noexcept
{
    minFreqHz        = juce::jlimit(8.0f, 200.0f, hz);
    maxPeriodSamples = (float)(sampleRate / (double)minFreqHz);
}
```

`prepare()` calls `setMinFreqHz(minFreqHz)` to recalculate when sample rate changes. The member default is `8.0f`.

The existing "Max Freq" knob is renamed to "Hi" in the UI to pair visually with "Lo". The parameter ID (`synth_max_track_hz`) and label in Parameters.h ("Max Track Hz") remain unchanged — only the HTML knob label changes.

### UI Placement

Harmonic Engine panel (Row 1). The "Lo" knob is placed immediately before the existing "Hi" (Max Freq) knob:

```
... | Gate | Lo | Hi | Track |
```

Both are `size="medium"`.

### Wiring

- `Parameters.h`: New parameter definition
- `PhantomEngine`: New `setMinFreqHz(float)` forwarding to all 4 synths
- `PluginProcessor::syncParamsToEngine()`: Read and forward
- `PluginEditor.h`: New relay + attachment
- `PluginEditor.cpp`: Wire relay in `buildWebViewOptions` + slider binding
- `index.html`: New `<phantom-knob>` element

---

## 2. Sub-Sample Zero-Crossing Interpolation

### Problem

Zero crossings are detected at sample boundaries (`lastSample <= 0 && x > 0`). For a 100Hz signal at 44.1kHz, one sample = 0.82 degrees of phase. Snapping to integer boundaries introduces up to 0.82 degrees of jitter per crossing, which accumulates across skip intervals and degrades period estimation accuracy and phase coherence in the resynthesized wavelet.

### Design

At every detected positive zero crossing, compute the fractional offset using linear interpolation:

```cpp
const float fraction = -lastSample / (x - lastSample);  // 0..1
```

`fraction` is the sub-sample offset from the previous sample to the true zero crossing. A fraction of 0.0 means the crossing was exactly at `lastSample`; 1.0 means exactly at `x`.

Apply the correction to timing accumulators:

```cpp
// Before: samplesSinceLastCrossing counted integer samples
// After:  subtract the fractional overshoot from both accumulators
const float correction = 1.0f - fraction;
samplesSinceLastCrossing -= correction;
accumulatedSamples       -= correction;
```

Apply the correction to phase reset (WaveletSynth only — ZeroCrossingSynth uses continuous phase):

```cpp
// Instead of: currentPhase = 0.0f
// Advance past the fractional offset so the wavelet starts at the correct sub-sample phase
currentPhase = correction * (kTwoPi / safePeriod);
```

Where `safePeriod` is the clamped `estimatedPeriod` used for the phase advance.

### Safety

Guard against division by zero when `x == lastSample` (both exactly 0):

```cpp
const float denom = x - lastSample;
const float fraction = (std::abs(denom) > 1e-12f) ? (-lastSample / denom) : 0.5f;
```

### Scope

Applied identically in both `WaveletSynth::process()` and `ZeroCrossingSynth::process()`. The correction to `samplesSinceLastCrossing` and `accumulatedSamples` improves period estimation in both engines. The phase reset correction only applies to WaveletSynth (ZeroCrossingSynth uses `fundamentalPhase` which is continuous, not reset).

### Files

- `WaveletSynth.cpp`: Interpolation + accumulator correction + phase reset correction
- `ZeroCrossingSynth.cpp`: Interpolation + accumulator correction (no phase reset)

No new parameters, no UI changes.

---

## 3. Threshold & Boost (Upward Expansion)

### Problem

The existing Punch feature blends smooth envelope with per-wavelet peak amplitude, but there's no way to emphasize loud wavelets relative to quiet ones. Miya's "Threshold & Boost" provides upward expansion: wavelets whose peak exceeds a threshold get boosted, emphasizing transients and strong harmonics while leaving quiet content unchanged.

### Design

Two new parameters:

| Field | `SYNTH_BOOST_THRESHOLD` | `SYNTH_BOOST_AMOUNT` |
|---|---|---|
| ID | `synth_boost_threshold` | `synth_boost_amount` |
| Label | "Threshold" | "Boost" |
| Range | 0–100% | 0–200% |
| Default | 0% (off) | 0% (no boost) |
| Unit | % | % |

The threshold is normalized to `inputPeak` (same convention as the gate). When `boostThreshold > 0` and the completed wavelet's peak amplitude exceeds `boostThreshold * inputPeak`, the output is scaled by `1.0 + boostAmount`.

Per-wavelet gain computation at each crossing completion (inside the `crossingsAccum >= skipCount` block, after gate gain):

```cpp
if (rawBoostThr <= 0.0f || lastWaveletPeak < rawBoostThr * inputPeak)
    lastBoostGain = 1.0f;
else
    lastBoostGain = 1.0f + boostAmount;  // boostAmount is 0..2
```

Applied in the output path after the gate gain, before the length gate:

```cpp
y *= lastGateGain;
y *= lastBoostGain;
// ... length gate follows
```

### Decay

`lastBoostGain` does NOT need independent decay — it's recalculated at every crossing. Between crossings it holds the last computed value. During silence, `lastWaveletPeak` already decays (0.9998/sample), so `lastWaveletPeak` drops below the threshold naturally and `lastBoostGain` returns to 1.0 at the next crossing.

### Smoothing

Both parameters use `juce::SmoothedValue<float>` with the standard 10ms ramp, following the same pattern as `smoothGate`. The smoothers are initialized at the member declaration (not in `prepare()`) to survive DAW transport restarts.

### Scope

RESYN mode (WaveletSynth) only. The gate is RESYN-only, and Threshold & Boost is the same class of per-wavelet amplitude control. ZeroCrossingSynth (Effect mode) does not get Threshold & Boost.

### UI Placement

Punch panel (Row 2), which expands from 1 to 3 knobs:

```
Punch [On]
| Amount | Threshold | Boost |
```

All three are `size="medium"`.

### Wiring

- `Parameters.h`: 2 new parameter definitions
- `WaveletSynth.h`: 2 new smoothers, `lastBoostGain` member, 2 setters
- `WaveletSynth.cpp`: Boost gain computation + output scaling
- `PhantomEngine.h/.cpp`: Forward 2 new params to resynL/resynR
- `PluginProcessor.cpp`: Read and forward in `syncParamsToEngine()`
- `PluginEditor.h`: 2 new relays + attachments
- `PluginEditor.cpp`: Wire relays in `buildWebViewOptions` + slider bindings
- `index.html`: 2 new `<phantom-knob>` elements in Punch panel

---

## New Parameters Summary

| # | ID | Label | Range | Default | UI Location |
|---|---|---|---|---|---|
| 1 | `synth_min_freq_hz` | Lo | 8–200 Hz (skew 0.25) | 8 Hz | Harmonic Engine, before Hi |
| 2 | `synth_boost_threshold` | Threshold | 0–100% | 0% | Punch panel |
| 3 | `synth_boost_amount` | Boost | 0–200% | 0% | Punch panel |

## Files Touched Summary

| File | Changes |
|---|---|
| `Source/Parameters.h` | 3 new params, count update |
| `Source/Engines/WaveletSynth.h` | `minFreqHz` member + setter, `lastBoostGain` + 2 smoothers, `setBoostThreshold`/`setBoostAmount` |
| `Source/Engines/WaveletSynth.cpp` | Sub-sample interpolation, minFreq setter, boost gain logic + output scaling |
| `Source/Engines/ZeroCrossingSynth.h` | `minFreqHz` member + setter |
| `Source/Engines/ZeroCrossingSynth.cpp` | Sub-sample interpolation, minFreq setter |
| `Source/Engines/PhantomEngine.h` | 3 new forwarding setters |
| `Source/Engines/PhantomEngine.cpp` | 3 setter implementations |
| `Source/PluginProcessor.cpp` | 3 new lines in `syncParamsToEngine()` |
| `Source/PluginEditor.h` | 3 new relays + attachments |
| `Source/PluginEditor.cpp` | 3 new relays in `buildWebViewOptions` + 3 slider bindings |
| `Source/WebUI/index.html` | 3 new `<phantom-knob>` elements, rename Max Freq label to "Hi" |
| `tests/WaveletSynthTests.cpp` | New tests for sub-sample interpolation accuracy, boost behavior |
| `tests/ParameterTests.cpp` | Update param count, add REQUIRE checks for new params |

## Testing Strategy

- **Sub-sample interpolation**: Feed a known-frequency sine, compare `getEstimatedHz()` accuracy before/after. The interpolated version should have tighter margins (e.g., 100Hz within 1Hz instead of 5Hz).
- **Min Freq**: Set minFreq to 100Hz, feed 50Hz sine, verify `getEstimatedHz()` returns 0 or default (crossings rejected as too slow).
- **Threshold & Boost**: Feed signal, verify output RMS increases when boost is active and wavelet peak exceeds threshold. Verify boost returns to 1.0 during silence.
