# Wavelet Length + Gate Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two RESYN-mode knobs — Length (gates wavelet output to the first N% of each period with cosine fade) and Gate (hysteresis threshold on zero-crossing detection) — wired from Parameters → WaveletSynth → PhantomEngine → PluginProcessor → UI.

**Architecture:** WaveletSynth gets two new SmoothedValue members, two setters, and per-sample logic in `process()`. PhantomEngine forwards to `resynL`/`resynR` only. Parameters.h declares the APVTS entries. PluginProcessor reads them in `syncParamsToEngine()`. UI gets two medium knobs in the Harmonic Engine row.

**Tech Stack:** JUCE (AudioParameterFloat, SmoothedValue, IIRFilter), Catch2 (tests), vanilla JS / HTML (UI)

---

## File Map

| File | Change |
|---|---|
| `tests/WaveletSynthTests.cpp` | **Create** — 4 unit tests for new WaveletSynth behaviour |
| `tests/CMakeLists.txt` | Add `WaveletSynthTests.cpp` to test executable |
| `Source/Engines/WaveletSynth.h` | Add 2 public setters + 3 private members |
| `Source/Engines/WaveletSynth.cpp` | Implement setters; update `prepare()`, `reset()`, `process()` |
| `Source/Parameters.h` | Add `SYNTH_WAVELET_LENGTH`, `SYNTH_GATE_THRESHOLD` |
| `Source/Engines/PhantomEngine.h` | Add 2 public setter declarations |
| `Source/Engines/PhantomEngine.cpp` | Implement 2 forwarding setters |
| `Source/PluginProcessor.cpp` | Read both params in `syncParamsToEngine()` |
| `Source/WebUI/index.html` | Add Length + Gate knobs to Harmonic Engine row |

---

## Key Facts for All Tasks

**WaveletSynth.cpp current structure (line references):**
- `prepare()`: lines 9–24. Existing smoothers init at lines 18–21.
- `reset()`: lines 26–34. Last reset line is `estimatedPeriod = ...` at line 33.
- `setters`: lines 36–68.
- `process()`: lines 106–184.
  - Zero-crossing detection block: lines 123–159.
  - Valid crossing branch: lines 127–151.
  - Phase reset inside valid branch: line 149 (`currentPhase = 0.0f`), line 150 (`samplesSinceLastCrossing = 0.0f`).
  - Shape smoothers: lines 168–169.
  - Synthesis loop: lines 171–183.
  - `return y`: line 183.

**WaveletSynth.h current private members** (lines 33–53): `sampleRate`, `lastSample`, `samplesSinceLastCrossing`, `accumulatedSamples`, `crossingsAccum`, `skipCount`, `estimatedPeriod`, `trackingAlpha`, `minPeriodSamples`, `maxPeriodSamples`, `currentPhase`, `amps`, `smoothStep`, `smoothDuty`.

**Parameters.h layout:** `SYNTH_HPF_HZ` is the last entry in the Synth Filter section (line 50). New params go after it.

**syncParamsToEngine() location:** `Source/PluginProcessor.cpp`, the function ends with `engine.setSynthMode(...)` at line 63.

---

### Task 1: Write failing tests + add WaveletSynth declarations

