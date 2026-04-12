# Wavelet Length Control — Design Spec

## Goal

Add a **Length** knob to the Harmonic Engine row that gates WaveletSynth (RESYN mode) output to the first `length × period` of each wavelet, with a cosine fade-out at the end of the active zone. Has no effect at default (1.0 = full length). Only affects RESYN mode.

---

## Scope

- New parameter: `synth_wavelet_length`
- DSP: `WaveletSynth` only (not ZeroCrossingSynth)
- `PhantomEngine`: forwarding setter for `resynL`/`resynR`
- `PluginProcessor`: reads param, calls engine setter
- UI: one new medium knob in Harmonic Engine row

---

## Parameter

| Property | Value |
|---|---|
| ID | `synth_wavelet_length` |
| Range | 0.05–1.0 (normalised), maps to 5%–100% in DSP |
| Default | 1.0 (full wavelet length — current behaviour, no audible change) |
| APVTS range | `NormalisableRange<float>(0.05f, 1.0f)` |
| Label | `""` (dimensionless ratio) |

---

## DSP — WaveletSynth

### New member

```cpp
// waveletLength smoothed (target set by setWaveletLength)
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothLength;
float waveletLength = 1.0f;  // 0.05..1.0 — fraction of period to synthesise
```

`smoothLength` is initialised in `prepare()` with the same 10 ms ramp used by `smoothStep` and `smoothDuty`. Default current+target = 1.0f.

### New setter

```cpp
void WaveletSynth::setWaveletLength(float len) noexcept
{
    smoothLength.setTargetValue(juce::jlimit(0.05f, 1.0f, len));
}
```

### Gate logic in `process()`

After synthesising `y` (the harmonic sum), apply the length gate before returning:

```cpp
const float len = smoothLength.getNextValue();
if (len < 1.0f)
{
    const float gateEnd   = len * kTwoPi;
    const float fadeStart = gateEnd * 0.8f;   // cosine fade occupies last 20% of active zone
    if (currentPhase >= gateEnd)
        y = 0.0f;
    else if (currentPhase >= fadeStart)
    {
        const float t = (currentPhase - fadeStart) / (gateEnd - fadeStart); // 0→1
        y *= 0.5f * (1.0f + std::cos(kPi * t));  // cosine window: 1→0
    }
}
```

The `len < 1.0f` guard skips all gating when at default — zero CPU overhead at the common case.

**Why phase, not sample count:** `currentPhase` is the authoritative per-wavelet clock. It resets to 0 at every valid crossing, so the gate always aligns with wavelet boundaries regardless of pitch change lag in `estimatedPeriod`.

---

## PhantomEngine

Add to `PhantomEngine.h` public setters:
```cpp
void setWaveletLength(float len);
```

Implementation in `PhantomEngine.cpp`:
```cpp
void PhantomEngine::setWaveletLength(float len)
{
    resynL.setWaveletLength(len);
    resynR.setWaveletLength(len);
    // synthL / synthR intentionally not forwarded — length is RESYN-only
}
```

---

## PluginProcessor

In `syncParamsToEngine()`, add:
```cpp
engine.setWaveletLength(
    apvts.getRawParameterValue(ParamID::SYNTH_WAVELET_LENGTH)->load());
```

---

## Parameters.h

Add to `ParamID`:
```cpp
inline constexpr auto SYNTH_WAVELET_LENGTH = "synth_wavelet_length";
```

Add to `getAllParameterIDs()`:
```cpp
ParamID::SYNTH_WAVELET_LENGTH,
```

Add to `createParameterLayout()`:
```cpp
params.push_back(std::make_unique<APF>(
    ParamID::SYNTH_WAVELET_LENGTH, "Wavelet Length",
    NormalisableRange<float>(0.05f, 1.0f), 1.0f));
```

---

## UI — index.html

Add one medium knob to the Harmonic Engine `knob-row`, after the Push knob and before Skip:

```html
<phantom-knob data-param="synth_wavelet_length" size="medium" label="Length" default-value="1"></phantom-knob>
```

No CSS changes needed. The Harmonic Engine panel flex ratio (1.2) accommodates the additional knob.

---

## Testing

1. **Default (Length=1.0):** RESYN mode output identical to before — no gating audible
2. **Length=0.5:** Each wavelet sounds for half its period, then silence; rhythm matches input pitch
3. **Length=0.05:** Extremely short click-like bursts at pitch frequency
4. **Cosine fade:** No clicks at any length setting; boundary is smooth
5. **Effect mode:** Length knob has no effect on ZCS output — confirmed by checking `synthL`/`synthR` are not forwarded
6. **Smoothing:** Sweeping Length while audio plays has no zipper noise
7. **Unit tests:** `WaveletSynth` tests verify gated output is zero beyond `length × period` and non-zero before it

---

## Out of Scope

- ZeroCrossingSynth gating
- Mode-conditional UI visibility
- Fade width as a user-configurable parameter
