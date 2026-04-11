# Phantom DSP Rewrite — Waveshaper Architecture

**Date:** 2026-04-11
**Status:** Approved for immediate implementation
**Replaces:** The pitch-tracking + harmonic synthesis architecture entirely

## Motivation

The existing DSP architecture (pitch tracker + sinewave voice synthesis) has fundamental limitations:

- **Poor sub-100Hz tracking** — YIN requires long windows (~50-100ms) for low frequencies, causing latency and unreliable detection
- **Stuck harmonics** — synthesized voices don't naturally silence when input stops
- **No amplitude following** — voices render at fixed amplitude, not input envelope
- **Chord confusion** — pitch trackers pick one pitch arbitrarily on polyphonic material
- **Not snappy** — inherent analysis latency

Commercial phantom bass plugins (Waves RBass, MaxxBass, Renaissance Bass, Sonnox Oxford Bass Enhancer, Schepps Bass Rider) don't use pitch tracking at all. They use **nonlinear waveshaping** — distorting the bass band with a carefully-shaped nonlinearity to create harmonics naturally.

## New Architecture

```
Input
  │
  ├──────────────────────────────────────────────► Dry path
  │                                                    │
  ▼                                                    │
BassExtractor (LR4 crossover @ phantom_threshold)      │
  │                                                    │
  ├── Low band ───► Waveshaper ───► HarmonicShaper     │
  │                 (drive + curve)  (per-H2-H8 EQ)    │
  │                                       │            │
  │                                       ▼            │
  │                                  Envelope gate     │
  │                                  (input RMS)       │
  │                                       │            │
  │                                       ▼            │
  │                                  Phantom signal    │
  │                                       │            │
  │                                       ▼            │
  │                                 BinauralStage      │
  │                                       │            │
  │                                       ▼            │
  │                                 Ghost Mix ◄────────┘
  │                                 (Replace / Add)
  │                                       │
  │                                       ▼
  │                                  Output Gain
  │                                       │
  │                                       ▼
  └──────────────────────────────────── Output
```

### Why this works

1. **Frequency-agnostic** — waveshaper produces harmonics for ANY input frequency, including 20Hz and below
2. **Amplitude-tracking for free** — output amplitude is a direct function of input amplitude
3. **Silences when input stops** — no internal state, nothing to sustain
4. **Handles polyphony** — every frequency present in the input gets its own harmonics
5. **Sample-accurate** — no analysis windows, zero tracking latency
6. **Simple** — ~300 lines of DSP vs ~1500 currently

## DSP Components

### 1. BassExtractor

Linkwitz-Riley 4th-order crossover at `phantom_threshold` (20-150 Hz, default 80 Hz). Splits input into:
- **lowBand** — isolated bass content to feed the waveshaper
- **highBand** — untouched upper content that stays in the dry path

LR4 = two cascaded Butterworth 2nd-order filters. Provides flat sum (linear-phase when both bands are recombined) and 24 dB/oct slopes.

**Interface:**
```cpp
class BassExtractor {
public:
    void prepare(double sampleRate, int blockSize);
    void setCrossoverHz(float hz);
    // Processes in place: writes lowOut/highOut from input
    void process(const float* in, float* lowOut, float* highOut, int n);
private:
    juce::dsp::LinkwitzRileyFilter<float> lpf, hpf;
};
```

### 2. Waveshaper

Nonlinear distortion that generates harmonics. The shape of the nonlinearity determines the harmonic content:

- **tanh** — even + odd harmonics, smooth (warm)
- **soft-clip (x - x³/3)** — mostly odd harmonics (hollow)
- **asymmetric (half-wave + tanh)** — strong 2nd harmonic emphasis (aggressive)
- **polynomial sum** — custom curve from Chebyshev polynomials for precise harmonic control

We use the **Chebyshev polynomial approach** for direct control:

For input `x ∈ [-1, 1]`, the Chebyshev polynomial `T_n(cos(θ)) = cos(nθ)` means if you feed a sine wave through `T_n`, you get a cosine wave at `n × frequency`. So summing `a_n · T_n(x)` gives you direct per-harmonic amplitude control.

We use T2-T8 (for H2-H8 since H1 = fundamental = T1):

```
T2(x) = 2x² - 1
T3(x) = 4x³ - 3x
T4(x) = 8x⁴ - 8x² + 1
T5(x) = 16x⁵ - 20x³ + 5x
T6(x) = 32x⁶ - 48x⁴ + 18x² - 1
T7(x) = 64x⁷ - 112x⁵ + 56x³ - 7x
T8(x) = 128x⁸ - 256x⁶ + 160x⁴ - 32x² + 1
```

Output: `y = Σ (a_n · T_n(x_drive)) for n in [2..8]`
where `x_drive = x * (1 + drive × 4)` clamped to [-1, 1].

**Interface:**
```cpp
class Waveshaper {
public:
    void setHarmonicAmplitudes(const std::array<float, 7>& amps); // H2..H8 [0..1]
    void setDrive(float drive);                                   // [0..1]
    void setSaturation(float sat);                                // [0..1] adds tanh blend
    void process(const float* in, float* out, int n);
private:
    std::array<float, 7> amps { 0.8f, 0.7f, 0.5f, 0.35f, 0.2f, 0.12f, 0.07f };
    float drive = 0.5f;
    float saturation = 0.0f;
};
```

