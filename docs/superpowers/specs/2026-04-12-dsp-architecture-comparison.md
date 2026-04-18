# Kaigen Phantom — DSP Architecture Comparison

**Date:** 2026-04-12

## Purpose

This document compares the two DSP architectures used in the Kaigen Phantom plugin: the original pitch-tracking + voice synthesis approach (v1, now retired) and the current Chebyshev waveshaping approach (v2, production).

---

## Overview

| | v1: Pitch Tracker + Synthesis | v2: Chebyshev Waveshaper |
|---|---|---|
| **Core technique** | Detect fundamental → synthesize sine waves at harmonic multiples | Split bass band → apply nonlinear polynomial distortion → recombine |
| **Harmonic generation** | Additive synthesis (7 oscillators per voice) | Chebyshev polynomials T2–T8 applied to input waveform |
| **Pitch tracking** | YIN autocorrelation algorithm | None required |
| **Amplitude tracking** | Manual RMS gate (added late, band-aid fix) | Envelope follower (1ms attack, 50ms release, sample-accurate) |
| **Voice management** | 8-voice polyphonic pool with note stealing | No voices — input signal IS the carrier |
| **Latency** | 50–100ms (analysis window for YIN) | 0ms (sample-by-sample processing) |
| **Lines of DSP code** | ~1,500 across 6 engine files + 6 deconfliction strategies | ~400 across 4 engine files |
| **Test assertions** | 590 (29 test cases) | 2,761 (25 test cases) |

---

## v1: Pitch Tracker + Harmonic Voice Synthesis

### Signal Flow

```
Audio Input
    │
    ▼
PitchTracker (YIN autocorrelation)
    │   detects fundamental Hz
    ▼
HarmonicGenerator (7 sine oscillators per voice)
    │   generates H2–H8 at exact multiples
    ▼
PerceptualOptimizer (A-weighting compensation)
    │
    ▼
BinauralStage (optional L/R decorrelation)
    │
    ▼
CrossoverBlend (mixes dry + phantom below threshold)
    │
    ▼
Output
```

### Engine Files

| File | Lines | Purpose |
|---|---|---|
| `PitchTracker.h/cpp` | ~125 | YIN autocorrelation pitch detector |
| `HarmonicGenerator.h/cpp` | ~250 | 8-voice polyphonic additive synthesizer |
| `PerceptualOptimizer.h/cpp` | ~100 | A-weighting loudness compensation |
| `CrossoverBlend.h/cpp` | ~185 | Crossover filter + wet/dry mix |
| `BinauralStage.h/cpp` | ~80 | Stereo decorrelation |
| `Deconfliction/*` (6 strategies) | ~250 | Voice allocation strategies for polyphony |
| **Total** | **~990** | |

### How It Worked

1. **PitchTracker** runs a YIN autocorrelation algorithm on each process block (~512 samples). It outputs a smoothed fundamental frequency estimate in Hz, or -1 if no pitch is detected.

2. **HarmonicGenerator** maintains a pool of 8 Voice objects. Each voice stores a fundamental frequency, 7 oscillator phases (for H2–H8), and amplitude values from the recipe preset. In Effect mode, a single "effect voice" renders harmonics at the detected pitch. In Instrument mode, MIDI note-on/off events activate/deactivate voices.

3. Each voice renders its harmonics by incrementing 7 independent phase accumulators per sample:
   ```
   for each harmonic n in [2..8]:
       output += amp[n] * sin(phase[n])
       phase[n] += 2π × (fundamental × n) / sampleRate
   ```

4. **PerceptualOptimizer** applies A-weighting compensation to the phantom signal so low-frequency content sounds perceptually correct on different playback systems.

5. **CrossoverBlend** splits the original signal around the phantom threshold using a crossover filter, then mixes the phantom harmonics with the dry signal using the Ghost Amount (wet/dry) and Ghost Mode (Replace/Add) controls.

6. **Deconfliction strategies** (Partition, Lane, Stagger, Odd-Even, Residue, Binaural) handled the Instrument mode case where multiple MIDI notes play simultaneously — each strategy assigns different harmonics to different voices to avoid frequency collisions.

### Strengths

- **Theoretically precise** — synthesized harmonics are at exact integer multiples of the fundamental, producing a mathematically perfect harmonic series.
- **Phase-controllable** — each harmonic could have its own phase offset (the recipe_phase_h2..h8 parameters), allowing fine-tuning of the timbral character.
- **Recipe rotation** — a unique parameter that shifted the emphasis pattern around the harmonic series.
- **Deconfliction** — in Instrument mode, multiple simultaneous notes were handled with sophisticated voice allocation strategies that no commercial plugin offers.