**Files:**
- Create: `tests/WaveletSynthTests.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `Source/Engines/WaveletSynth.h`

- [ ] **Step 1: Create test file**

Create `tests/WaveletSynthTests.cpp` with this exact content:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Engines/WaveletSynth.h"
#include <cmath>

TEST_CASE("WaveletSynth: setters and prepare/reset do not crash")
{
    WaveletSynth syn;
    syn.prepare(44100.0);
    syn.setWaveletLength(0.5f);
    syn.setGateThreshold(0.3f);
    syn.reset();
    syn.setWaveletLength(1.0f);
    syn.setGateThreshold(0.0f);
    SUCCEED();
}

TEST_CASE("WaveletSynth: default params produce same output as explicit defaults")
{
    // Explicit length=1.0 / gate=0.0 must be identical to factory default
    WaveletSynth a, b;
    a.prepare(44100.0);
    b.prepare(44100.0);
    b.setWaveletLength(1.0f);
    b.setGateThreshold(0.0f);

    const float w = 2.0f * juce::MathConstants<float>::pi * 200.0f / 44100.0f;
    for (int i = 0; i < 2048; ++i)
    {
        const float x = std::sin(w * (float)i);
        REQUIRE(a.process(x) == Catch::Approx(b.process(x)).margin(1e-5f));
    }
}

TEST_CASE("WaveletSynth: gate threshold blocks low-amplitude crossings")
{
    // Low-amplitude signal (0.2) should NOT update period when gate=0.5
    // because the negative swing (-0.2) never reaches -threshold (-0.5).
    // The same signal DOES update period when gate=0.0.
    WaveletSynth gated, ungated;
    gated.prepare(44100.0);
    ungated.prepare(44100.0);
    gated.setGateThreshold(0.5f);
    ungated.setGateThreshold(0.0f);

    const float freq = 200.0f;
    const float amp  = 0.2f;
    const float w    = 2.0f * juce::MathConstants<float>::pi * freq / 44100.0f;

    for (int i = 0; i < 8820; ++i)
    {
        const float x = amp * std::sin(w * (float)i);
        gated.process(x);
        ungated.process(x);
    }

    // Ungated: should have converged toward 200 Hz
    REQUIRE(ungated.getEstimatedHz() == Catch::Approx(200.0f).margin(15.0f));
    // Gated: period estimate should stay near default (100 Hz — no valid crossings)
    REQUIRE(gated.getEstimatedHz() == Catch::Approx(100.0f).margin(20.0f));
}

TEST_CASE("WaveletSynth: reduced length reduces output RMS")
{
    // length=0.1 should produce significantly less energy than length=1.0
    auto measureRMS = [](float length, float freq) -> float {
        WaveletSynth syn;
        syn.prepare(44100.0);
        syn.setWaveletLength(length);
        const float w = 2.0f * juce::MathConstants<float>::pi * freq / 44100.0f;
        // Warm-up: let period converge
        for (int i = 0; i < 4410; ++i)
            syn.process(std::sin(w * (float)i));
        // Measure RMS over 8820 samples (200 ms)
        float sum = 0.0f;
        for (int i = 0; i < 8820; ++i)
        {
            const float y = syn.process(std::sin(w * (float)(i + 4410)));
            sum += y * y;
        }
        return std::sqrt(sum / 8820.0f);
    };

    const float rms100 = measureRMS(1.0f, 200.0f);
    const float rms10  = measureRMS(0.1f, 200.0f);

    REQUIRE(rms100 > 0.01f);               // sanity: synth produced output
    REQUIRE(rms10 < rms100 * 0.5f);        // 10% length → < half the energy
}
```

- [ ] **Step 2: Add test file to CMakeLists.txt**

In `tests/CMakeLists.txt`, find:
```cmake
add_executable(KaigenPhantomTests
    ParameterTests.cpp
```

Change to:
```cmake
add_executable(KaigenPhantomTests
    ParameterTests.cpp
    WaveletSynthTests.cpp
```

- [ ] **Step 3: Add declarations to WaveletSynth.h**

In `Source/Engines/WaveletSynth.h`, find the public setter block:
```cpp
    void setSkipCount(int n) noexcept;
```

Change to:
```cpp
    void setSkipCount(int n) noexcept;
    void setWaveletLength(float len) noexcept;  // 0.05–1.0: fraction of period to synthesise
    void setGateThreshold(float thr) noexcept;  // 0.0–1.0: min negative-peak amplitude for valid crossing
```

Then find the private members block:
```cpp
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothStep;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDuty;
```

Change to:
```cpp
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothStep;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDuty;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothLength;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothGate;
    float lastNegativePeak = 0.0f;
```

- [ ] **Step 4: Run tests to confirm they fail (setters not yet implemented)**

```bash
cd "C:\Documents\NEw project\Kaigen Phantom\build"
cmake --build . --target KaigenPhantomTests --config Debug 2>&1 | tail -20
```

Expected: compile error — `setWaveletLength` and `setGateThreshold` undefined (linker error is fine here).

- [ ] **Step 5: Commit**

```bash
git add tests/WaveletSynthTests.cpp tests/CMakeLists.txt "Source/Engines/WaveletSynth.h"
git commit -m "test: add WaveletSynth length/gate tests; declare new members in header"
```

---

### Task 2: Implement WaveletSynth setters, prepare, reset

**Files:**
- Modify: `Source/Engines/WaveletSynth.cpp`

- [ ] **Step 1: Add setters**

