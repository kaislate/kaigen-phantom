# RESYN Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a RESYN synthesis mode that replaces ZeroCrossingSynth with a wavelet resynthesizer that phase-resets at every zero crossing and includes H1 (fundamental), activated by renaming the "Instrument" header toggle to "Resyn".

**Architecture:** New `WaveletSynth` class mirrors the `ZeroCrossingSynth` interface. `PhantomEngine` holds both instances and routes to either based on `synthMode`. All downstream DSP (envelope, filters, ghost mix, binaural, stereo) is unchanged.

**Tech Stack:** JUCE, C++17, Catch2 v3 tests, CMake/MSBuild.

**Build command (use throughout):**
```
"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build build --config Debug --target KaigenPhantomTests
```
**VST3 build:**
```
"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build build --config Debug --target KaigenPhantom_VST3
```
**Run tests:**
```
./build/tests/Debug/KaigenPhantomTests.exe
```

---

## File Map

| Action | File | What changes |
|--------|------|--------------|
| Create | `Source/Engines/WaveletSynth.h` | New class declaration |
| Create | `Source/Engines/WaveletSynth.cpp` | New class implementation |
| Modify | `Source/Engines/PhantomEngine.h` | Add `WaveletSynth` members, `synthMode`, `setSynthMode()` |
| Modify | `Source/Engines/PhantomEngine.cpp` | Prepare/reset/setters/process routing |
| Modify | `Source/Parameters.h` | MODE choices: `{"Effect","RESYN"}` |
| Modify | `Source/PluginProcessor.cpp` | Add `setSynthMode` call in `syncParamsToEngine()` |
| Modify | `Source/WebUI/index.html` | Rename "Instrument" button to "Resyn" |
| Modify | `tests/CMakeLists.txt` | Add `WaveletSynth.cpp` to test sources |
| Modify | `tests/PhantomEngineTests.cpp` | Two new RESYN test cases |

---

## Task 1: Write failing RESYN tests

**Files:**
- Modify: `tests/PhantomEngineTests.cpp`

- [ ] **Step 1: Add two RESYN test cases at the bottom of PhantomEngineTests.cpp**

Append after the last `TEST_CASE` (after line 127):

```cpp
TEST_CASE("PhantomEngine: RESYN mode produces nonzero output for sine input")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setSynthMode(1);          // will not compile until Task 3
    eng.setCrossoverHz(150.0f);
    eng.setGhostAmount(1.0f);
    eng.setGhostReplace(false);
    eng.setPhantomStrength(1.0f);

    auto buf = makeSineBuffer(60.0f, 0.5f, 4096);
    eng.process(buf);

    float maxOut = 0.0f;
    for (int i = 2048; i < 4096; ++i)
        maxOut = juce::jmax(maxOut, std::fabs(buf.getSample(0, i)));

    REQUIRE(maxOut > 0.1f);
}

TEST_CASE("PhantomEngine: RESYN mode silence in -> silence out")
{
    PhantomEngine eng;
    eng.prepare(44100.0, 512, 2);
    eng.setSynthMode(1);
    eng.setGhostAmount(1.0f);
    eng.setGhostReplace(true);
    eng.setPhantomStrength(1.0f);

    juce::AudioBuffer<float> buf(2, 1024);
    buf.clear();
    eng.process(buf);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            REQUIRE(std::fabs(buf.getSample(ch, i)) < 1e-4f);
}
```

- [ ] **Step 2: Confirm the build fails with a clear compiler error**

Run the test build command. Expected: compile error — `PhantomEngine` has no member `setSynthMode`. This confirms the test is actually exercising new code.

```
error C2039: 'setSynthMode': is not a member of 'PhantomEngine'
```

---

## Task 2: Create WaveletSynth

**Files:**
- Create: `Source/Engines/WaveletSynth.h`
- Create: `Source/Engines/WaveletSynth.cpp`

- [ ] **Step 1: Create `Source/Engines/WaveletSynth.h`**