### 3. HarmonicShaper (optional phase — may skip initially)

Post-waveshaper bandpass filter bank that refines the spectral envelope of the harmonics. Useful if the waveshaper's natural output needs additional shaping. **For v1 we skip this** — the Chebyshev approach already gives direct per-harmonic control. Can add later if needed.

### 4. EnvelopeFollower

Tracks input RMS with fast attack and medium release. The phantom output is multiplied by this envelope so it always follows the input loudness.

```cpp
class EnvelopeFollower {
public:
    void prepare(double sampleRate);
    void setAttackMs(float ms);       // default 1ms
    void setReleaseMs(float ms);      // default 50ms
    float process(float input);       // returns smoothed envelope [0..1]
private:
    float attackCoef = 0, releaseCoef = 0;
    float env = 0.0f;
};
```

Formula:
- target = |input|
- if target > env: env += (target - env) × attackCoef
- else: env += (target - env) × releaseCoef

This gives sample-accurate envelope following with no lookahead latency.

### 5. PhantomEngine

The top-level DSP unit that ties it all together.

```cpp
class PhantomEngine {
public:
    void prepare(double sr, int blockSize, int numChannels);
    void reset();

    // Parameter setters (called once per block from APVTS)
    void setCrossoverHz(float hz);
    void setHarmonicAmplitudes(const std::array<float, 7>& amps);
    void setDrive(float drive);
    void setSaturation(float sat);
    void setGhostAmount(float amt);      // wet/dry [0..1]
    void setGhostMode(bool replace);     // true=replace, false=add
    void setPhantomStrength(float s);    // post-waveshaper gain [0..1]
    void setOutputGainDb(float db);

    // Audio processing
    void process(juce::AudioBuffer<float>& buffer);

private:
    BassExtractor     bassExtractor;
    Waveshaper        waveshaper;
    EnvelopeFollower  envelope;

    // Working buffers
    juce::AudioBuffer<float> lowBuf;    // bandpass-isolated bass
    juce::AudioBuffer<float> highBuf;   // pass-through upper band
    juce::AudioBuffer<float> phantomBuf;// shaped harmonics

    // Parameters (block-rate)
    float ghostAmount    = 1.0f;
    bool  ghostReplace   = true;
    float phantomStrength = 0.8f;
    float outputGainLin  = 1.0f;
};
```

### Signal flow in process()

```cpp
void PhantomEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();

    // 1. Split input into bass and highs (per channel)
    for (int ch = 0; ch < nCh; ++ch)
    {
        bassExtractor.process(
            buffer.getReadPointer(ch),
            lowBuf.getWritePointer(ch),
            highBuf.getWritePointer(ch),
            n);
    }

    // 2. Waveshape the bass band -> phantomBuf
    for (int ch = 0; ch < nCh; ++ch)
    {
        waveshaper.process(
            lowBuf.getReadPointer(ch),
            phantomBuf.getWritePointer(ch),
            n);
    }

    // 3. Scale by phantom strength
    phantomBuf.applyGain(phantomStrength);

    // 4. Sample-by-sample envelope follow + Ghost mix
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* out = buffer.getWritePointer(ch);
        const auto* low = lowBuf.getReadPointer(ch);
        const auto* high = highBuf.getReadPointer(ch);
        const auto* phantom = phantomBuf.getReadPointer(ch);

        for (int i = 0; i < n; ++i)
        {
            // Envelope from LOW band only (bass energy determines harmonic amplitude)
            const float inLevel = envelope.process(std::abs(low[i]));

            // Envelope-multiplied phantom signal
            const float phantomScaled = phantom[i] * inLevel * 3.0f;

            // Ghost mix: blend dry low with phantom low
            //   Replace: (1-g)*low + g*phantom
            //   Add:     low + g*phantom
            float mixedLow;
            if (ghostReplace)
                mixedLow = low[i] * (1.0f - ghostAmount) + phantomScaled * ghostAmount;
            else
                mixedLow = low[i] + phantomScaled * ghostAmount;

            // Recombine with high band, apply output gain
            out[i] = (mixedLow + high[i]) * outputGainLin;
        }
    }
}
```

## Parameter Changes

### REMOVE (no longer meaningful)

- `tracking_sensitivity` — no pitch tracker
- `tracking_glide` — no pitch tracker
- `deconfliction_mode` — no voices to deconflict
- `max_voices` — no voices
- `stagger_delay` — no voices
- `recipe_phase_h2..h8` — Chebyshev polynomials don't have user-controllable phase (phase is locked to the input signal)
- `recipe_rotation` — not applicable to waveshaper model

### KEEP

