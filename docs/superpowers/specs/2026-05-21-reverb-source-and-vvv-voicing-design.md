# Reverb source selector + VVV-character voicing — design

**Date:** 2026-05-21
**Branch:** `integration/native-plus-reverb`
**Status:** approved (in-conversation)

## Goal

Two related additions to the single-knob reverb:

1. **Reverb source selector** — a two-position toggle that switches the
   reverb input between the current post-engine signal and a
   phantom-only signal (just the synthesised harmonic content, no dry
   input pass-through).
2. **VVV voicing pass** — four targeted constant tweaks in
   `SingleKnobReverb` to push the character closer to Valhalla Vintage
   Verb "Concert Hall 1970s".

Both keep the existing "one user-facing knob" contract — no new tunable
DSP params, just a routing toggle.

## Part 1 — Reverb source selector

### New APVTS param

- ID: `reverb_source` (global, single value across both engines)
- Type: `AudioParameterChoice` with values `{ "Post-Engine", "Phantom Only" }`
- Default: `"Post-Engine"` (preserves current behaviour)

### Engine surface changes

`PhantomEngine`:

- New stereo `juce::AudioBuffer<float> phantomOnlyBuf` member, sized in
  `prepare()` to the worst-case block size.
- Inside the existing per-sample channel loop, after computing
  `phantomOut`, also write `phantomOut * ghostAmount * outputGainLin`
  into `phantomOnlyBuf` at the current sample index. **Same gain stage
  as the regular output's phantom contribution** so the reverb hears
  the same level whether the routing is post-engine or phantom-only.
- New accessor: `const juce::AudioBuffer<float>& getPhantomOnlyOutput() const noexcept`.
- Cost: one extra store per sample per channel — negligible.

`DualEngineHost`:

- New stereo `juce::AudioBuffer<float> phantomOnlyMix` member.
- After the existing crossfader runs on the regular outputs, apply the
  same per-sample crossfade weights to `engineA.getPhantomOnlyOutput()`
  and `engineB.getPhantomOnlyOutput()` into `phantomOnlyMix`. Re-uses
  the existing per-sample weight calculation; one extra MAC per sample
  per channel.
- New accessor: `const juce::AudioBuffer<float>& getPhantomOnlyOutput() const noexcept`.

### Routing in `PhantomProcessor::processBlock`

At the existing reverb-send block (the `isAudibleNow` branch), select
the source buffer based on `reverb_source`:

```cpp
const juce::AudioBuffer<float>* reverbInput =
    (reverbSourceParam->load() < 0.5f)
        ? &buffer                                      // Post-Engine
        : &dualEngineHost.getPhantomOnlyOutput();      // Phantom Only

for (int c = 0; c < nCh && c < reverbScratch.getNumChannels(); ++c)
    reverbScratch.copyFrom(c, 0, *reverbInput, c, 0, n);
```

All other reverb logic (slewing, parallel mix, bypass-on-zero-mix)
stays identical. The dry signal mixed back in is always `buffer` — only
the wet input changes.

Cache `reverbSourceParam = apvts.getRawParameterValue(ParamID::REVERB_SOURCE)`
alongside the existing `reverbMixParam` so the audio thread does a
single atomic load per block.

### UI

**Native editor** (`Source/UI/panels/RightPanel.cpp/.h`): add an
`EtchedToggle` alongside `reverbKnob` inside the Levels card (same
widget the Auto-gain toggle uses on this branch — single etched word,
on/off visually obvious). Label cycles `"POST" ↔ "PHNTM"` (max 5 chars
to fit the Levels card). Implemented as a 2-value `AudioParameterChoice`
that the toggle reads as a bool (0 = Post, 1 = Phantom).

**WebView UI** (`Source/WebUI/index.html` + the appropriate JS):

- Add a small `<button class="tog">POST/PHNTM</button>` next to the
  `phantom-knob data-param="reverb_mix"` element.