In `WaveletSynth.cpp`, find:
```cpp
void WaveletSynth::setSkipCount(int n) noexcept
```

Insert these two setters immediately before `setSkipCount`:

```cpp
void WaveletSynth::setWaveletLength(float len) noexcept
{
    smoothLength.setTargetValue(juce::jlimit(0.05f, 1.0f, len));
}

void WaveletSynth::setGateThreshold(float thr) noexcept
{
    smoothGate.setTargetValue(juce::jlimit(0.0f, 1.0f, thr));
}

```

- [ ] **Step 2: Update prepare() to initialise new smoothers**

In `prepare()`, find:
```cpp
    smoothDuty.reset(sr, rampSec);
    smoothDuty.setCurrentAndTargetValue(0.5f);
```

Change to:
```cpp
    smoothDuty.reset(sr, rampSec);
    smoothDuty.setCurrentAndTargetValue(0.5f);
    smoothLength.reset(sr, rampSec);
    smoothLength.setCurrentAndTargetValue(1.0f);
    smoothGate.reset(sr, rampSec);
    smoothGate.setCurrentAndTargetValue(0.0f);
```

- [ ] **Step 3: Update reset() to clear lastNegativePeak**

In `reset()`, find:
```cpp
    estimatedPeriod          = (float)(sampleRate / 100.0); // 100 Hz safe default
```

Change to:
```cpp
    estimatedPeriod          = (float)(sampleRate / 100.0); // 100 Hz safe default
    lastNegativePeak         = 0.0f;
```

- [ ] **Step 4: Run tests — first two should now pass**

```bash
cd "C:\Documents\NEw project\Kaigen Phantom\build"
cmake --build . --target KaigenPhantomTests --config Debug && ctest -R "WaveletSynth" -V
```

Expected: "WaveletSynth: setters and prepare/reset do not crash" ✅ and "WaveletSynth: default params produce same output as explicit defaults" ✅. The gate and length behaviour tests may still fail (process() not yet updated).

- [ ] **Step 5: Commit**

```bash
git add "Source/Engines/WaveletSynth.cpp"
git commit -m "feat: add WaveletSynth setWaveletLength/setGateThreshold setters and smoother init"
```

---

### Task 3: WaveletSynth — gate threshold in process()

**Files:**
- Modify: `Source/Engines/WaveletSynth.cpp` — `process()` only

- [ ] **Step 1: Add negative peak tracking + read gate smoother at top of process()**

In `process()`, find the very first lines:
```cpp
    samplesSinceLastCrossing += 1.0f;
    accumulatedSamples       += 1.0f;
```

Insert immediately before them:
```cpp
    // Track most negative excursion since last valid crossing (gate hysteresis)
    if (x < lastNegativePeak)
        lastNegativePeak = x;
    const float gateThr = smoothGate.getNextValue();

```

- [ ] **Step 2: Add gate condition to the valid-crossing check**

Find:
```cpp
        if (samplesSinceLastCrossing >= minPeriodSamples &&
            samplesSinceLastCrossing <= maxPeriodSamples)
```

Change to:
```cpp
        if (samplesSinceLastCrossing >= minPeriodSamples &&
            samplesSinceLastCrossing <= maxPeriodSamples &&
            lastNegativePeak <= -gateThr)
```

- [ ] **Step 3: Reset lastNegativePeak after a valid crossing**

Find (inside the valid crossing branch, the two reset lines at the end):
```cpp
            currentPhase             = 0.0f;
            samplesSinceLastCrossing = 0.0f;
```

Change to:
```cpp
            currentPhase             = 0.0f;
            samplesSinceLastCrossing = 0.0f;
            lastNegativePeak         = 0.0f;
```

- [ ] **Step 4: Run gate test**

```bash
cd "C:\Documents\NEw project\Kaigen Phantom\build"
cmake --build . --target KaigenPhantomTests --config Debug && ctest -R "gate threshold" -V
```

Expected: "WaveletSynth: gate threshold blocks low-amplitude crossings" ✅

- [ ] **Step 5: Commit**

```bash
git add "Source/Engines/WaveletSynth.cpp"
git commit -m "feat: add gate threshold hysteresis to WaveletSynth zero-crossing detection"
```

---

### Task 4: WaveletSynth — length gate in process()

**Files:**
- Modify: `Source/Engines/WaveletSynth.cpp` — `process()` only

