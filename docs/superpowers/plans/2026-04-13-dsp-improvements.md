# DSP Improvements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Four DSP improvements — glitch-free skip automation, harmonic soft-clipping, per-harmonic anti-aliasing, and a sidechain envelope input — covering M1–M4 from the spec.

**Architecture:** M1–M4 touch the two synth engines (`WaveletSynth`, `ZeroCrossingSynth`) and the `PhantomEngine` layer. M3 (sidechain) also adds a parameter and updates `PluginProcessor` and the Settings UI. All changes are self-contained within the DSP engine — no changes to the oscilloscope, spectrum, or recipe wheel.

**Tech Stack:** JUCE 7, C++17, `juce::AudioProcessorValueTreeState`, `AudioBuffer<float>`

---

## Task 1 — M4: Skip Count Glitch Fix

**Files:**
- Modify: `Source/Engines/WaveletSynth.cpp`
- Modify: `Source/Engines/ZeroCrossingSynth.cpp`

Changing `synth_skip` mid-note resets `accumulatedSamples` to zero, causing a period-measurement gap until the next `skipCount` crossings complete — audible as pitch flutter. Fix: scale the accumulated samples proportionally to the new skip count for a warm start.

- [ ] **Step 1: Update WaveletSynth::setSkipCount()**

Open `Source/Engines/WaveletSynth.cpp`. Find `setSkipCount` (around line 70):
```cpp
void WaveletSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(1, 8, n);
    if (newSkip != skipCount)
    {
        skipCount      = newSkip;
        crossingsAccum = 0;       // restart accumulation on skip change
        accumulatedSamples = 0.0f;
    }
}
```

Replace with:
```cpp
void WaveletSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(1, 8, n);
    if (newSkip != skipCount)
    {
        // Scale accumulated samples proportionally so the EMA has a warm start.
        // e.g. going from skip=1 to skip=2: double the accumulation so the
        // first new measurement lands near the current estimate.
        if (accumulatedSamples > 0.0f)
            accumulatedSamples *= (float)newSkip / (float)skipCount;
        skipCount      = newSkip;
        crossingsAccum = 0;
        // estimatedPeriod is intentionally NOT reset
    }
}
```

- [ ] **Step 2: Update ZeroCrossingSynth::setSkipCount()**

Open `Source/Engines/ZeroCrossingSynth.cpp`. Find `setSkipCount` (around line 55):
```cpp
void ZeroCrossingSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(1, 8, n);
    if (newSkip != skipCount)
    {
        skipCount      = newSkip;
        crossingsAccum = 0;       // restart accumulation on skip change
        accumulatedSamples = 0.0f;
    }
}
```

Replace with:
```cpp
void ZeroCrossingSynth::setSkipCount(int n) noexcept
{
    const int newSkip = juce::jlimit(1, 8, n);
    if (newSkip != skipCount)
    {
        // Scale accumulated samples proportionally so the EMA has a warm start.
        if (accumulatedSamples > 0.0f)
            accumulatedSamples *= (float)newSkip / (float)skipCount;
        skipCount      = newSkip;
        crossingsAccum = 0;
        // estimatedPeriod is intentionally NOT reset
    }
}
```

- [ ] **Step 3: Build and verify**

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" "C:\Documents\NEw project\Kaigen Phantom\build\Kaigen Phantom.sln" /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```
Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 4: Manual verification**

Load plugin with a sustained bass note. Automate or quickly change the Skip knob from 1 → 2 → 3 mid-note. There should be no audible pitch flutter or click at the transition. The fundamental pitch of the harmonics may shift (expected — different sub-harmonic mode) but smoothly.

- [ ] **Step 5: Commit**

```bash
git add "Source/Engines/WaveletSynth.cpp" "Source/Engines/ZeroCrossingSynth.cpp"
git commit -m "fix: skip count change preserves accumulated samples for glitch-free automation (M4)"
```

---

## Task 2 — M2: Harmonic Sum Soft-Clip

**Files:**
- Modify: `Source/Engines/WaveletSynth.cpp`
- Modify: `Source/Engines/ZeroCrossingSynth.cpp`

`WaveletSynth` outputs H1 at amplitude 1.0 unconditionally and adds H2–H8 on top. With Dense preset all 7 harmonics at 0.7, the theoretical peak is ±5.9 — at high Strength settings this hard-clips the output. Apply tanh soft-saturation after the harmonic sum to compress large sums gracefully.

The formula `tanh(y / kRef) / tanh(1/kRef) * kRef` normalises so that amplitude 1.0 passes through unchanged. With `kRef = 1.5`:
- y = 1.0 → out ≈ 1.0 (transparent)
- y = 3.0 → out ≈ 2.5 (gentle compression)
- y = 6.0 → out ≈ 3.0 (strong compression — Dense preset becomes usable)