### Weaknesses (why it was replaced)

1. **Sub-100Hz pitch detection is unreliable.**
   YIN autocorrelation needs at least 2 full cycles of the fundamental to produce a confident estimate. At 40Hz, one cycle is 25ms. At typical buffer sizes (512 samples @ 44.1kHz = 11.6ms), the algorithm often doesn't have enough data to resolve fundamentals below ~80Hz. This is a fundamental mathematical limitation of time-domain pitch trackers — not a bug in our implementation.

2. **Tracking latency is inherent.**
   The smoothing filter on the pitch tracker output adds 20-200ms of latency depending on the Glide parameter. This means the phantom harmonics "chase" the input rather than following it instantly. For percussive bass content (kicks, plucks), this chase is audible as a delayed "shimmer" after the attack.

3. **Stuck voices.**
   When input stops abruptly, the pitch tracker's last confident estimate stays latched. The effect voice continues rendering harmonics at that frequency indefinitely because `setEffectModePitch()` was only called when a new pitch was detected — not when detection failed. We patched this with an RMS gate, but that's a band-aid: the voice system conceptually doesn't have a notion of "input stopped, silence the output."

4. **No amplitude tracking.**
   Synthesized harmonics render at fixed amplitudes from the recipe preset, regardless of input loudness. A fortissimo bass note and a pianissimo bass note produce the same phantom level. This sounds unnatural — the ear expects harmonic overtones to scale with the fundamental's loudness.

5. **Chord confusion.**
   When multiple notes are present simultaneously (bass chords, layered parts), the pitch tracker picks the strongest fundamental arbitrarily. The phantom harmonics follow that one pitch while ignoring the others, producing incorrect harmonic relationships for the non-tracked notes.

6. **Complexity overhead.**
   The deconfliction system (6 strategies, voice pools, MIDI dispatching) was complex and only relevant in the rarely-used Instrument mode. The Effect mode — which is what 95% of users would use — didn't benefit from any of this complexity.

---

## v2: Chebyshev Polynomial Waveshaper

### Signal Flow

```
Audio Input
    │
    ├───────────────────────────────────────────► Dry path
    │                                                │
    ▼                                                │
BassExtractor (LR4 crossover @ phantom_threshold)    │
    │                                                │
    ├── Low band ──► Waveshaper (Chebyshev T2–T8)    │
    │                      │                         │
    │                      ▼                         │
    │               EnvelopeFollower                 │
    │               (scales phantom by input level)  │
    │                      │                         │
    │                      ▼                         │
    │               Phantom signal                   │
    │                      │                         │
    │                      ▼                         │
    │               Ghost Mix (Replace / Add) ◄──────┘
    │                      │
    │                      ▼
    │               BinauralStage
    │                      │
    │                      ▼
    │               Stereo Width (M/S)
    │                      │
    │                      ▼
    └── High band ──► Recombine ──► Output Gain ──► Output
```

### Engine Files

| File | Lines | Purpose |
|---|---|---|
| `BassExtractor.h/cpp` | ~65 | LR4 crossover (single-filter dual-output for perfect reconstruction) |
| `Waveshaper.h/cpp` | ~110 | Chebyshev polynomial T2–T8 with drive and tanh saturation blend |
| `EnvelopeFollower.h/cpp` | ~85 | Asymmetric peak detector (1ms attack, 50ms release) |
| `PhantomEngine.h/cpp` | ~140 | Top-level DSP container wiring everything together |
| `BinauralStage.h/cpp` | ~80 | Stereo decorrelation (retained from v1) |
| **Total** | **~480** | |

### How It Works

1. **BassExtractor** uses JUCE's `LinkwitzRileyFilter` in dual-output mode to split the input into a low band (below the crossover frequency) and a high band (above). The dual-output mode guarantees magnitude reconstruction: `low + high = allpass(input)`, preserving the full amplitude of the original signal.

2. **Waveshaper** processes the isolated low band through Chebyshev polynomials. The Chebyshev polynomial T_n has the mathematical property that `T_n(cos θ) = cos(nθ)`. This means if you feed a sine wave at frequency f through T_n, the output is a cosine at frequency n×f. By summing weighted T2 through T8:

   ```
   output = a2·T2(x) + a3·T3(x) + a4·T4(x) + a5·T5(x) + a6·T6(x) + a7·T7(x) + a8·T8(x)
   ```

   ...we get direct, independent control over each harmonic's amplitude. The input `x` is the bass band signal after drive scaling.

   The Chebyshev recurrence relation (`T_{n+1}(x) = 2x·T_n(x) - T_{n-1}(x)`) makes evaluation efficient — only multiplications and additions, no trigonometric functions.

