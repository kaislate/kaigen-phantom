# Kaigen Phantom — Design Spec
**Date:** 2026-04-08  
**Status:** Approved, UI deferred  
**Plugin:** KAIGEN | PHANTOM — Psychoacoustic Bass & Sub Engine

---

## Overview

Phantom generates bass that doesn't require speakers to reproduce it. Using the psychoacoustic **phantom fundamental** phenomenon, it removes or replaces real sub-bass with a precisely tuned harmonic series that causes the brain to perceive the fundamental — even on small speakers, headphones, and phone playback.

It operates in two modes (Effect and Instrument) and exposes a **Recipe Engine** for deep per-harmonic timbral control over the phantom's character.

---

## Modes of Operation

### Effect Mode
- Processes incoming audio
- **Monophonic** YIN pitch detection on the main input
- Detects the fundamental below `phantom_threshold` (default 80Hz)
- Generates phantom harmonics, crossfades with the real signal via `ghost` control
- Sidechain bus available for ducking

### Instrument Mode
- Triggered via MIDI note input
- **Polyphonic** — one voice per active MIDI note (up to `max_voices`, default 4)
- MIDI note frequency used directly as fundamental — no pitch tracking needed
- Polyphonic deconfliction selectable per 6 strategies (see below)
- Same harmonic generation and downstream engines as effect mode

### Sound Design
- Not a separate mode — the Recipe Engine's per-harmonic amplitude and phase control is available in both modes
- The timbral character of the phantom (Warm, Aggressive, Hollow, Dense, Custom) is shaped here

---

## DSP Architecture

### Signal Flow

```
EFFECT MODE:
Audio In → PitchTracker → fundamental Hz
                                    ↓
INSTRUMENT MODE:
MIDI In → NoteHz lookup →           ↓
                                    ↓
                        HarmonicGenerator (voice pool)
                            └─ Recipe: H2–H8 amplitude + phase
                            └─ Deconfliction strategy (instrument mode)
                                    ↓
                        BinauralStage
                            └─ Off / Spread / Voice-Split
                            └─ Width: 0–100%
                                    ↓
                        PerceptualOptimizer
                            └─ Equal-loudness contour compensation
                                    ↓
                        CrossoverBlend
                            └─ Ghost 0–100% (Replace default, Add toggle)
                            └─ Sidechain bus → ducking envelope
                                    ↓
                        Audio Out (stereo)
```

### Engines

**PitchTracker** (`Engines/PitchTracker.h/cpp`)
- YIN algorithm, monophonic
- Configurable confidence threshold (`tracking_sensitivity`)
- Pitch glide smoothing (`tracking_glide`, 0–200ms)
- Effect mode only — bypassed in instrument mode

**HarmonicGenerator** (`Engines/HarmonicGenerator.h/cpp`)
- Manages a voice pool (1 voice in effect mode, N voices in instrument mode)
- Each voice: 7 sine oscillators at harmonics H2–H8 of the fundamental
- Per-harmonic amplitude (`recipe_h2`–`recipe_h8`) and phase (`recipe_phase_h2`–`recipe_phase_h8`)
- Recipe presets: Warm, Aggressive, Hollow, Dense, Custom
- Recipe rotation: shifts harmonic emphasis up/down the series
- Harmonic saturation: adds further overtones via soft-clip waveshaping
- In instrument mode, delegates to the selected deconfliction strategy

**BinauralStage** (`Engines/BinauralStage.h/cpp`)
- Applies L/R harmonic matrix to the harmonic buffer
- **Off**: mono/center phantom output
- **Spread**: lower harmonics center, higher harmonics fan outward
- **Voice-Split**: each voice assigned to a stereo position (instrument mode)
- Width parameter controls degree of L↔R divergence
- Exposes `isUsingBinaural()` flag → UI headphone indicator

**PerceptualOptimizer** (`Engines/PerceptualOptimizer.h/cpp`)
- Applies ISO 226 equal-loudness contour correction per channel
- Adjusts harmonic amplitudes so perceived loudness of phantom matches original bass
- Operates post-binaural so each channel is independently corrected

**CrossoverBlend** (`Engines/CrossoverBlend.h/cpp`)
- IIR crossover at `phantom_threshold` frequency
- **Replace mode** (default): sub content removed as phantom harmonics added
- **Add mode**: phantom harmonics layered on top of unmodified signal
- `ghost` controls the crossfade (0% = real only, 100% = full phantom)
- Sidechain input: envelope follower drives ducking with configurable attack/release
- Stereo width applied to phantom harmonics before final mix