- Wire it via the existing toggle-state pattern (`getToggleStateLogical`
  or the equivalent for a 2-value choice param).
- Register `reverb_source` in `KAIGEN_GLOBAL_PARAMS`.

### Param ID + relay

- `Parameters.h`: add `inline constexpr auto REVERB_SOURCE = "reverb_source";`
  and a push_back in `getAllParameterIDs()`.
- `PluginEditor.h`: add `juce::WebComboBoxRelay reverbSourceRelay { "reverb_source" };`
  (since it's a `juce::AudioParameterChoice`).
- `PluginEditor.cpp`: register in `comboRelays[]` and `comboBindings[]`
  alongside the existing engine-mode/ghost-mode combos.

## Part 2 — VVV voicing tweaks

All in `Source/DSP/SingleKnobReverb.cpp/.h`. No new params; no API
changes. Constants only.

1. **Mod depth 2.0 ms → 3.5 ms**
   - Change `kModDepthMs` (currently inline in `prepareDelayLines()`).
   - Stays well under the shortest delay-line headroom (1789 samples at
     44.1 kHz = 40.5 ms; 3.5 ms is ~9% headroom usage vs 5% before).
   - Verify: `modDepthSamples * 1.5f + 8` (the headroom calculation)
     must still fit; it does.

2. **Early diffusion 4 → 6 allpass stages**
   - Change `kNumEarlyAPs = 4` → `6` in `SingleKnobReverb.h`.
   - Extend `kEarlyAPSamples[]` to `{ 113, 197, 313, 421, 571, 691 }`
     (continued mutual-prime progression).
   - The `earlyAPL` / `earlyAPR` `std::array`s grow accordingly — no
     allocator change since the array size is templated on the constant.

3. **Static output low-shelf, +4 dB below 250 Hz**
   - Add an extra one-pole low-shelf to the output filter section.
   - New state: `outShelfL`, `outShelfR`, `outShelfCoef`. Add to the
     reset path.
   - Implementation: one-pole LPF tracking the input, then add
     `(shelfGain - 1.0f) * shelfState` back to the signal. Identical
     structure to the in-loop bass-shelf — but applied *post-tank, post-
     band-limit filters* so stability is irrelevant.
   - Coefficient: `onePoleCoef(250.0f, sr)`. Shelf gain factor: 0.585
     (gives ~+4 dB at DC, ~0 dB above 250 Hz).

4. **Pretank LPF 10 kHz → 8 kHz**
   - Change `kPretankLpfHz = 10000.0f` → `8000.0f` in the header.
   - More aggressive smoothing of highs going into the tank — VVV
     "1970s color" depends on this.

## Out of scope (explicitly)

- Increasing FDN line count (8 → 12+).
- Replacing the FDN topology with a different reverb structure.
- Exposing reverb internals as user-tunable params.
- Touching the engine ghost-mode logic. The selector reads
  `phantomOut * ghostAmount` regardless of which ghost mode the engine
  is in — that's intentional.

## Migration / compatibility

- `reverb_source` defaults to `"Post-Engine"`, so existing presets
  and saved DAW projects continue to behave exactly as before.
- The voicing tweaks (mod depth, AP count, output shelf, pretank LPF)
  change the reverb's *sound* on every preset, but the reverb is
  off-by-default (`reverb_mix = 0`) so no preset that doesn't
  explicitly use reverb is affected.

## Testing notes

- Build and verify 0 errors / 0 warnings.
- Listen test (user):
  - Reverb on a clean input with reverb_source = Post: should match
    previous behaviour for the wet character apart from the voicing
    pass (more lush, bassier, slightly darker tank input).
  - Reverb with reverb_source = Phntm and engine in any ghost mode:
    only the synth contribution should hit the reverb; dry input
    should pass through to the output without being reverbed.
- Existing tests (`tests/`) shouldn't need changes — reverb DSP isn't
  test-covered yet and this adds no new test surface that's easily
  isolatable from audio.