3. **EnvelopeFollower** tracks the input bass band's amplitude in real-time using a one-pole filter with separate attack (1ms default) and release (50ms default) coefficients. The phantom signal is multiplied by this envelope value on every sample, ensuring:
   - Loud input → loud phantom harmonics
   - Quiet input → quiet phantom harmonics
   - Silent input → zero phantom output (within release time)

4. **Ghost mix** combines the phantom signal with the original low band:
   - **Replace mode:** `out = low × (1 - amount) + phantom × amount`
   - **Add mode:** `out = low + phantom × amount`

5. The mixed low band is recombined with the untouched high band, processed through BinauralStage and stereo width, and output.

### Why Chebyshev Polynomials?

Other nonlinear functions could generate harmonics (tanh, polynomial, full-wave rectifier), but Chebyshev polynomials are uniquely suited because:

- **Independent harmonic control.** Each T_n produces ONLY the nth harmonic (plus a DC offset for even-order terms). This means setting `a3 = 0.8, a5 = 0.4` gives you exactly 80% third harmonic and 40% fifth harmonic — no crosstalk between harmonics.

- **Bounded output.** For input in [-1, 1], each T_n is also bounded to [-1, 1]. No matter how many harmonics you add, the output amplitude is predictable and bounded by the sum of the weights.

- **Efficient evaluation.** The three-term recurrence `T_{n+1} = 2x·T_n - T_{n-1}` means computing all 7 polynomials requires only ~14 multiplications and 7 additions. No trigonometric functions, no lookup tables.

- **Precise preset design.** The Warm/Aggressive/Hollow/Dense presets are simply different weight vectors applied to the same polynomial chain, making it trivial to design and A/B different harmonic profiles.

### Strengths

1. **Frequency-agnostic.** The waveshaper doesn't know or care what frequency it's processing. A 20Hz bass note, a 100Hz bass note, and a chord containing both will all get correct harmonics simultaneously — because the polynomials operate sample-by-sample on the waveform itself.

2. **Zero tracking latency.** There is no analysis window, no autocorrelation, no pitch estimation. The output for sample N depends only on input sample N (plus the envelope history). This makes the response instantaneous for transients like kick drums.

3. **Automatic silencing.** When input goes silent, the envelope follower decays toward zero within the release time. The phantom signal = waveshaper_output × envelope, so it naturally goes to zero. No voice management, no gate, no special "silence detection" code.

4. **Natural amplitude scaling.** Because the waveshaper's input IS the bass signal, the output amplitude is inherently a function of input amplitude. Loud input → loud harmonics. Quiet input → quiet harmonics. The envelope follower adds additional smoothing but the basic relationship is already there in the math.

5. **Polyphony for free.** If the input contains two simultaneous bass notes at 40Hz and 60Hz, the waveshaper generates harmonics for BOTH simultaneously — {80, 120, 160, 200, 240, 280, 320} Hz from the 40Hz note AND {120, 180, 240, 300, 360, 420, 480} Hz from the 60Hz note — all from a single pass through the polynomial. No voice allocation needed.

6. **Simplicity.** The entire DSP chain is ~400 lines of C++. There are no edge cases for "what happens when the tracker fails" or "what happens when a voice steals from another voice" or "what happens during a pitch glide." The math is stateless and deterministic.

### Weaknesses

1. **No phase control.** Chebyshev polynomials lock the phase of each harmonic to the input signal's phase. The v1 recipe_phase_h2..h8 parameters allowed shifting individual harmonic phases, which could produce subtly different timbres. This control is lost in v2.

2. **No explicit Instrument mode.** v1 could accept MIDI input and synthesize harmonics from scratch for note events. v2 requires audio input — it can only add harmonics to an existing signal, not create one from nothing. (This could be re-added later by synthesizing a simple sine wave from MIDI note events and feeding it into the waveshaper.)

3. **Crossover artifacts.** The LR4 crossover at the phantom threshold introduces a 360° phase shift at the crossover frequency. While magnitude reconstruction is guaranteed (`low + high = allpass(input)`), the phase shift means the sum is NOT identical to the original signal. For most audio material this is inaudible, but it's theoretically imperfect.

4. **Intermodulation distortion.** When the bass band contains multiple frequencies, the waveshaper generates not only the intended harmonics of each frequency but also intermodulation products (sum and difference frequencies). For a single bass note this is irrelevant, but for bass chords with closely-spaced notes, intermodulation can produce dissonant tones. v1's additive synthesis approach produced only clean harmonics with no intermodulation.