```cpp
#pragma once
#include <JuceHeader.h>
#include <array>

/**
 * WaveletSynth — zero-crossing-triggered wavelet resynthesizer.
 *
 * Like ZeroCrossingSynth, detects positive-slope zero crossings to estimate
 * the instantaneous period. Unlike ZCS, the oscillator phase resets to 0 at
 * every valid crossing — each interval is a fresh wavelet aligned to the
 * source at its zero-crossing boundaries.
 *
 * Synthesises H1 (fundamental, fixed amplitude 1.0) + H2-H8 (recipe).
 */
class WaveletSynth
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    /** Per-harmonic amplitudes H2..H8 (H1 is always 1.0). */
    void setHarmonicAmplitudes(const std::array<float, 7>& amps) noexcept;
    void setStep(float step) noexcept;
    void setDutyCycle(float duty) noexcept;
    void setSkipCount(int n) noexcept;
    void setTrackingSpeed(float speed) noexcept;

    float process(float x) noexcept;
    float getEstimatedHz() const noexcept;

private:
    double sampleRate = 44100.0;

    float lastSample               = 0.0f;
    float samplesSinceLastCrossing = 0.0f;
    float accumulatedSamples       = 0.0f;
    int   crossingsAccum           = 0;
    int   skipCount                = 1;
    float estimatedPeriod          = 441.0f;
    float trackingAlpha            = 0.15f;
    float minPeriodSamples         = 0.0f;
    float maxPeriodSamples         = 0.0f;

    float currentPhase = 0.0f;   // resets to 0.0 at every positive zero crossing

    std::array<float, 7> amps {};   // H2..H8 amplitudes

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothStep;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDuty;

    static float warpPhase(float phase, float duty) noexcept;
    static float shapedWave(float wp, float step) noexcept;
};
```

- [ ] **Step 2: Create `Source/Engines/WaveletSynth.cpp`**

```cpp
#include "WaveletSynth.h"
#include <cmath>

static constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
static constexpr float kPi    = juce::MathConstants<float>::pi;

void WaveletSynth::prepare(double sr) noexcept
{
    sampleRate = sr;
    minPeriodSamples = (float)(sr / 4000.0);
    maxPeriodSamples = (float)(sr / 16.0);

    const double rampSec = 0.010;
    smoothStep.reset(sr, rampSec);
    smoothDuty.reset(sr, rampSec);
    smoothStep.setCurrentAndTargetValue(0.0f);
    smoothDuty.setCurrentAndTargetValue(0.5f);

    reset();
}

void WaveletSynth::reset() noexcept
{
    lastSample               = 0.0f;
    currentPhase             = 0.0f;
    samplesSinceLastCrossing = 0.0f;
    accumulatedSamples       = 0.0f;
    crossingsAccum           = 0;
    estimatedPeriod          = (float)(sampleRate / 100.0);
}

void WaveletSynth::setHarmonicAmplitudes(const std::array<float, 7>& newAmps) noexcept
{
    for (int i = 0; i < 7; ++i)
        amps[(size_t)i] = juce::jlimit(0.0f, 1.0f, newAmps[(size_t)i]);
}

void WaveletSynth::setStep(float s) noexcept
{
    smoothStep.setTargetValue(juce::jlimit(0.0f, 1.0f, s));
}

void WaveletSynth::setDutyCycle(float d) noexcept
{
    smoothDuty.setTargetValue(juce::jlimit(0.05f, 0.95f, d));
}

void WaveletSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(1, 8, n);
    if (newSkip != skipCount)
    {
        skipCount          = newSkip;
        crossingsAccum     = 0;
        accumulatedSamples = 0.0f;
    }
}

void WaveletSynth::setTrackingSpeed(float speed) noexcept
{
    trackingAlpha = juce::jlimit(0.01f, 0.80f, speed);
}

float WaveletSynth::getEstimatedHz() const noexcept
{
    return estimatedPeriod > 0.0f ? (float)(sampleRate / (double)estimatedPeriod) : 0.0f;
}

float WaveletSynth::warpPhase(float phase, float duty) noexcept
{
    const float d = juce::jlimit(0.05f, 0.95f, duty);
    if (phase < kTwoPi * d)
        return phase / (2.0f * d);
    else
        return kPi + (phase - kTwoPi * d) / (2.0f * (1.0f - d));
}

float WaveletSynth::shapedWave(float wp, float step) noexcept
{
    const float s = std::sin(wp);
    if (step <= 0.0f) return s;
    const float drive = 1.0f + step * 19.0f;
    const float tanhD = std::tanh(drive);
    return std::tanh(s * drive) / tanhD;
}

float WaveletSynth::process(float x) noexcept
{
    samplesSinceLastCrossing += 1.0f;
    accumulatedSamples       += 1.0f;

    if (lastSample <= 0.0f && x > 0.0f)
    {
        if (samplesSinceLastCrossing >= minPeriodSamples &&
            samplesSinceLastCrossing <= maxPeriodSamples)
        {
            crossingsAccum++;

            if (crossingsAccum >= skipCount)
            {
                // Rate-limit period changes to ±20% per measurement
                const float maxDelta = estimatedPeriod * 0.20f;
                const float delta    = accumulatedSamples - estimatedPeriod;
                estimatedPeriod += trackingAlpha * juce::jlimit(-maxDelta, maxDelta, delta);

                accumulatedSamples = 0.0f;
                crossingsAccum     = 0;
            }
        }
        else
        {
            accumulatedSamples = 0.0f;
            crossingsAccum     = 0;
        }

        // KEY DIFFERENCE from ZCS: reset phase at every valid crossing.
        // Each interval is a fresh wavelet starting at phase 0.
        currentPhase             = 0.0f;
        samplesSinceLastCrossing = 0.0f;
    }
    lastSample = x;

    // Advance phase: one full 2π cycle per estimatedPeriod samples
    currentPhase += kTwoPi / estimatedPeriod;
    if (currentPhase >= kTwoPi)
        currentPhase -= kTwoPi;

    const float step = smoothStep.getNextValue();
    const float duty = smoothDuty.getNextValue();

    // H1 at amplitude 1.0 — fundamental carrier, always present in RESYN mode
    float y = shapedWave(warpPhase(currentPhase, duty), step);

    // H2-H8 additive content from recipe wheel
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;
        float hp = std::fmod((float)(i + 2) * currentPhase, kTwoPi);
        y += amps[(size_t)i] * shapedWave(warpPhase(hp, duty), step);
    }

    return y;
}
```