- [ ] **Step 1: Add smoothLength read alongside existing smoothers**

In `process()`, find:
```cpp
    // ── Advance shape smoothers (one step per audio sample) ──────────────
    const float step = smoothStep.getNextValue();
    const float duty = smoothDuty.getNextValue();
```

Change to:
```cpp
    // ── Advance shape smoothers (one step per audio sample) ──────────────
    const float step = smoothStep.getNextValue();
    const float duty = smoothDuty.getNextValue();
    const float len  = smoothLength.getNextValue();
```

- [ ] **Step 2: Apply length gate after synthesis, before return**

Find the last line of process():
```cpp
    return y;
```

Change to:
```cpp
    // ── Length gate: silence output after len×2π of each wavelet ────────
    if (len < 1.0f)
    {
        const float gateEnd   = len * kTwoPi;
        const float fadeStart = gateEnd * 0.8f;   // cosine fade = last 20% of active zone
        if (currentPhase >= gateEnd)
            y = 0.0f;
        else if (currentPhase >= fadeStart)
        {
            const float t = (currentPhase - fadeStart) / (gateEnd - fadeStart); // 0→1
            y *= 0.5f * (1.0f + std::cos(kPi * t));  // cosine window 1→0
        }
    }
    return y;
```

- [ ] **Step 3: Run all WaveletSynth tests**

```bash
cd "C:\Documents\NEw project\Kaigen Phantom\build"
cmake --build . --target KaigenPhantomTests --config Debug && ctest -R "WaveletSynth" -V
```

Expected: all 4 WaveletSynth tests ✅

- [ ] **Step 4: Run full test suite to check no regressions**

```bash
ctest --output-on-failure
```

Expected: same pass/fail count as before this feature (3 pre-existing failures in ParameterTests, SerializationTests, WaveshaperTests are unrelated — any new failures are regressions).

- [ ] **Step 5: Commit**

```bash
git add "Source/Engines/WaveletSynth.cpp"
git commit -m "feat: add phase-based length gate with cosine fade to WaveletSynth"
```

---

### Task 5: Parameters.h + PhantomEngine + PluginProcessor wiring

**Files:**
- Modify: `Source/Parameters.h`
- Modify: `Source/Engines/PhantomEngine.h`
- Modify: `Source/Engines/PhantomEngine.cpp`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Add ParamIDs to Parameters.h**

Find in the `// ── Synth Filter` section:
```cpp
    /** High-pass filter on synthesised harmonics. 20–2000 Hz. Default 20 (transparent). */
    inline constexpr auto SYNTH_HPF_HZ      = "synth_hpf_hz";
```

Change to:
```cpp
    /** High-pass filter on synthesised harmonics. 20–2000 Hz. Default 20 (transparent). */
    inline constexpr auto SYNTH_HPF_HZ      = "synth_hpf_hz";

    // ── RESYN (WaveletSynth) controls ─────────────────────────────────────
    /** Fraction of each wavelet period to synthesise. 0.05–1.0. Default 1.0 (full). */
    inline constexpr auto SYNTH_WAVELET_LENGTH = "synth_wavelet_length";
    /** Gate threshold: min negative-peak amplitude for a crossing to be valid. 0–1. Default 0. */
    inline constexpr auto SYNTH_GATE_THRESHOLD = "synth_gate_threshold";
```

- [ ] **Step 2: Add to getAllParameterIDs()**

Find:
```cpp
        ParamID::SYNTH_HPF_HZ,
```

Change to:
```cpp
        ParamID::SYNTH_HPF_HZ,
        ParamID::SYNTH_WAVELET_LENGTH,
        ParamID::SYNTH_GATE_THRESHOLD,
```

- [ ] **Step 3: Add to createParameterLayout()**

Find (after the HPF parameter push_back):
```cpp
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_HPF_HZ, "Synth HPF",
        NormalisableRange<float>(20.0f, 2000.0f, 0.0f, 0.3f), 20.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));
```

Insert after it:
```cpp
    // ── RESYN controls ────────────────────────────────────────────────
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_WAVELET_LENGTH, "Wavelet Length",
        NormalisableRange<float>(0.05f, 1.0f), 1.0f));
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_GATE_THRESHOLD, "Gate Threshold",
        NormalisableRange<float>(0.0f, 1.0f), 0.0f));
```

- [ ] **Step 4: Add setter declarations to PhantomEngine.h**