- [ ] **Step 1: Add soft-clip to WaveletSynth::process()**

Open `Source/Engines/WaveletSynth.cpp`. Find the harmonic sum loop (around line 213) and the length gate block that follows it. The section looks like:

```cpp
    float y = shapedWave(warpPhase(currentPhase, duty), step);

    // H2-H8 additive content from recipe wheel
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;
        float hp = std::fmod((float)(i + 2) * currentPhase, kTwoPi);
        y += amps[(size_t)i] * shapedWave(warpPhase(hp, duty), step);
    }

    // ── Length gate: silence output after len×2π of each wavelet ────────
```

Insert the soft-clip block between the harmonic sum and the length gate comment:

```cpp
    float y = shapedWave(warpPhase(currentPhase, duty), step);

    // H2-H8 additive content from recipe wheel
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;
        float hp = std::fmod((float)(i + 2) * currentPhase, kTwoPi);
        y += amps[(size_t)i] * shapedWave(warpPhase(hp, duty), step);
    }

    // Soft-clip the harmonic sum to prevent hard clipping on dense recipes.
    // tanh(y / kClipRef) / tanh(1/kClipRef) normalises so amplitude 1.0 passes
    // through unchanged while gently compressing larger sums.
    {
        static const float kClipRef  = 1.5f;
        static const float kClipNorm = std::tanh(1.0f / kClipRef);
        y = std::tanh(y / kClipRef) / kClipNorm;
    }

    // ── Length gate: silence output after len×2π of each wavelet ────────
```

- [ ] **Step 2: Add soft-clip to ZeroCrossingSynth::process()**

Open `Source/Engines/ZeroCrossingSynth.cpp`. Find the harmonic sum loop (around line 188) and the `return y` at the end:

```cpp
    float y = 0.0f;
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;

        float hp = std::fmod((float)(i + 2) * fundamentalPhase, kTwoPi);
        y += amps[(size_t)i] * shapedWave(warpPhase(hp, duty), step);
    }

    return y;
```

Insert the soft-clip before `return y`:

```cpp
    float y = 0.0f;
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;

        float hp = std::fmod((float)(i + 2) * fundamentalPhase, kTwoPi);
        y += amps[(size_t)i] * shapedWave(warpPhase(hp, duty), step);
    }

    // Soft-clip harmonic sum — prevents hard clipping on dense recipes.
    // Amplitude 1.0 passes through unchanged; larger sums are gently compressed.
    {
        static const float kClipRef  = 1.5f;
        static const float kClipNorm = std::tanh(1.0f / kClipRef);
        y = std::tanh(y / kClipRef) / kClipNorm;
    }

    return y;
```

- [ ] **Step 3: Build and verify**

Run MSBuild. Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 4: Manual verification**

Load plugin with Dense preset (all harmonics at 0.7). Set Strength to maximum. The output should remain clean — no hard-clipping artifacts. Compare against a sine-only single harmonic (Warm preset) — both should be comparable in perceived loudness, not drastically different.

- [ ] **Step 5: Commit**

```bash
git add "Source/Engines/WaveletSynth.cpp" "Source/Engines/ZeroCrossingSynth.cpp"
git commit -m "feat: harmonic sum tanh soft-clip — prevents clipping on dense recipes (M2)"
```

---

## Task 3 — M1: Per-Harmonic Anti-Aliasing

**Files:**
- Modify: `Source/Engines/WaveletSynth.cpp`
- Modify: `Source/Engines/ZeroCrossingSynth.cpp`

High harmonics can approach Nyquist when the fundamental is in the upper bass range or at non-standard sample rates. Add a per-harmonic fade factor that silently rolls off harmonics in the octave below Nyquist. This is transparent at 44.1 kHz under normal use (fade starts at ~11 kHz) and only engages near Nyquist.

The formula: `aaMul = clamp((nyquist - harmonicHz) / (nyquist - fadeStartHz), 0, 1)` where `fadeStartHz = nyquist * 0.5`.

- [ ] **Step 1: Compute Nyquist once at prepare() in WaveletSynth**

These will be computed inline in the loop using `sampleRate` (already a member). No new member variables needed — `sampleRate` is `double`, compute `float` locals in the loop.

- [ ] **Step 2: Add anti-aliasing to WaveletSynth::process() harmonic loop**

Open `Source/Engines/WaveletSynth.cpp`. Find the H2–H8 loop (around line 216):
```cpp
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;
        float hp = std::fmod((float)(i + 2) * currentPhase, kTwoPi);
        y += amps[(size_t)i] * shapedWave(warpPhase(hp, duty), step);
    }
```