### Polyphonic Deconfliction Strategies (`Deconfliction/`)

Six selectable strategies for instrument mode polyphony:

| Strategy | Approach |
|---|---|
| **Partition** | Shared harmonics assigned to one voice; others skip them |
| **Spectral Lane** | Each voice gets its own frequency window |
| **Stagger** | Onset priming — voices delayed by `stagger_delay` (0–30ms) |
| **Odd/Even** | Voice 1 gets odd harmonics, Voice 2 gets even harmonics |
| **Residue** | Shared harmonics attenuated; unique harmonics boosted |
| **Binaural** | Voices assigned to L/R positions; BinauralStage handles separation |

Each strategy implements a common `IDeconflictionStrategy` interface with a `resolveVoices(VoicePool&, HarmonicBuffer&)` method.

---

## Parameters (~35 total)

### Mode & Global
| ID | Range | Default |
|---|---|---|
| `mode` | Effect / Instrument | Effect |
| `ghost` | 0–100% | 100% |
| `ghost_mode` | Replace / Add | Replace |
| `phantom_threshold` | 20–150 Hz | 80 Hz |
| `phantom_strength` | 0–100% | 80% |
| `output_gain` | -24–+12 dB | 0 dB |

### Recipe Engine
| ID | Range | Default |
|---|---|---|
| `recipe_h2`–`recipe_h8` | 0–100% (×7) | Warm preset |
| `recipe_phase_h2`–`recipe_phase_h8` | 0–360° (×7) | 0° |
| `recipe_preset` | Warm/Aggressive/Hollow/Dense/Custom | Warm |
| `recipe_rotation` | -180–+180° | 0° |
| `harmonic_saturation` | 0–100% | 0% |

### Binaural Stage
| ID | Range | Default |
|---|---|---|
| `binaural_mode` | Off / Spread / Voice-Split | Off |
| `binaural_width` | 0–100% | 50% |

### Pitch Tracker (Effect Mode)
| ID | Range | Default |
|---|---|---|
| `tracking_sensitivity` | 0–100% | 70% |
| `tracking_glide` | 0–200ms | 20ms |

### Polyphonic Deconfliction (Instrument Mode)
| ID | Range | Default |
|---|---|---|
| `deconfliction_mode` | Partition/Lane/Stagger/Odd-Even/Residue/Binaural | Partition |
| `max_voices` | 1–8 | 4 |
| `stagger_delay` | 0–30ms | 8ms |

### Crossover & Sidechain
| ID | Range | Default |
|---|---|---|
| `sidechain_duck_amount` | 0–100% | 0% |
| `sidechain_duck_attack` | 1–100ms | 5ms |
| `sidechain_duck_release` | 10–500ms | 80ms |
| `stereo_width` | 0–200% | 100% |

---

## UI Design

**Deferred.** User will provide a layout design (sketch/photo) before UI implementation begins. 

Known constraints from concept doc:
- Window size: 900×550
- Centerpiece: Recipe Wheel — circular interface with 7 radial sliders (H2–H8)
- Recipe wheel glows with the color of the perceived fundamental pitch
- Color palette: dark indigo background (#0a0820), ghost blue-white harmonics (#88bbff→#fff), deep purple real bass (#6633aa), amber threshold (#ffaa00), phosphorescent green active (#00ff88)
- Headphone indicator shown when binaural mode is active

---

## Build Configuration

Mirrors Conduit exactly:
- **CMake 3.22+** with JUCE 7.0.9 via FetchContent
- **Formats:** VST3 + Standalone
- **NEEDS_MIDI_INPUT:** true
- **IS_SYNTH:** false
- **Sidechain:** second input bus declared in processor
- **Tests:** Catch2 v3, one test file per engine
- **Company:** Kaigen | **Plugin Code:** Kgph

---

## What's Explicitly Out of Scope (v1)

- Speaker simulation (better tools exist on the market)
- Polyphonic audio pitch tracking (monophonic YIN only in effect mode)
- Sidechain masking compensation (equal-loudness only for perceptual optimizer)
- Preset/program management beyond default values
- UI design (deferred to separate session with user-provided layout)
