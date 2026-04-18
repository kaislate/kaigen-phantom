# Wavelet Length + Gate Controls — Design Spec

## Goal

Add two knobs to the Harmonic Engine row for RESYN mode:

1. **Length** — gates WaveletSynth output to the first `length × period` of each wavelet, with a cosine fade-out. Has no effect at default (1.0).
2. **Gate** — hysteresis threshold on zero-crossing detection. A crossing only triggers resynthesis if the signal previously swung past `±threshold`. Has no effect at default (0.0).

Both only affect RESYN mode (WaveletSynth). Both are transparent at their defaults — no behaviour change on existing sessions.

---

## Scope

- Two new parameters: `synth_wavelet_length`, `synth_gate_threshold`
- DSP: `WaveletSynth` only (not ZeroCrossingSynth)
- `PhantomEngine`: two new forwarding setters for `resynL`/`resynR`
- `PluginProcessor`: reads both params in `syncParamsToEngine()`
- UI: two new medium knobs in Harmonic Engine row (Length after Push, Gate after Length)

---

## Parameters

### Length

| Property | Value |
|---|---|
| ID | `synth_wavelet_length` |
| Range | `NormalisableRange<float>(0.05f, 1.0f)` |
| Default | `1.0f` (full wavelet — no audible change) |
| Display label | `"Length"` |

### Gate

| Property | Value |
|---|---|
| ID | `synth_gate_threshold` |
| Range | `NormalisableRange<float>(0.0f, 1.0f)` |
| Default | `0.0f` (no threshold — all crossings count, same as current) |
| Display label | `"Gate"` |

---

## DSP — WaveletSynth

### New members

```cpp
// Length
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothLength;

// Gate
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothGate;
float lastNegativePeak = 0.0f;  // most negative value since last valid crossing
```

Both smoothers initialised in `prepare()` with the same 10 ms ramp as `smoothStep`/`smoothDuty`:
```cpp
smoothLength.reset(sr, rampSec);  smoothLength.setCurrentAndTargetValue(1.0f);
smoothGate.reset(sr, rampSec);    smoothGate.setCurrentAndTargetValue(0.0f);
```

`lastNegativePeak` reset to `0.0f` in `reset()`.

### New setters

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

### Changes to `process()`

**1. Track negative peak** — add at the start of `process()`, before zero-crossing detection:

```cpp
if (x < lastNegativePeak)
    lastNegativePeak = x;
```

**2. Gate threshold check** — add to the valid-crossing branch, alongside the existing timing check. The full valid-crossing condition becomes:

```cpp
const float gateThr = smoothGate.getNextValue();  // read once per sample at top of process()

// ... existing samplesSinceLastCrossing tracking ...

if (lastSample <= 0.0f && x > 0.0f)
{
    if (samplesSinceLastCrossing >= minPeriodSamples &&
        samplesSinceLastCrossing <= maxPeriodSamples &&
        lastNegativePeak <= -gateThr)              // ← new gate condition
    {
        // valid crossing — existing period update + phase reset logic unchanged
        // ...
        lastNegativePeak = 0.0f;  // ← reset peak tracker after valid crossing
        samplesSinceLastCrossing = 0.0f;
    }
    else
    {
        // invalid crossing — existing reset logic unchanged
        accumulatedSamples       = 0.0f;
        crossingsAccum           = 0;
        samplesSinceLastCrossing = 0.0f;
        // do NOT reset lastNegativePeak here — keep accumulating
    }
}
```

At `gateThr = 0.0f`: `lastNegativePeak <= 0.0f` is always true (any negative excursion qualifies). Identical to current behaviour.

**3. Length gate** — after synthesising `y` (the harmonic sum), read the length smoother and apply:

```cpp
const float len = smoothLength.getNextValue();
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
```

`len < 1.0f` guard: zero CPU overhead at default.

**Note on smoother call order:** `smoothGate.getNextValue()` must be called exactly once per sample. Call it at the top of `process()` before the zero-crossing block so all downstream logic uses the same stepped value.

---

## PhantomEngine

Add to `PhantomEngine.h` public setters:
```cpp
void setWaveletLength(float len);
void setGateThreshold(float thr);
```

`PhantomEngine.cpp`:
```cpp
void PhantomEngine::setWaveletLength(float len)
{
    resynL.setWaveletLength(len);
    resynR.setWaveletLength(len);
    // synthL/synthR not forwarded — RESYN only
}

void PhantomEngine::setGateThreshold(float thr)
{
    resynL.setGateThreshold(thr);
    resynR.setGateThreshold(thr);
    // synthL/synthR not forwarded — RESYN only
}
```

---

## PluginProcessor

In `syncParamsToEngine()`, add:
```cpp
engine.setWaveletLength(
    apvts.getRawParameterValue(ParamID::SYNTH_WAVELET_LENGTH)->load());
engine.setGateThreshold(
    apvts.getRawParameterValue(ParamID::SYNTH_GATE_THRESHOLD)->load());
```

---

## Parameters.h

Add to `ParamID`:
```cpp
inline constexpr auto SYNTH_WAVELET_LENGTH  = "synth_wavelet_length";
inline constexpr auto SYNTH_GATE_THRESHOLD  = "synth_gate_threshold";
```

Add to `getAllParameterIDs()`:
```cpp
ParamID::SYNTH_WAVELET_LENGTH,
ParamID::SYNTH_GATE_THRESHOLD,
```

Add to `createParameterLayout()`:
```cpp
params.push_back(std::make_unique<APF>(
    ParamID::SYNTH_WAVELET_LENGTH, "Wavelet Length",
    NormalisableRange<float>(0.05f, 1.0f), 1.0f));
params.push_back(std::make_unique<APF>(
    ParamID::SYNTH_GATE_THRESHOLD, "Gate Threshold",
    NormalisableRange<float>(0.0f, 1.0f), 0.0f));
```

---

## UI — index.html

Add two medium knobs to the Harmonic Engine `knob-row`, after Push and before Skip:

```html
<phantom-knob data-param="synth_wavelet_length" size="medium" label="Length" default-value="1"></phantom-knob>
<phantom-knob data-param="synth_gate_threshold" size="medium" label="Gate"   default-value="0"></phantom-knob>
```

No CSS changes needed. The Harmonic Engine panel flex ratio (1.2) accommodates the additional knobs.

---

## Testing

1. **Default (Length=1.0, Gate=0.0):** RESYN output identical to before — no audible change
2. **Length=0.5:** Each wavelet audible for half its period, then silence; gating rhythm matches input pitch
3. **Length=0.05:** Very short burst per crossing — click-like pitched transients
4. **Cosine fade:** No clicks at any Length setting
5. **Gate=0.5:** Quiet signals below 50% amplitude produce no resynthesis; loud signals pass through normally
6. **Gate=0.0:** All crossings trigger as before (identical to pre-feature behaviour)
7. **Effect mode:** Neither knob affects ZCS output
8. **Smoothing:** Sweeping either knob during playback produces no zipper noise
9. **Unit tests:** `WaveletSynth` tests verify: (a) output zero beyond `length × period`; (b) crossing suppressed when `lastNegativePeak > -threshold`

---

## Out of Scope

- ZeroCrossingSynth length/gate
- Mode-conditional UI visibility
- Fade width as a user-configurable parameter
