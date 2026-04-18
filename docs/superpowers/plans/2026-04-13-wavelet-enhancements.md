# Wavelet Synth Enhancements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add three DSP enhancements to the wavelet resynthesis engine: user-controllable minimum tracking frequency, sub-sample zero-crossing interpolation, and per-wavelet upward expansion (Threshold & Boost).

**Architecture:** All changes modify existing files following established patterns. Three new JUCE parameters are added with WebSliderRelay bindings. The DSP changes touch WaveletSynth and ZeroCrossingSynth crossing detection. No new files created.

**Tech Stack:** JUCE 8, C++17, Catch2 tests, WebView2 UI

---

### Task 1: Add Min Frequency parameter and DSP

**Files:**
- Modify: `Source/Parameters.h`
- Modify: `Source/Engines/WaveletSynth.h`
- Modify: `Source/Engines/WaveletSynth.cpp`
- Modify: `Source/Engines/ZeroCrossingSynth.h`
- Modify: `Source/Engines/ZeroCrossingSynth.cpp`
- Modify: `Source/Engines/PhantomEngine.h`
- Modify: `Source/Engines/PhantomEngine.cpp`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `Source/WebUI/index.html`
- Modify: `tests/ParameterTests.cpp`
- Modify: `tests/WaveletSynthTests.cpp`

- [ ] **Step 1: Add parameter ID and definition to Parameters.h**

In `Source/Parameters.h`, add the new constant after `SYNTH_MAX_TRACK_HZ` (line 64):

```cpp
    /** Minimum frequency to track. Crossings slower than this are rejected.
     *  8–200 Hz. Default 8 Hz. Replaces hardcoded 16Hz floor. */
    inline constexpr auto SYNTH_MIN_FREQ_HZ = "synth_min_freq_hz";
```

Add it to `getAllParameterIDs()` (after the `SYNTH_MAX_TRACK_HZ` entry, line 121):

```cpp
        ParamID::SYNTH_MIN_FREQ_HZ,
```

Add the parameter definition in `createParameterLayout()`, after the `SYNTH_MAX_TRACK_HZ` block (after line 237):

```cpp
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_MIN_FREQ_HZ, "Min Track",
        NormalisableRange<float>(8.0f, 200.0f, 0.0f, 0.25f), 8.0f,
        AudioParameterFloatAttributes().withLabel("Hz")));
```

- [ ] **Step 2: Add setMinFreqHz to WaveletSynth**

In `Source/Engines/WaveletSynth.h`, add after `setMaxTrackHz` declaration (line 34):

```cpp
    /** Minimum frequency to track [8–200 Hz]. Crossings slower than this are rejected. */
    void setMinFreqHz(float hz) noexcept;
```

Add member after `maxTrackHz` (line 57):

```cpp
    float minFreqHz                = 8.0f;
```

In `Source/Engines/WaveletSynth.cpp`, add after `setMaxTrackHz` (after line 81):

```cpp
void WaveletSynth::setMinFreqHz(float hz) noexcept
{
    minFreqHz        = juce::jlimit(8.0f, 200.0f, hz);
    maxPeriodSamples = (float)(sampleRate / (double)minFreqHz);
}
```

Replace the hardcoded line in `prepare()` (line 15):

```cpp
    // Old: maxPeriodSamples = (float)(sr / 16.0);
    maxPeriodSamples = (float)(sr / (double)minFreqHz);
```

- [ ] **Step 3: Add setMinFreqHz to ZeroCrossingSynth**

In `Source/Engines/ZeroCrossingSynth.h`, add after `setMaxTrackHz` declaration (line 71):

```cpp
    /** Minimum frequency to track [8–200 Hz]. Crossings slower than this are rejected. */
    void setMinFreqHz(float hz) noexcept;
```

Add member after `maxTrackHz` (line 96):

```cpp
    float minFreqHz               = 8.0f;
```

In `Source/Engines/ZeroCrossingSynth.cpp`, add after `setMaxTrackHz` (after line 80):

```cpp
void ZeroCrossingSynth::setMinFreqHz(float hz) noexcept
{
    minFreqHz        = juce::jlimit(8.0f, 200.0f, hz);
    maxPeriodSamples = (float)(sampleRate / (double)minFreqHz);
}
```

Replace the hardcoded line in `prepare()` (line 15):