---

## Task 3: Integrate WaveletSynth into PhantomEngine

**Files:**
- Modify: `Source/Engines/PhantomEngine.h`
- Modify: `Source/Engines/PhantomEngine.cpp`

- [ ] **Step 1: Update `Source/Engines/PhantomEngine.h`**

Add `#include "WaveletSynth.h"` after the existing includes at the top.

Add `void setSynthMode(int mode);` to the public parameter setters section, after `setSynthHPF`:

```cpp
void setSynthMode(int mode);        // 0 = Effect (ZCS), 1 = RESYN (WaveletSynth)
```

Add to the private section, after `ZeroCrossingSynth synthR;`:

```cpp
WaveletSynth resynL;
WaveletSynth resynR;
int synthMode = 0;                  // 0 = Effect, 1 = RESYN
```

- [ ] **Step 2: Update `Source/Engines/PhantomEngine.cpp` — prepare()**

After `synthR.prepare(sr);`, add:

```cpp
resynL.prepare(sr);
resynR.prepare(sr);
```

- [ ] **Step 3: Update `Source/Engines/PhantomEngine.cpp` — reset()**

After `synthR.reset();`, add:

```cpp
resynL.reset();
resynR.reset();
```

- [ ] **Step 4: Update `Source/Engines/PhantomEngine.cpp` — parameter setters**

In `setHarmonicAmplitudes()`, after the two existing `synthL`/`synthR` lines, add:
```cpp
resynL.setHarmonicAmplitudes(amps);
resynR.setHarmonicAmplitudes(amps);
```

Replace the three one-liner setters with:
```cpp
void PhantomEngine::setSynthStep(float step)
{
    synthL.setStep(step); synthR.setStep(step);
    resynL.setStep(step); resynR.setStep(step);
}
void PhantomEngine::setSynthDuty(float duty)
{
    synthL.setDutyCycle(duty); synthR.setDutyCycle(duty);
    resynL.setDutyCycle(duty); resynR.setDutyCycle(duty);
}
void PhantomEngine::setSynthSkip(int n)
{
    synthL.setSkipCount(n); synthR.setSkipCount(n);
    resynL.setSkipCount(n); resynR.setSkipCount(n);
}
```

Add the new setter at the end of the setters block:
```cpp
void PhantomEngine::setSynthMode(int mode)
{
    synthMode = juce::jlimit(0, 1, mode);
}
```

- [ ] **Step 5: Update `Source/Engines/PhantomEngine.cpp` — process() inner loop**

In the per-channel loop, the local references are set up as:
```cpp
auto& syn  = (ch == 0) ? synthL     : synthR;
```

