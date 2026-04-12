# RESYN Mode Implementation Design

**Date:** 2026-04-12

> **For agentic workers:** Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this spec task-by-task.

**Goal:** Add a RESYN synthesis mode that replaces the ZeroCrossingSynth with a wavelet resynthesizer — at every positive-slope zero crossing the oscillator phase resets to 0, producing a fresh wavelet per interval rather than a continuous oscillator. The mode replaces the existing "Instrument" mode slot in the header toggle.

**Architecture:** New `WaveletSynth` engine (parallel to `ZeroCrossingSynth`). `PhantomEngine` routes to either engine based on the `mode` parameter. All downstream processing (envelope follower, LPF/HPF, ghost mix, binaural, stereo) is unchanged.

**Tech Stack:** JUCE, C++17, existing plugin infrastructure.

---

## What Changes

### New file: `Source/Engines/WaveletSynth.h` / `.cpp`

Mirrors the `ZeroCrossingSynth` interface so `PhantomEngine` can swap between them cleanly.

**Interface:**
```cpp
class WaveletSynth
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setHarmonicAmplitudes(const std::array<float, 7>& amps) noexcept; // H2..H8 (H1 implicit = 1.0)
    void setStep(float step) noexcept;      // 0=sine 1=square
    void setDutyCycle(float duty) noexcept; // 0.05–0.95
    void setSkipCount(int n) noexcept;      // 1–8

    float process(float x) noexcept;
    float getEstimatedHz() const noexcept;

private:
    double sampleRate = 44100.0;
    float minPeriodSamples = 0.0f;
    float maxPeriodSamples = 0.0f;

    float lastSample               = 0.0f;
    float currentPhase             = 0.0f;   // resets to 0 at each positive crossing
    float estimatedPeriod          = 441.0f; // 100 Hz default
    float samplesSinceLastCrossing = 0.0f;
    float accumulatedSamples       = 0.0f;
    int   crossingsAccum           = 0;
    int   skipCount                = 1;
    float trackingAlpha            = 0.15f;

    std::array<float, 7> amps {};  // H2..H8
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothStep;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDuty;

    // Shared waveform helpers (same as ZeroCrossingSynth)
    static float warpPhase(float phase, float duty) noexcept;
    static float shapedWave(float wp, float step) noexcept;
};
```

**Key difference from ZeroCrossingSynth:**

1. **H1 is always synthesised at amplitude 1.0.** The recipe H2–H8 control the additive harmonic content on top. This gives the resynthesised output a solid fundamental that ZCS deliberately omits.

2. **Phase resets to 0 at every positive-slope zero crossing** (after period update). ZCS maintains continuous phase and only updates its period estimate. In WaveletSynth, `currentPhase = 0.0f` at each valid crossing — each interval between crossings is a fresh wavelet aligned to the source at its boundaries.

3. **Synthesis of H1:** The synthesis loop runs `n = 1..8` instead of `n = 2..8`:
   ```cpp
   // H1 always at 1.0
   float hp1 = std::fmod(1.0f * currentPhase, kTwoPi);
   y = shapedWave(warpPhase(hp1, duty), step);

   // H2–H8 from recipe
   for (int i = 0; i < 7; ++i)
   {
       if (amps[i] <= 0.0f) continue;
       float hp = std::fmod((float)(i + 2) * currentPhase, kTwoPi);
       y += amps[i] * shapedWave(warpPhase(hp, duty), step);
   }
   ```

4. **Period detection is otherwise identical to ZCS:** same ±20% rate-limiting, same skip accumulator, same min/max period bounds (16 Hz – 4 kHz).

---

### Modified: `Source/Engines/PhantomEngine.h`

Add `WaveletSynth` members alongside the existing `ZeroCrossingSynth` members:

```cpp
#include "WaveletSynth.h"

// existing:
ZeroCrossingSynth synthL;
ZeroCrossingSynth synthR;

// new:
WaveletSynth resynL;
WaveletSynth resynR;

// new mode flag:
int synthMode = 0;  // 0 = Effect (ZCS), 1 = RESYN (WaveletSynth)
```

Add setter:
```cpp
void setSynthMode(int mode);  // 0 = Effect, 1 = RESYN
```

---

### Modified: `Source/Engines/PhantomEngine.cpp`

**In `prepare()`:** call `resynL.prepare(sr)` and `resynR.prepare(sr)` alongside existing synth prepares.

**In `reset()`:** call `resynL.reset()` and `resynR.reset()`.

**In `setHarmonicAmplitudes()` / `setSynthStep()` / `setSynthDuty()` / `setSynthSkip()`:** forward to both ZCS and WaveletSynth instances so both stay in sync with recipe wheel changes regardless of current mode.

**In `process()`, inner loop — replace:**
```cpp
float phantomSample = syn.process(trackIn);
```
**With:**
```cpp
float phantomSample = (synthMode == 1)
    ? ((ch == 0) ? resynL : resynR).process(trackIn)
    : syn.process(trackIn);
```

**`setSynthMode()`:**
```cpp
void PhantomEngine::setSynthMode(int mode)
{
    synthMode = juce::jlimit(0, 1, mode);
}
```

---

### Modified: `Source/Parameters.h`

Change the `MODE` parameter choices from `{"Effect", "Instrument"}` to `{"Effect", "RESYN"}`:

```cpp
params.push_back(std::make_unique<APC>(
    ParamID::MODE, "Mode",
    StringArray{ "Effect", "RESYN" }, 0));
```

---

### Modified: `Source/PluginProcessor.cpp`

In `syncParamsToEngine()`, add:
```cpp
engine.setSynthMode((int) apvts.getRawParameterValue(ParamID::MODE)->load());
```

---

### Modified: `Source/WebUI/index.html`

Rename the Instrument mode button label:
```html
<!-- before -->
<div class="mb" data-mode="1">Instrument</div>
<!-- after -->
<div class="mb" data-mode="1">Resyn</div>
```

---

### Modified: `tests/PhantomEngineTests.cpp`

Add two new test cases:

1. **RESYN mode: sine input produces nonzero output** — verify wavelet synthesis fires when a sine is fed in with `mode=1`.

2. **RESYN mode: silence in → silence out** — verify no phantom output with silent input in RESYN mode.

---

## What Does NOT Change

- `BassExtractor` — identical in both modes
- `EnvelopeFollower` — identical in both modes
- LPF/HPF filter chain — identical in both modes
- Ghost mix (Replace/Add) — identical in both modes
- `BinauralStage`, stereo width, output gain — identical in both modes
- Recipe wheel H2–H8 — works in both modes (H1 is implicit in RESYN, not on the wheel)
- All existing presets (Stable, Warm, etc.) — apply in both modes
- Envelope attack/release, crossover Hz, phantom strength — all apply in both modes

---

## Behaviour Summary

| | Effect mode | RESYN mode |
|---|---|---|
| Synthesis | ZeroCrossingSynth | WaveletSynth |
| Phase behaviour | Continuous, period updated at crossings | Resets to 0 at every positive crossing |
| H1 (fundamental) | Never synthesised | Always 1.0 |
| H2–H8 | Recipe wheel | Recipe wheel (same) |
| Character | Smooth harmonic enhancement | Grittier wavelet reconstruction |
| All downstream DSP | Shared | Shared |

---

## Non-Goals

- Instrument/MIDI mode is removed, not deferred — the slot is permanently RESYN
- No new knobs, panels, or parameters beyond the mode rename
- No H1 user control — it is always 1.0 in RESYN mode
- Gate threshold remains hardcoded at -40 dBFS (kTrackingGate = 0.01f), not exposed as a UI control