5. **Less "controllable character" per preset.** v1's rotation parameter shifted the harmonic emphasis pattern in a way that produced continuously-varying timbral changes. v2's presets are fixed weight vectors — you can interpolate between them, but there's no equivalent of rotation.

---

## Parameter Comparison

### Removed Parameters (v1 only)

| Parameter | Why removed |
|---|---|
| `tracking_sensitivity` | No pitch tracker in v2 |
| `tracking_glide` | No pitch tracker |
| `deconfliction_mode` | No voice system |
| `max_voices` | No voice system |
| `stagger_delay` | No voice system |
| `recipe_phase_h2..h8` | Chebyshev polynomials don't support independent phase control |
| `recipe_rotation` | Not applicable to polynomial waveshaping |
| `sidechain_duck_amount/attack/release` | Removed to simplify; sidechain ducking may be re-added later |

### Added Parameters (v2 only)

| Parameter | Purpose |
|---|---|
| `env_attack_ms` | Envelope follower attack time (how fast phantom follows input onset) |
| `env_release_ms` | Envelope follower release time (how fast phantom fades after input stops) |
| `bypass` | Proper bypass parameter (also wired to DAW's native bypass) |

### Retained Parameters

| Parameter | v1 behavior | v2 behavior |
|---|---|---|
| `ghost` | Wet/dry blend between original and phantom | Same |
| `ghost_mode` | Replace (mute lows) or Add (keep lows) | Same |
| `phantom_threshold` | Crossover frequency for CrossoverBlend | Crossover frequency for BassExtractor LR4 |
| `phantom_strength` | Pre-mix gain on phantom signal | Same |
| `harmonic_saturation` | Drove saturation stage in HarmonicGenerator | Drives Chebyshev polynomial input scaling + tanh blend |
| `recipe_h2..h8` | Amplitude of each synthesized harmonic oscillator | Chebyshev polynomial T2–T8 coefficients |
| `recipe_preset` | Selects predefined H2–H8 weight vectors | Same (different values — optimized for waveshaper response) |
| `output_gain` | Final output gain in dB | Same |
| `binaural_mode/width` | BinauralStage L/R decorrelation | Same (engine retained from v1) |
| `stereo_width` | M/S stereo width control | Same |
| `mode` | Switches between Effect (pitch tracker) and Instrument (MIDI voices) | Retained but Instrument mode is currently a no-op |

---

## Preset Table Comparison

### v1 Presets (oscillator amplitudes)

```
Warm       = { 0.80, 0.70, 0.50, 0.35, 0.20, 0.12, 0.07 }
Aggressive = { 0.40, 0.50, 0.90, 1.00, 0.80, 0.50, 0.30 }
Hollow     = { 0.10, 0.80, 0.10, 0.70, 0.10, 0.60, 0.10 }
Dense      = { 0.85, 0.85, 0.85, 0.85, 0.85, 0.85, 0.85 }
```

### v2 Presets (Chebyshev polynomial weights)

```
Warm       = { 0.80, 0.60, 0.40, 0.28, 0.18, 0.10, 0.05 }
Aggressive = { 0.50, 0.70, 0.85, 0.75, 0.55, 0.35, 0.20 }
Hollow     = { 0.00, 0.80, 0.00, 0.60, 0.00, 0.40, 0.00 }
Dense      = { 0.70, 0.70, 0.70, 0.70, 0.70, 0.70, 0.70 }
```

v2 presets are tuned differently because the waveshaper's output level and spectral balance differ from additive synthesis. In particular, Warm has a faster rolloff (0.80 → 0.05) because the waveshaper naturally adds energy and a steep rolloff prevents the upper harmonics from sounding harsh.

---

## Performance Comparison

| Metric | v1 | v2 |
|---|---|---|
| CPU per block (estimated) | ~15µs (7 sin() per sample × voices) | ~5µs (polynomial eval + envelope) |
| Memory | ~4KB per voice (phase arrays, state) | ~200 bytes (filter state + envelope) |
| Allocation during processing | None (pre-allocated voice pool) | None (pre-allocated buffers) |
| Thread safety | Voice pool access needed careful gating | No shared mutable state between channels |

---

## Conclusion

The v2 Chebyshev waveshaper architecture solves all the production-blocking issues of v1 (stuck voices, tracking latency, sub-100Hz failure, amplitude mismatch) at the cost of losing phase control and explicit MIDI-driven synthesis. For the primary use case of the plugin — enhancing bass content in a mix — v2 is unambiguously better. The lost features (phase control, MIDI instrument mode, deconfliction) served edge cases that can be re-added later if user demand warrants it.