Add immediately after that line:
```cpp
auto& resyn = (ch == 0) ? resynL : resynR;
```

Then replace the single line:
```cpp
float phantomSample = syn.process(trackIn);
```
With:
```cpp
float phantomSample = (synthMode == 1)
    ? resyn.process(trackIn)
    : syn.process(trackIn);
```

---

## Task 4: Add WaveletSynth to test build and run tests

**Files:**
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add WaveletSynth.cpp to the test sources list**

In `tests/CMakeLists.txt`, after the line `../Source/Engines/ZeroCrossingSynth.cpp`, add:

```cmake
../Source/Engines/WaveletSynth.cpp
```

The sources block should now read:
```cmake
add_executable(KaigenPhantomTests
    ParameterTests.cpp
    BinauralStageTests.cpp
    SerializationTests.cpp
    BassExtractorTests.cpp
    WaveshaperTests.cpp
    EnvelopeFollowerTests.cpp
    PhantomEngineTests.cpp
    ../Source/Engines/BinauralStage.cpp
    ../Source/Engines/BassExtractor.cpp
    ../Source/Engines/Waveshaper.cpp
    ../Source/Engines/EnvelopeFollower.cpp
    ../Source/Engines/PhantomEngine.cpp
    ../Source/Engines/ZeroCrossingSynth.cpp
    ../Source/Engines/WaveletSynth.cpp
)
```

- [ ] **Step 2: Build and run the test target**

Run the test build command. Expected output includes:
```
WaveletSynth.cpp
KaigenPhantomTests.vcxproj -> ...KaigenPhantomTests.exe
```

Run tests:
```
./build/tests/Debug/KaigenPhantomTests.exe
```

Expected: all tests pass including the two new RESYN cases:
```
All N test cases passed.
```

- [ ] **Step 3: Commit**

```bash
git add Source/Engines/WaveletSynth.h Source/Engines/WaveletSynth.cpp
git add Source/Engines/PhantomEngine.h Source/Engines/PhantomEngine.cpp
git add tests/CMakeLists.txt tests/PhantomEngineTests.cpp
git commit -m "feat: add WaveletSynth and RESYN mode to PhantomEngine"
```

---

## Task 5: Wire the mode parameter

**Files:**
- Modify: `Source/Parameters.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Update MODE parameter choices in `Source/Parameters.h`**

Find the MODE parameter (around line 104):
```cpp
params.push_back(std::make_unique<APC>(
    ParamID::MODE, "Mode", StringArray{ "Effect", "Instrument" }, 0));
```

Replace with:
```cpp
params.push_back(std::make_unique<APC>(
    ParamID::MODE, "Mode", StringArray{ "Effect", "RESYN" }, 0));
```

- [ ] **Step 2: Add `setSynthMode` call in `Source/PluginProcessor.cpp`**

In `syncParamsToEngine()`, add as the last line before the closing brace:

```cpp
engine.setSynthMode((int) apvts.getRawParameterValue(ParamID::MODE)->load());
```

- [ ] **Step 3: Build the VST3**

Run the VST3 build command. Expected: clean build, VST3 installed to `C:\Users\kaislate\AppData\Local\Programs\Common\VST3\Kaigen Phantom.vst3`.

- [ ] **Step 4: Commit**

```bash
git add Source/Parameters.h Source/PluginProcessor.cpp
git commit -m "feat: wire MODE parameter to PhantomEngine synthMode (Effect / RESYN)"
```

---

## Task 6: Update WebUI

**Files:**
- Modify: `Source/WebUI/index.html`

- [ ] **Step 1: Rename the Instrument mode button to Resyn**

In `index.html`, find:
```html
<div class="mb" data-mode="1">Instrument</div>
```

Replace with:
```html
<div class="mb" data-mode="1">Resyn</div>
```

- [ ] **Step 2: Build the VST3**

Run the VST3 build command. Confirm BinaryData regenerates (you will see lines like `Generating juce_binarydata_PhantomWebUI/...`).

- [ ] **Step 3: Verify in Ableton**

Load the plugin. The header toggle should read `Effect | Resyn`. Switching to Resyn should produce a different, grittier sound character on bass content (H1 present, phase resets each crossing). Switching back to Effect should restore the prior ZCS character.

- [ ] **Step 4: Commit**

```bash
git add Source/WebUI/index.html
git commit -m "feat: rename Instrument mode to Resyn in WebUI header toggle"
```