Replace with:
```cpp
    const float nyquist      = (float)(sampleRate * 0.5);
    const float aaFadeStart  = nyquist * 0.5f;   // fade starts 1 octave below Nyquist
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;
        const float harmonicHz = (float)(i + 2) / estimatedPeriod * (float)sampleRate;
        const float aaMul      = juce::jlimit(0.0f, 1.0f,
                                   (nyquist - harmonicHz) / (nyquist - aaFadeStart));
        if (aaMul <= 0.0f) continue;
        float hp = std::fmod((float)(i + 2) * currentPhase, kTwoPi);
        y += amps[(size_t)i] * aaMul * shapedWave(warpPhase(hp, duty), step);
    }
```

- [ ] **Step 3: Add anti-aliasing to ZeroCrossingSynth::process() harmonic loop**

Open `Source/Engines/ZeroCrossingSynth.cpp`. Find the H2–H8 loop (around line 188):
```cpp
    float y = 0.0f;
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;

        float hp = std::fmod((float)(i + 2) * fundamentalPhase, kTwoPi);
        y += amps[(size_t)i] * shapedWave(warpPhase(hp, duty), step);
    }
```

Replace with:
```cpp
    float y = 0.0f;
    const float nyquist     = (float)(sampleRate * 0.5);
    const float aaFadeStart = nyquist * 0.5f;   // fade starts 1 octave below Nyquist
    for (int i = 0; i < 7; ++i)
    {
        if (amps[(size_t)i] <= 0.0f) continue;

        const float harmonicHz = (float)(i + 2) / estimatedPeriod * (float)sampleRate;
        const float aaMul      = juce::jlimit(0.0f, 1.0f,
                                   (nyquist - harmonicHz) / (nyquist - aaFadeStart));
        if (aaMul <= 0.0f) continue;
        float hp = std::fmod((float)(i + 2) * fundamentalPhase, kTwoPi);
        y += amps[(size_t)i] * aaMul * shapedWave(warpPhase(hp, duty), step);
    }
```

- [ ] **Step 4: Build and verify**

Run MSBuild. Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 5: Manual verification**

At 44.1 kHz: play a low bass note (e.g., 60 Hz). All harmonics H2–H8 land at 120–480 Hz — all well below fade start (~11 kHz). Output should sound identical to before this change. For a higher note (e.g., 300 Hz), H8 = 2400 Hz — still transparent. Only at very high fundamentals (>1.4 kHz) would the fade engage.

- [ ] **Step 6: Commit**

```bash
git add "Source/Engines/WaveletSynth.cpp" "Source/Engines/ZeroCrossingSynth.cpp"
git commit -m "feat: per-harmonic anti-aliasing fade below Nyquist in both synth engines (M1)"
```

---

## Task 4 — M3: Sidechain Envelope Input

**Files:**
- Modify: `Source/Parameters.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/Engines/PhantomEngine.h`
- Modify: `Source/Engines/PhantomEngine.cpp`
- Modify: `Source/WebUI/index.html`

The plugin already declares a sidechain input bus (see `PluginProcessor.cpp` constructor). This task wires it up: when `env_source = 1`, the envelope follower reads amplitude from the sidechain bus instead of the main input bass band.

### Step-by-step

- [ ] **Step 1: Add ENV_SOURCE parameter to Parameters.h**

In `Parameters.h`, add to the `ParamID` namespace after `ENV_RELEASE_MS`:
```cpp
    inline constexpr auto ENV_SOURCE           = "env_source";
```

In `getAllParameterIDs()`, add `ParamID::ENV_SOURCE` to the vector after `ParamID::ENV_RELEASE_MS`:
```cpp
        ParamID::ENV_SOURCE,
```

In `createParameterLayout()`, after the `env_release_ms` push_back (around line 178), add:
```cpp
    params.push_back(std::make_unique<APC>(
        ParamID::ENV_SOURCE, "Envelope Source",
        StringArray{ "Input", "Sidechain" }, 0));
```

- [ ] **Step 2: Add envSource member and setter to PhantomEngine.h**

Open `Source/Engines/PhantomEngine.h`. Find the private member section. Add after `float stereoWidth`:
```cpp
    int envSource = 0;  // 0 = main input bass band, 1 = sidechain
```

In the public setter declarations section, add:
```cpp
    void setEnvSource(int s);
```

- [ ] **Step 3: Add setEnvSource implementation to PhantomEngine.cpp**

In `Source/Engines/PhantomEngine.cpp`, in the parameter setters section (after `setStereoWidth`), add:
```cpp
void PhantomEngine::setEnvSource(int s)   { envSource = juce::jlimit(0, 1, s); }
```

- [ ] **Step 4: Update PhantomEngine::process() to use sidechain envelope**