Find:
```cpp
    void setSynthMode(int mode);           // 0 = Effect (ZCS), 1 = RESYN (WaveletSynth)
```

Change to:
```cpp
    void setSynthMode(int mode);           // 0 = Effect (ZCS), 1 = RESYN (WaveletSynth)
    void setWaveletLength(float len);      // RESYN only: 0.05–1.0 fraction of period
    void setGateThreshold(float thr);      // RESYN only: 0.0–1.0 min negative-peak threshold
```

- [ ] **Step 5: Implement setters in PhantomEngine.cpp**

Find:
```cpp
void PhantomEngine::setSynthMode(int mode)
```

Insert these two setters immediately before `setSynthMode`:

```cpp
void PhantomEngine::setWaveletLength(float len)
{
    resynL.setWaveletLength(len);
    resynR.setWaveletLength(len);
    // synthL/synthR not forwarded — Length is RESYN-only
}

void PhantomEngine::setGateThreshold(float thr)
{
    resynL.setGateThreshold(thr);
    resynR.setGateThreshold(thr);
    // synthL/synthR not forwarded — Gate is RESYN-only
}

```

- [ ] **Step 6: Wire up in syncParamsToEngine()**

In `Source/PluginProcessor.cpp`, find:
```cpp
    engine.setSynthMode((int) apvts.getRawParameterValue(ParamID::MODE)->load());
```

Change to:
```cpp
    engine.setSynthMode((int) apvts.getRawParameterValue(ParamID::MODE)->load());
    engine.setWaveletLength(apvts.getRawParameterValue(ParamID::SYNTH_WAVELET_LENGTH)->load());
    engine.setGateThreshold(apvts.getRawParameterValue(ParamID::SYNTH_GATE_THRESHOLD)->load());
```

- [ ] **Step 7: Build and run tests**

```bash
cd "C:\Documents\NEw project\Kaigen Phantom\build"
cmake --build . --target KaigenPhantomTests --config Debug && ctest --output-on-failure
```

Expected: same result as before (4 new WaveletSynth tests pass, no new failures).

- [ ] **Step 8: Commit**

```bash
git add "Source/Parameters.h" "Source/Engines/PhantomEngine.h" "Source/Engines/PhantomEngine.cpp" "Source/PluginProcessor.cpp"
git commit -m "feat: wire synth_wavelet_length and synth_gate_threshold through Parameters → PhantomEngine → PluginProcessor"
```

---

### Task 6: UI — add Length and Gate knobs

**Files:**
- Modify: `Source/WebUI/index.html`

- [ ] **Step 1: Add two knobs to the Harmonic Engine row**

In `index.html`, find:
```html
              <phantom-knob data-param="synth_skip" size="medium" label="Skip" default-value="1"></phantom-knob>
```

Change to:
```html
              <phantom-knob data-param="synth_wavelet_length" size="medium" label="Length" default-value="1"></phantom-knob>
              <phantom-knob data-param="synth_gate_threshold" size="medium" label="Gate"   default-value="0"></phantom-knob>
              <phantom-knob data-param="synth_skip" size="medium" label="Skip" default-value="1"></phantom-knob>
```

Length and Gate come before Skip so they group with Push (all RESYN-flavoured controls together).

- [ ] **Step 2: Visual verification — Effect mode**

Load the plugin in Effect mode. The Harmonic Engine row should now show: Saturation (large) · Strength · Shape (large) · Push · Length · Gate · Skip. All knobs render with correct labels. Length shows "1.00" (or 100%), Gate shows "0.00" (or 0%). No overflow or clipping.

- [ ] **Step 3: Visual verification — RESYN mode, sweep Length**

Switch to RESYN mode, play a sustained bass note. Sweep the Length knob from 1.0 down to 0.05. The output should go from continuous resynthesis to short sharp bursts at the pitch frequency, with no clicks.

- [ ] **Step 4: Visual verification — RESYN mode, sweep Gate**

With Length at 1.0, sweep Gate upward. On a loud signal the resynthesis should be unaffected until the gate exceeds the signal's amplitude, then drop to silence. On a quiet signal, even a small Gate value should suppress resynthesis.

- [ ] **Step 5: Commit**

```bash
git add "Source/WebUI/index.html"
git commit -m "feat: add Length and Gate knobs to Harmonic Engine row"
```