```cpp
    // Old: maxPeriodSamples = (float)(sr / 16.0);
    maxPeriodSamples = (float)(sr / (double)minFreqHz);
```

- [ ] **Step 4: Add forwarding setter to PhantomEngine**

In `Source/Engines/PhantomEngine.h`, add after `setMaxTrackHz` (line 53):

```cpp
    void setMinFreqHz(float hz);           // min crossing frequency [8–200 Hz]
```

In `Source/Engines/PhantomEngine.cpp`, add after the `setMaxTrackHz` implementation (after line 165):

```cpp
void PhantomEngine::setMinFreqHz(float hz)
{
    synthL.setMinFreqHz(hz);
    synthR.setMinFreqHz(hz);
    resynL.setMinFreqHz(hz);
    resynR.setMinFreqHz(hz);
}
```

- [ ] **Step 5: Wire parameter in PluginProcessor**

In `Source/PluginProcessor.cpp`, add in `syncParamsToEngine()` after the `setMaxTrackHz` line (after line 74):

```cpp
    engine.setMinFreqHz    (apvts.getRawParameterValue(ParamID::SYNTH_MIN_FREQ_HZ)->load());
```

- [ ] **Step 6: Add relay and attachment in PluginEditor**

In `Source/PluginEditor.h`, add after `synthMaxTrackHzRelay` (line 55):

```cpp
    juce::WebSliderRelay synthMinFreqHzRelay        { "synth_min_freq_hz" };
```

In `Source/PluginEditor.cpp`, add `&self.synthMinFreqHzRelay` to the `sliderRelays[]` array (after `&self.synthMaxTrackHzRelay`, line 49):

```cpp
        &self.synthH1Relay, &self.synthMaxTrackHzRelay, &self.synthMinFreqHzRelay, &self.trackingSpeedRelay,
```

Add to `sliderBindings[]` after the `SYNTH_MAX_TRACK_HZ` entry (after line 182):

```cpp
        { ParamID::SYNTH_MIN_FREQ_HZ,      synthMinFreqHzRelay },
```

- [ ] **Step 7: Add knob to HTML and rename Max Freq**

In `Source/WebUI/index.html`, change the Max Freq knob label from "Max Freq" to "Hi" (line 78):

```html
              <phantom-knob data-param="synth_max_track_hz" size="medium" label="Hi" default-value="0.5"></phantom-knob>
```

Add the new Lo knob immediately before it:

```html
              <phantom-knob data-param="synth_min_freq_hz" size="medium" label="Lo" default-value="0"></phantom-knob>
              <phantom-knob data-param="synth_max_track_hz" size="medium" label="Hi" default-value="0.5"></phantom-knob>
```

- [ ] **Step 8: Update ParameterTests**

In `tests/ParameterTests.cpp`, add after the `PUNCH_AMOUNT` check (line 58):

```cpp
    REQUIRE(has(ParamID::SYNTH_MIN_FREQ_HZ));
```

Update the count from `34u` to `35u` (line 60):

```cpp
    REQUIRE(ids.size() == 35u);
```

- [ ] **Step 9: Add WaveletSynth min-freq test**

In `tests/WaveletSynthTests.cpp`, add at the end of the file:

```cpp
TEST_CASE("WaveletSynth: minFreq rejects crossings below the floor")
{
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setMinFreqHz(100.0f);  // reject anything below 100 Hz
    syn.setTrackingSpeed(0.25f);

    // Feed 50 Hz sine (below the 100 Hz floor) for 1 second
    feedSine(syn, 50.0f, (int)kSR);

    // Crossings at 50 Hz have intervals of ~882 samples, exceeding maxPeriodSamples
    // (= 44100/100 = 441). So all crossings are rejected and the estimate stays at the
    // default 100 Hz.
    REQUIRE(syn.getEstimatedHz() == Catch::Approx(100.0f).margin(5.0f));
}
```

- [ ] **Step 10: Build and run tests**

Run:
```bash
powershell -Command "& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' 'C:\Documents\NEw project\Kaigen Phantom\build\KaigenPhantom.sln' /p:Configuration=Release /p:Platform=x64 /m /v:minimal"
```

Then:
```bash
powershell -Command "& 'C:\Documents\NEw project\Kaigen Phantom\build\tests\Release\KaigenPhantomTests.exe'"
```

Expected: All tests pass (47 test cases).

- [ ] **Step 11: Commit**