In `Source/Engines/PhantomEngine.cpp`, find the `process()` method signature:
```cpp
void PhantomEngine::process(juce::AudioBuffer<float>& buffer)
```

Change it to accept an optional sidechain buffer:
```cpp
void PhantomEngine::process(juce::AudioBuffer<float>& buffer,
                            const juce::AudioBuffer<float>* sidechain)
```

Then in the per-sample loop, find:
```cpp
            const float inLvl = env.process(low[i]);
```

Replace with:
```cpp
            // Envelope source: main input bass band (default) or sidechain ch0
            const float envIn = (envSource == 1 && sidechain != nullptr && ch < sidechain->getNumChannels())
                ? sidechain->getReadPointer(ch)[i]
                : low[i];
            const float inLvl = env.process(envIn);
```

- [ ] **Step 5: Update PhantomEngine.h process() signature**

In `Source/Engines/PhantomEngine.h`, update the `process` declaration to match:
```cpp
    void process(juce::AudioBuffer<float>& buffer,
                 const juce::AudioBuffer<float>* sidechain = nullptr);
```

- [ ] **Step 6: Update syncParamsToEngine() in PluginProcessor.cpp**

In `Source/PluginProcessor.cpp`, in `syncParamsToEngine()`, add after `engine.setEnvelopeReleaseMs`:
```cpp
    engine.setEnvSource((int) apvts.getRawParameterValue(ParamID::ENV_SOURCE)->load());
```

- [ ] **Step 7: Update processBlock() to pass sidechain buffer**

In `Source/PluginProcessor.cpp`, find the `engine.process(buffer)` call (around line 183):
```cpp
    engine.process(buffer);
```

Replace with:
```cpp
    // Read sidechain bus if present
    const juce::AudioBuffer<float>* sidechainPtr = nullptr;
    juce::AudioBuffer<float> scBuf;
    auto* sidechainInput = getBus(true, 1);  // bus index 1 = sidechain
    if (sidechainInput && sidechainInput->isEnabled())
    {
        scBuf = sidechainInput->getBusBuffer(buffer);
        sidechainPtr = &scBuf;
    }
    engine.process(buffer, sidechainPtr);
```

- [ ] **Step 8: Add ENV SOURCE row to Settings overlay in index.html**

In `Source/WebUI/index.html`, find the `settings-content` div (around line 149). After the BINAURAL section's closing `</div>`, add a new section:
```html
      <div class="settings-section">
        <div class="el">ENVELOPE SOURCE</div>
        <div class="settings-row">
          <select id="env-source-select" class="param-select" data-param="env_source">
            <option value="0">Input</option>
            <option value="1">Sidechain</option>
          </select>
        </div>
      </div>
```

- [ ] **Step 9: Wire the env-source select in phantom.js**

In `Source/WebUI/phantom.js`, find the binaural select wiring (around line 286). After its closing `}`, add:
```javascript
// =============================================================================
// 10. Wire envelope source select
// =============================================================================

const envSourceSelect = document.getElementById("env-source-select");
if (envSourceSelect) {
  const envSourceState = getComboBoxState?.("env_source");
  if (envSourceState) {
    envSourceSelect.addEventListener("change", () => {
      envSourceState.setChoiceIndex(parseInt(envSourceSelect.value));
    });
    envSourceState.valueChangedEvent?.addListener(() => {
      envSourceSelect.value = String(envSourceState.getChoiceIndex());
    });
    // Initialize
    envSourceSelect.value = String(envSourceState.getChoiceIndex());
  }
}
```

- [ ] **Step 10: Build and verify**

Run MSBuild. Expected: `Build succeeded. 0 Error(s)`

- [ ] **Step 11: Manual verification**

Load plugin. Open Settings — "ENVELOPE SOURCE" section should appear below BINAURAL with Input/Sidechain dropdown, default Input. Load a kick drum on the sidechain input of the plugin (in a DAW that supports sidechaining — Ableton, Logic, etc.). Play a bass note on the main input. Switch Envelope Source to "Sidechain" — the phantom harmonic amplitude should now follow the kick's envelope rather than the bass note's envelope, creating a rhythmic gating effect.

If the DAW has no sidechain signal, Sidechain mode should produce silence (no envelope = no phantom output). Input mode should work exactly as before.

- [ ] **Step 12: Commit**

```bash
git add "Source/Parameters.h" "Source/PluginProcessor.cpp" \
        "Source/Engines/PhantomEngine.h" "Source/Engines/PhantomEngine.cpp" \
        "Source/WebUI/index.html" "Source/WebUI/phantom.js"
git commit -m "feat: sidechain envelope input — drive phantom amplitude from external signal (M3)"
```