- `mode` (Effect/Instrument) — in new architecture, Instrument mode can still use MIDI input to generate a synthesized bass tone that gets waveshaped
- `bypass`
- `ghost` — wet/dry (renamed internally to ghost_amount)
- `ghost_mode` — Replace vs Add
- `phantom_threshold` — crossover frequency
- `phantom_strength` — post-waveshaper gain
- `output_gain`
- `harmonic_saturation` — waveshaper drive
- `recipe_h2..h8` — Chebyshev coefficients (direct harmonic control)
- `recipe_preset` — still selects from Warm/Aggressive/Hollow/Dense/Custom
- `binaural_mode`, `binaural_width` — keep BinauralStage applied to phantom output
- `sidechain_duck_amount/attack/release` — keep for sidechain compression
- `stereo_width` — keep, applied to phantom output

## File Structure

```
Source/
  Engines/
    BassExtractor.h/.cpp       — LR4 crossover
    Waveshaper.h/.cpp          — Chebyshev polynomial waveshaper
    EnvelopeFollower.h/.cpp    — RMS/peak envelope
    PhantomEngine.h/.cpp       — top-level DSP container
    BinauralStage.h/.cpp       — KEPT (already works)

  PluginProcessor.h/.cpp       — REWRITTEN: uses PhantomEngine
  Parameters.h                 — UPDATED: removed tracking/deconfliction params
  PluginEditor.h/.cpp          — minor updates (remove tracking/decon relays)

  WebUI/                       — mostly unchanged, panel labels updated
    index.html                 — remove PitchTracker/Deconfliction panels
    phantom.js                 — remove tracking/decon wiring

Engines REMOVED:
  PitchTracker.h/.cpp
  HarmonicGenerator.h/.cpp
  PerceptualOptimizer.h/.cpp
  CrossoverBlend.h/.cpp
  Deconfliction/*  (all 6 strategies)

tests/
  BassExtractorTests.cpp       — new
  WaveshaperTests.cpp          — new
  EnvelopeFollowerTests.cpp    — new
  PhantomEngineTests.cpp       — new
  ParameterTests.cpp           — updated
  BinauralStageTests.cpp       — kept
  SerializationTests.cpp       — kept

REMOVED tests:
  PitchTrackerTests.cpp
  HarmonicGeneratorTests.cpp
  DeconflictionTests.cpp
  PerceptualOptimizerTests.cpp
  CrossoverBlendTests.cpp
```

## Recipe Presets (new tables)

Presets now define Chebyshev T2-T8 coefficients directly. These were designed by analyzing the harmonic spectra of classic bass enhancer plugins:

```cpp
// Warm — heavy low harmonics, smooth rolloff (RBass-like)
kWarmAmps       = { 0.80, 0.60, 0.40, 0.28, 0.18, 0.10, 0.05 };

// Aggressive — mid-high emphasis, bright (MaxxBass-like aggressive)
kAggressiveAmps = { 0.50, 0.70, 0.85, 0.75, 0.55, 0.35, 0.20 };

// Hollow — odd harmonics only (square-wave-like)
kHollowAmps     = { 0.00, 0.80, 0.00, 0.60, 0.00, 0.40, 0.00 };

// Dense — uniform spread (full saturation)
kDenseAmps      = { 0.70, 0.70, 0.70, 0.70, 0.70, 0.70, 0.70 };

// Custom — user-controlled
```

## Testing Strategy

**BassExtractor:**
- Impulse response has correct LR4 magnitude shape
- Low + high band sum reconstructs the input (within rounding)
- Crossover point at set frequency (±5%)

**Waveshaper:**
- Sine input produces expected harmonic series
- Bypass (amps all zero) preserves DC, zero output
- Full-scale input stays bounded (no blow-up)

**EnvelopeFollower:**
- Step input reaches target within attack time
- Silent input decays within release time
- RMS of noise converges to 1/√2 for unit-amplitude sine

**PhantomEngine:**
- Silent input → silent output (envelope gates it to zero)
- Sine at fundamental produces audible harmonics that cease immediately when input stops
- Ghost Replace at 100% removes low band from output
- Ghost Add at 50% preserves low band at full level plus half-scaled phantom

## Implementation Order

1. Write the spec (this document) — committed first
2. Delete old engine files + update CMakeLists
3. Implement DSP classes in dependency order: EnvelopeFollower → BassExtractor → Waveshaper → PhantomEngine
4. Write tests as each class is built
5. Update Parameters.h, PluginProcessor, PluginEditor
6. Update WebUI (index.html, phantom.js) to remove orphaned panels
7. Full build + test pass
8. Auto-deploy to `%LOCALAPPDATA%/Programs/Common/VST3` and copy to `Downloads/KAIGEN`

## Success Criteria

- Plugin loads and passes audio through in bypass
- With Ghost Amount > 0, audible phantom harmonics appear above the bass band
- Harmonics track input amplitude **sample-accurately** (no tracking latency)
- Harmonics stop **immediately** when input stops (within the envelope release time, ~50ms)
- Works on sub-100Hz content (e.g., 40Hz sine → audible 80, 120, 160 Hz harmonics)
- Works on polyphonic content (multiple bass notes simultaneously → harmonics for each)
- Bypass button works
- All existing WebUI controls update parameters correctly
- All tests pass