```bash
git add Source/Parameters.h Source/Engines/WaveletSynth.h Source/Engines/WaveletSynth.cpp Source/Engines/ZeroCrossingSynth.h Source/Engines/ZeroCrossingSynth.cpp Source/Engines/PhantomEngine.h Source/Engines/PhantomEngine.cpp Source/PluginProcessor.cpp Source/PluginEditor.h Source/PluginEditor.cpp Source/WebUI/index.html tests/ParameterTests.cpp tests/WaveletSynthTests.cpp
git commit -m "feat: add Min Frequency (Lo) knob — user-controllable tracking floor

Replaces the hardcoded 16Hz minimum with a parameter (8–200Hz, default 8Hz).
Both WaveletSynth and ZeroCrossingSynth get the setter. Existing Max Freq
knob renamed to 'Hi' in the UI to pair with the new 'Lo' knob.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

### Task 2: Sub-sample zero-crossing interpolation

**Files:**
- Modify: `Source/Engines/WaveletSynth.cpp`
- Modify: `Source/Engines/ZeroCrossingSynth.cpp`
- Modify: `tests/WaveletSynthTests.cpp`

- [ ] **Step 1: Add sub-sample interpolation to WaveletSynth**

In `Source/Engines/WaveletSynth.cpp`, inside the `if (lastSample <= 0.0f && x > 0.0f)` block (line 180), add the interpolation immediately after the opening brace:

```cpp
    if (lastSample <= 0.0f && x > 0.0f)
    {
        // ── Sub-sample interpolation ────────────────────────────────────
        // Linear interpolation gives the fractional offset (0..1) from the
        // previous sample to the true zero crossing. Correcting the timing
        // accumulators eliminates up to 1 sample of jitter per crossing.
        const float denom    = x - lastSample;
        const float fraction = (std::abs(denom) > 1e-12f) ? (-lastSample / denom) : 0.5f;
        const float correction = 1.0f - fraction;
        samplesSinceLastCrossing -= correction;
        accumulatedSamples       -= correction;
```

Then, where the phase reset currently reads `currentPhase = 0.0f` (line 210), replace with:

```cpp
                    currentPhase = correction * (kTwoPi / safePeriod);
```

Note: `safePeriod` is computed later (line 256). We need the clamped value at reset time. Since `estimatedPeriod` was just updated, use it directly with a local clamp:

```cpp
                if (inputPeak >= kAmplitudeFloor)
                {
                    const float resetPeriod = juce::jlimit(minPeriodSamples, maxPeriodSamples, estimatedPeriod);
                    currentPhase = correction * (kTwoPi / resetPeriod);
                }
```

- [ ] **Step 2: Add sub-sample interpolation to ZeroCrossingSynth**

In `Source/Engines/ZeroCrossingSynth.cpp`, inside the `if (lastSample <= 0.0f && x > 0.0f)` block (line 150), add the interpolation immediately after the opening brace:

```cpp
    if (lastSample <= 0.0f && x > 0.0f)
    {
        // ── Sub-sample interpolation ────────────────────────────────────
        const float denom    = x - lastSample;
        const float fraction = (std::abs(denom) > 1e-12f) ? (-lastSample / denom) : 0.5f;
        const float correction = 1.0f - fraction;
        samplesSinceLastCrossing -= correction;
        accumulatedSamples       -= correction;
```

No phase reset correction needed — ZeroCrossingSynth uses continuous `fundamentalPhase` that is never reset.

- [ ] **Step 3: Add interpolation accuracy test**

In `tests/WaveletSynthTests.cpp`, add:

```cpp
TEST_CASE("WaveletSynth: sub-sample interpolation improves pitch accuracy")
{
    // At 44100 Hz, a 100 Hz signal has period = 441.0 samples exactly.
    // Without interpolation, snapping to integer boundaries introduces
    // up to 1 sample of error per crossing. With interpolation, the
    // fractional offset is accounted for.
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setTrackingSpeed(0.25f);

    feedSine(syn, 100.0f, (int)(2.0 * kSR));  // 2 seconds for stable estimate

    // With interpolation, the estimate should be within ~1 Hz of 100 Hz.
    // (Without interpolation the margin needed to be 5 Hz.)
    REQUIRE(syn.getEstimatedHz() == Catch::Approx(100.0f).margin(1.0f));
}
```

- [ ] **Step 4: Build and run tests**

Run:
```bash
powershell -Command "& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' 'C:\Documents\NEw project\Kaigen Phantom\build\KaigenPhantom.sln' /p:Configuration=Release /p:Platform=x64 /m /v:minimal"
```

Then:
```bash
powershell -Command "& 'C:\Documents\NEw project\Kaigen Phantom\build\tests\Release\KaigenPhantomTests.exe'"
```

Expected: All tests pass (48 test cases).

- [ ] **Step 5: Commit**

```bash
git add Source/Engines/WaveletSynth.cpp Source/Engines/ZeroCrossingSynth.cpp tests/WaveletSynthTests.cpp
git commit -m "feat: sub-sample zero-crossing interpolation

Linear interpolation between lastSample and x gives a fractional crossing
offset. Corrects samplesSinceLastCrossing and accumulatedSamples for
tighter period estimation. WaveletSynth phase reset also accounts for
the fractional overshoot. Applied to both WaveletSynth and ZeroCrossingSynth.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

### Task 3: Add Threshold & Boost (upward expansion)

**Files:**
- Modify: `Source/Parameters.h`
- Modify: `Source/Engines/WaveletSynth.h`
- Modify: `Source/Engines/WaveletSynth.cpp`
- Modify: `Source/Engines/PhantomEngine.h`
- Modify: `Source/Engines/PhantomEngine.cpp`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`
- Modify: `Source/WebUI/index.html`
- Modify: `tests/ParameterTests.cpp`
- Modify: `tests/WaveletSynthTests.cpp`

- [ ] **Step 1: Add parameter IDs and definitions**

In `Source/Parameters.h`, add after `PUNCH_AMOUNT` (line 75):

```cpp
    /** Upward expansion threshold: wavelets above this level get boosted. 0–100%. Default 0 (off). */
    inline constexpr auto SYNTH_BOOST_THRESHOLD = "synth_boost_threshold";
    /** Upward expansion amount: additional gain for wavelets above threshold. 0–200%. Default 0. */
    inline constexpr auto SYNTH_BOOST_AMOUNT    = "synth_boost_amount";
```

Add both to `getAllParameterIDs()` after `PUNCH_AMOUNT`:

```cpp
        ParamID::SYNTH_BOOST_THRESHOLD,
        ParamID::SYNTH_BOOST_AMOUNT,
```

Add parameter definitions in `createParameterLayout()` after the `PUNCH_AMOUNT` block (after line 253):

```cpp
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_BOOST_THRESHOLD, "Boost Threshold",
        NormalisableRange<float>(0.0f, 100.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("%")));
    params.push_back(std::make_unique<APF>(
        ParamID::SYNTH_BOOST_AMOUNT, "Boost Amount",
        NormalisableRange<float>(0.0f, 200.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("%")));
```

- [ ] **Step 2: Add boost to WaveletSynth header**

In `Source/Engines/WaveletSynth.h`, add after `setH1Amplitude` (line 36):

```cpp
    /** Upward expansion threshold [0–1]. Normalised to inputPeak. Default 0 (off). */
    void setBoostThreshold(float thr) noexcept;
    /** Upward expansion amount [0–2]. Additional gain multiplier. Default 0. */
    void setBoostAmount(float amt) noexcept;
```

Add members after `lastGateGain` (line 77):

```cpp
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothBoostThr { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothBoostAmt { 0.0f };
    float lastBoostGain = 1.0f;
```

- [ ] **Step 3: Implement boost setters and DSP in WaveletSynth.cpp**

Add setters after `setH1Amplitude` (after line 86):

```cpp
void WaveletSynth::setBoostThreshold(float thr) noexcept
{
    smoothBoostThr.setTargetValue(juce::jlimit(0.0f, 1.0f, thr));
}

void WaveletSynth::setBoostAmount(float amt) noexcept
{
    smoothBoostAmt.setTargetValue(juce::jlimit(0.0f, 2.0f, amt));
}
```

In `prepare()`, add smoother reset calls alongside the existing ones (after `smoothGate.reset`):

```cpp
    smoothBoostThr.reset(sr, rampSec);
    smoothBoostAmt.reset(sr, rampSec);
```

In `reset()`, add after `lastGateGain = 1.0f`:

```cpp
    lastBoostGain            = 1.0f;
```

In `process()`, advance the boost smoothers alongside the gate smoother. After `const float rawGateVal = smoothGate.getNextValue();` (line 175):

```cpp
    const float rawBoostThr = smoothBoostThr.getNextValue();
    const float rawBoostAmt = smoothBoostAmt.getNextValue();
```

Inside the `crossingsAccum >= skipCount` block, after the gate gain computation (after the closing `}` of the gate `if/else` block), add the boost gain computation:

```cpp
                // ── Upward expansion (Miya-style Threshold & Boost) ─────────
                // If the wavelet peak exceeds the threshold, boost the output.
                // This emphasises transients and strong harmonics.
                if (rawBoostThr <= 0.0f || lastWaveletPeak < rawBoostThr * inputPeak)
                    lastBoostGain = 1.0f;
                else
                    lastBoostGain = 1.0f + rawBoostAmt;
```

In the output path, after `y *= lastGateGain;` (line 299), add:

```cpp
    y *= lastBoostGain;
```

- [ ] **Step 4: Add forwarding setters to PhantomEngine**

In `Source/Engines/PhantomEngine.h`, add after `setPunchAmount` (line 56):

```cpp
    void setBoostThreshold(float thr);     // RESYN only: upward expansion threshold [0–1]
    void setBoostAmount(float amt);        // RESYN only: upward expansion gain [0–2]
```

In `Source/Engines/PhantomEngine.cpp`, add after `setPunchAmount` implementation (after line 175):

```cpp
void PhantomEngine::setBoostThreshold(float thr)
{
    resynL.setBoostThreshold(thr);
    resynR.setBoostThreshold(thr);
}

void PhantomEngine::setBoostAmount(float amt)
{
    resynL.setBoostAmount(amt);
    resynR.setBoostAmount(amt);
}
```

- [ ] **Step 5: Wire parameters in PluginProcessor**

In `Source/PluginProcessor.cpp`, add in `syncParamsToEngine()` after the `setPunchAmount` line (after line 77):

```cpp
    engine.setBoostThreshold(apvts.getRawParameterValue(ParamID::SYNTH_BOOST_THRESHOLD)->load() / 100.0f);
    engine.setBoostAmount   (apvts.getRawParameterValue(ParamID::SYNTH_BOOST_AMOUNT)->load() / 100.0f);
```

- [ ] **Step 6: Add relays and attachments in PluginEditor**

In `Source/PluginEditor.h`, add after `punchAmountRelay` (line 57):

```cpp
    juce::WebSliderRelay synthBoostThresholdRelay   { "synth_boost_threshold" };
    juce::WebSliderRelay synthBoostAmountRelay      { "synth_boost_amount" };
```

In `Source/PluginEditor.cpp`, add both to the `sliderRelays[]` array (after `&self.punchAmountRelay`):

```cpp
        &self.punchAmountRelay,
        &self.synthBoostThresholdRelay, &self.synthBoostAmountRelay
```

Add to `sliderBindings[]` after the `PUNCH_AMOUNT` entry:

```cpp
        { ParamID::SYNTH_BOOST_THRESHOLD,   synthBoostThresholdRelay },
        { ParamID::SYNTH_BOOST_AMOUNT,      synthBoostAmountRelay },
```

- [ ] **Step 7: Add knobs to Punch panel in HTML**

In `Source/WebUI/index.html`, expand the Punch panel's knob-row (lines 117–119). Replace:

```html
          <div class="candy-inner panel-punch">
            <div class="el">Punch <span class="toggle-group"><button class="tog" id="punch-btn">On</button></span></div>
            <div class="knob-row">
              <phantom-knob data-param="punch_amount" size="medium" label="Amount" default-value="1"></phantom-knob>
            </div>
          </div>
```

With:

```html
          <div class="candy-inner panel-punch">
            <div class="el">Punch <span class="toggle-group"><button class="tog" id="punch-btn">On</button></span></div>
            <div class="knob-row">
              <phantom-knob data-param="punch_amount" size="medium" label="Amount" default-value="1"></phantom-knob>
              <phantom-knob data-param="synth_boost_threshold" size="medium" label="Threshold" default-value="0"></phantom-knob>
              <phantom-knob data-param="synth_boost_amount" size="medium" label="Boost" default-value="0"></phantom-knob>
            </div>
          </div>
```

- [ ] **Step 8: Update ParameterTests**

In `tests/ParameterTests.cpp`, add after the `SYNTH_MIN_FREQ_HZ` check:

```cpp
    REQUIRE(has(ParamID::SYNTH_BOOST_THRESHOLD));
    REQUIRE(has(ParamID::SYNTH_BOOST_AMOUNT));
```

Update the count from `35u` to `37u`:

```cpp
    REQUIRE(ids.size() == 37u);
```

- [ ] **Step 9: Add boost behaviour tests**

In `tests/WaveletSynthTests.cpp`, add:

```cpp
TEST_CASE("WaveletSynth: boost increases output RMS when wavelet peak exceeds threshold")
{
    auto measureRMS = [](float boostThr, float boostAmt) -> float {
        WaveletSynth syn;
        syn.prepare(kSR);
        syn.setTrackingSpeed(0.25f);
        syn.setBoostThreshold(boostThr);
        syn.setBoostAmount(boostAmt);

        // Warm up
        feedSine(syn, 200.0f, (int)(0.5 * kSR));

        // Measure
        float sum = 0.0f;
        const int N = (int)(0.5 * kSR);
        const float w = 2.0f * juce::MathConstants<float>::pi * 200.0f / (float)kSR;
        for (int i = 0; i < N; ++i)
        {
            const float y = syn.process(std::sin(w * (float)(i + (int)(0.5 * kSR))));
            sum += y * y;
        }
        return std::sqrt(sum / (float)N);
    };

    const float rmsNoBoost = measureRMS(0.0f, 0.0f);   // boost off
    const float rmsBoosted = measureRMS(0.3f, 1.0f);    // threshold 30%, boost 100% (+1x)

    // Wavelet peak is ~1.0 (full sine), well above 30% threshold.
    // Boost gain = 1.0 + 1.0 = 2.0 → output should be ~2× louder.
    REQUIRE(rmsBoosted > rmsNoBoost * 1.5f);
}

TEST_CASE("WaveletSynth: boost returns to 1.0 during silence")
{
    WaveletSynth syn;
    syn.prepare(kSR);
    syn.setTrackingSpeed(0.25f);
    syn.setBoostThreshold(0.3f);
    syn.setBoostAmount(1.0f);

    // Feed loud signal to establish boost
    feedSine(syn, 200.0f, (int)(0.5 * kSR));

    // Feed silence — lastWaveletPeak decays, boost should return to 1.0
    for (int i = 0; i < (int)(0.6 * kSR); ++i)
        syn.process(0.0f);

    // Feed quiet signal — boost threshold (0.3 * near-zero inputPeak) should be near 0,
    // but the wavelet peak is also near 0, so boost won't fire.
    // The output should be at normal (unboosted) level.
    float rms = 0.0f;
    const int N = (int)(0.1 * kSR);
    const float w = 2.0f * juce::MathConstants<float>::pi * 200.0f / (float)kSR;
    for (int i = 0; i < N; ++i)
    {
        const float y = syn.process(0.01f * std::sin(w * (float)i));
        rms += y * y;
    }
    rms = std::sqrt(rms / (float)N);

    // Output should be small (no boost amplification on near-silence)
    REQUIRE(rms < 0.5f);
}
```

- [ ] **Step 10: Build and run tests**

Run:
```bash
powershell -Command "& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' 'C:\Documents\NEw project\Kaigen Phantom\build\KaigenPhantom.sln' /p:Configuration=Release /p:Platform=x64 /m /v:minimal"
```

Then:
```bash
powershell -Command "& 'C:\Documents\NEw project\Kaigen Phantom\build\tests\Release\KaigenPhantomTests.exe'"
```

Expected: All tests pass (50 test cases).

- [ ] **Step 11: Commit**

```bash
git add Source/Parameters.h Source/Engines/WaveletSynth.h Source/Engines/WaveletSynth.cpp Source/Engines/PhantomEngine.h Source/Engines/PhantomEngine.cpp Source/PluginProcessor.cpp Source/PluginEditor.h Source/PluginEditor.cpp Source/WebUI/index.html tests/ParameterTests.cpp tests/WaveletSynthTests.cpp
git commit -m "feat: add Threshold & Boost — per-wavelet upward expansion

Miya-style dynamics: wavelets whose peak exceeds the threshold get boosted
by the Boost amount (0–200%). Threshold is normalised to inputPeak for
level-independence. Two new knobs in the Punch panel. RESYN mode only.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```
