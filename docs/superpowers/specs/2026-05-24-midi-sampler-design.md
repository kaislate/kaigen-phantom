# MIDI-playable sampler — design

**Date:** 2026-05-24
**Branch:** `integration/native-plus-reverb`
**Status:** approved (in-conversation)

## Goal

Add a MIDI-playable sampler to Kaigen Phantom so the plugin can be played
standalone via a MIDI clip — no sidechain audio required. The sampler
becomes a third option for the engines' input source (alongside the
existing main Input and Sidechain buses), keeping the plugin's identity
as a phantom-harmonic processor intact: notes trigger a sample, the
sample feeds the engines, the engines extract phantom harmonics as
they always have.

## Architecture

**Approach: pre-engine source mixer.**

A new `PhantomSampler` block sits before the engines. A new
`INPUT_SOURCE` parameter (choice: 0=Input, 1=Sidechain, 2=Sampler)
picks what audio the engines see each block. The sampler is just one
of three possible sources — no engine code changes, no engine focus
state changes, no modulation scoping changes.

```
                ┌───────────────────┐
   Main Input ──┤                   │
   Sidechain ───┤  Source select   ─┼──→ DualEngineHost (unchanged)
   PhantomSampler ─┤  (INPUT_SOURCE) │
                └───────────────────┘
                         ▲
                    MIDI events
                    (note on/off)
```

**Why this approach (vs. parallel engine slot or standalone bypass):**
- Sample-as-input matches the chosen mental model: notes produce phantom
  harmonics, not raw playback to output
- Surgical change to `processBlock` — one switch statement
- No reshaping of `DualEngineHost`, no new "Sampler" engine focus tab,
  no broadening of `Routing` / modulation scope rules
- The plugin stays an FX (`IS_SYNTH=FALSE`) — usable on FX tracks with
  MIDI routing; no separate instrument-variant build needed

## Components

### A. PhantomSampler — `Source/DSP/PhantomSampler.{h,cpp}` (new)

Wraps `juce::Synthesiser` with one `juce::SamplerSound` and up to 8
custom `PhantomSamplerVoice` voices. The custom voice is required
because JUCE's stock `juce::SamplerVoice` does not support
loop-while-held — it only plays a sample once and releases.

**Public surface:**
```cpp
class PhantomSampler
{
public:
    PhantomSampler();
    void prepareToPlay(double sampleRate, int blockSize);

    // Called from PluginProcessor::processBlock before engine rendering.
    void processMidi(const juce::MidiBuffer& midi);
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer);

    // Sample load — call from background thread; thread-safe swap.
    bool loadSample(const juce::AudioBuffer<float>& decoded, double sampleRate);
    void clearSample();

    // Live param updates — called from prepareToPlay + per-block APVTS scrape.
    void setRootNote(int midiNote);          // 0..127, native pitch note
    void setLoopEnabled(bool loop);
    void setGainDb(float gainDb);
    void setEnvelope(float attackSec, float decaySec,
                     float sustain01, float releaseSec);

    bool hasSample() const noexcept;
    int  getActiveVoiceCount() const noexcept;
    int  getPlayheadPosition() const noexcept;   // most-recent voice, 0..numSamples
};
```

**PhantomSamplerVoice (subclass of `juce::SamplerVoice`):**
- Maintains a per-voice playback-position float and a per-voice ADSR
- `renderNextBlock` reads from the shared `SamplerSound`'s `AudioBuffer`
  at the rate determined by (note / root-note) interval
- When `loop == true` and the position passes the buffer end, wraps to
  position 0 instead of releasing the voice
- When `loop == false`, releases the voice when position hits end
- On note-off: enters envelope release; voice fully stops when envelope
  reaches zero

**Sample data ownership:**
- One `PhantomSamplerSound` holds the decoded `AudioBuffer<float>` +
  source sample rate
- `loadSample` swaps the sound atomically using `juce::Synthesiser`'s
  `clearSounds` + `addSound` pattern, with a mutex to prevent
  `renderNextBlock` reading mid-swap. Mutex held only during swap (μs);
  audio thread waits at most one block.

### B. Source selector inside processBlock

`PluginProcessor::processBlock` gains a new local `juce::AudioBuffer<float>
samplerBuffer` (member, sized in `prepareToPlay`):

```cpp
// 1. Forward MIDI to sampler (in addition to existing engine MIDI fan-out).
phantomSampler.processMidi(midiMessages);

// 2. Render sampler into its own buffer.
samplerBuffer.clear();
phantomSampler.renderNextBlock(samplerBuffer);

// 3. Pick the buffer the engines will see.
const int source = (int) apvts.getRawParameterValue(ParamID::INPUT_SOURCE)->load();
juce::AudioBuffer<float>* engineInput = nullptr;
switch (source)
{
    case 0:  engineInput = &buffer; break;                 // main input
    case 1:  engineInput = sidechainAvailable
                            ? &sidechainBuffer : &buffer;  // sidechain or fallback
    case 2:  engineInput = &samplerBuffer; break;          // sampler
    default: engineInput = &buffer;
}

// 4. Copy engineInput into `buffer` (the in-place buffer engines mutate),
//    OR refactor DualEngineHost to take a source buffer + dest buffer.
```

The cleanest cut: keep the existing in-place pattern; before engine
processing, copy `engineInput` over `buffer`. Adds one buffer copy per
block, negligible cost.

### C. SamplerStrip — `Source/UI/widgets/SamplerStrip.{h,cpp}` (new)

A ~124px tall widget that lives in `RightPanel` between the spectrum
graph (above) and the oscilloscope (below). Three rows:

```
┌──────────────────────────────────────────────────────────────────────┐
│  ENGINE SOURCE: [Input] [Sidechain] [Sampler]      mysample.wav [📁] │  24 px
├──────────────────────────────────────────────────────────────────────┤
│     ▁▃▆▇█▇▆▃▁▁▂▄▆▇█▇▅▃▂▁▁▂▃▅▇█▇▅▃▁  (waveform thumbnail)             │  60 px
│                ↑ playhead while voice plays                          │
├──────────────────────────────────────────────────────────────────────┤
│  ROOT [C3 ▾]  LOOP [●]  GAIN [○]  A [─] D [─] S [─] R [─]            │  40 px
└──────────────────────────────────────────────────────────────────────┘
```

**Header row (24 px):**
- 3-segment `INPUT_SOURCE` selector on the left: Input | Sidechain | Sampler
  (matching the visual style of the existing `ChoiceToggle` widget).
  Bound to `ParamID::INPUT_SOURCE`. Elevating this above the strip makes
  it discoverable — users wanting the sampler to be audible must switch
  to "Sampler" here.
- Sample filename (greyed when none loaded) + small folder-icon button
  on the right. Folder icon opens `juce::FileChooser` async.

**Waveform row (60 px):**
- Static mono mixdown thumbnail of the loaded sample, rendered once at
  load time into a cached `juce::Image`. No live FFT cost on paint.
- Vertical playhead line drawn over the thumbnail at the position
  reported by `PhantomSampler::getPlayheadPosition()`. Tracks the
  most-recently-triggered voice only (cheap visual feedback; per-voice
  playheads would clutter).
- The whole waveform area is a click target → opens the file picker.
- Whole strip is a `juce::FileDragAndDropTarget` → dropping a supported
  audio file loads it.

**Controls row (40 px):**
- `ROOT` — combobox C-2 through G8 mapped to MIDI 0..127. Bound to
  `ParamID::SAMPLER_ROOT_NOTE`. Default C3 (note 60).
- `LOOP` — single-glyph toggle (matches `EtchedToggle` style). Bound to
  `ParamID::SAMPLER_LOOP`. Default off.
- `GAIN` — small knob (matches `PhantomMiniKnob`), ±12 dB. Bound to
  `ParamID::SAMPLER_GAIN`. Default 0 dB.
- `A D S R` — four tiny vertical sliders (~20 px wide each). Bounds:
  attack 1ms–4s, decay 0–4s, sustain 0–1, release 1ms–4s. Bound to
  `SAMPLER_A` / `D` / `S` / `R`. Defaults: A=5ms, D=200ms, S=0.8, R=200ms.

**Loading states:**
- Idle (no sample): waveform area shows "Click or drop a sample"
  placeholder text. Sample-name slot shows "—".
- Loading: "Loading…" overlay. Strip controls remain interactive; only
  the file-load buttons disable until decode completes.
- Loaded: thumbnail + filename + playhead active.
- Error (too large / unsupported / decode failure): brief error text in
  the waveform area ("Sample too large — max 5MB" / "Couldn't decode
  file" / etc.) that clears on next load attempt.

### D. PluginProcessor changes

**Members:**
- `kaigen::phantom::PhantomSampler phantomSampler;`
- `juce::AudioBuffer<float> samplerOutputBuffer;` (sized in
  `prepareToPlay` to maxBlockSize × 2 channels)

**New parameters (added to `makeLayout`):**
| ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `INPUT_SOURCE` | choice | 0/1/2 | 0 (Input) | Input / Sidechain / Sampler |
| `SAMPLER_ROOT_NOTE` | int | 0..127 | 60 (C3) | |
| `SAMPLER_LOOP` | bool | — | false | |
| `SAMPLER_GAIN` | float | -12..+12 dB | 0 | |
| `SAMPLER_A` | float | 0.001..4 s | 0.005 | |
| `SAMPLER_D` | float | 0..4 s | 0.200 | |
| `SAMPLER_S` | float | 0..1 | 0.80 | |
| `SAMPLER_R` | float | 0.001..4 s | 0.200 | |

`INPUT_SOURCE` and `SAMPLER_ROOT_NOTE` are categorical / non-modulatable;
the rest are full APVTS-automatable parameters following existing
conventions.

**processBlock additions** (per the source-selector pseudocode above) +
APVTS scrape that pushes the current root note / loop / gain / ADSR into
`phantomSampler` before render.

**Sample persistence:**
- New private member `juce::MemoryBlock cachedSampleBytes;` holds the
  original source-file bytes (for preset embed).
- `juce::String cachedSampleFilename;` for display + format hint.
- Setters used by the SamplerStrip's async load callback.

### E. State serialization

`PluginProcessor::getStateInformation` is extended to append a
`<Sampler>` child to the APVTS state ValueTree:

```cpp
juce::ValueTree sampler("Sampler");
if (cachedSampleBytes.getSize() > 0)
{
    sampler.setProperty("filename", cachedSampleFilename, nullptr);
    sampler.setProperty("bytes",
        juce::Base64::toBase64(cachedSampleBytes.getData(),
                                cachedSampleBytes.getSize()),
        nullptr);
}
state.appendChild(sampler, nullptr);
```

`setStateInformation`: after `apvts.replaceState`, scan for the
`<Sampler>` child. If present, decode bytes from base64 into a
`MemoryBlock`, kick a background-thread decode via
`juce::AudioFormatManager`, swap into `phantomSampler` when ready.

**Pre-sampler presets:** parse fine. No `<Sampler>` child means no
sample loaded, sampler stays silent.

**Newer presets in older builds:** the `<Sampler>` child is ignored by
JUCE's ValueTree parser. Plugin loads everything else normally.

## Async loading

Every sample-decode path runs on a background thread to keep the editor
+ DAW responsive even on multi-MB samples:

```cpp
// In SamplerStrip's file-load callback:
juce::Component::SafePointer<SamplerStrip> self(this);
juce::Thread::launch([self, file]
{
    juce::MemoryBlock bytes;
    if (! file.loadFileAsData(bytes))             { /* dispatch error */ }
    else if (bytes.getSize() > 5 * 1024 * 1024)   { /* dispatch too-large */ }
    else
    {
        juce::AudioBuffer<float> decoded;
        double sampleRate = 0;
        if (decodeSample(bytes, decoded, sampleRate))
        {
            juce::MessageManager::callAsync([self, bytes, decoded, sampleRate, file]
            {
                if (auto* p = self.getComponent())
                    p->onDecodeFinished(file, bytes, decoded, sampleRate);
            });
        }
        else { /* dispatch error */ }
    }
});
```

Same `SafePointer` + file-changed-since-launch pattern that the cover
editor uses. `MessageManager::callAsync` posts the result back to the
message thread, which then calls into `PhantomSampler` (whose
`loadSample` is itself thread-safe via its internal mutex).

## Sample format support

`juce::AudioFormatManager::registerBasicFormats()` enables WAV / AIFF /
FLAC / Ogg Vorbis / MP3 out of the box. No additional decoder
dependencies needed.

## Polyphony

8 voices, hard cap. `juce::Synthesiser` handles voice allocation and
stealing automatically (oldest voice wins). For typical phantom-harmonic
material this is more than enough — playing a 4-note chord mixes 4
voices into the sampler buffer, the engines process the mixed signal,
phantom harmonics emerge from the mix.

Polyphonic input may produce unexpected harmonics in some cases (the
phantom-extraction algorithm assumes a coherent fundamental), but that's
the user's choice — chords are not blocked.

## File layout

| File | Status | Purpose |
|---|---|---|
| `Source/DSP/PhantomSampler.h/.cpp` | new | Synthesiser wrapper + custom voice + sound |
| `Source/UI/widgets/SamplerStrip.h/.cpp` | new | The 3-row UI widget |
| `Source/PluginProcessor.h/.cpp` | modified | New members + processBlock + state I/O |
| `Source/Parameters.h` | modified | 8 new param IDs |
| `Source/UI/panels/RightPanel.h/.cpp` | modified | Insert SamplerStrip between spectrum + oscilloscope |
| `CMakeLists.txt` | modified | Add the two new .cpp files |
| `tests/PhantomSamplerTests.cpp` | new | Voice allocation + loop-while-held + rate-shift correctness |

## Testing strategy

**Unit tests (`tests/PhantomSamplerTests.cpp`):**
- Voice allocation: 8 sequential note-ons → 8 active voices; 9th steals oldest
- Loop behavior: note on with loop enabled, hold > sample length → playback position wraps to 0 (sampled at the buffer boundary)
- Rate-shift correctness: note 60 with root note 60 → playback rate = 1.0; note 72 with root note 60 → playback rate ≈ 2.0 (octave up)
- ADSR release: note-off → output decays to silence over the release time
- Source-byte → AudioBuffer roundtrip via AudioFormatManager (no audio thread)

**Manual:**
- Drag-drop a WAV onto the strip → loads, name appears, click waveform
  → file picker. Play C3 → hear the sample at native pitch. Play C4 →
  hear it an octave up.
- Switch INPUT_SOURCE between Input/Sidechain/Sampler → audible
  switching with no clicks (one-block fade if needed).
- Load a 5+ MB sample → polite "Sample too large" error, sampler stays
  empty, no crash.
- Save preset → reopen project → sample loaded automatically with the
  same root note + ADSR + loop state.
- Drag-drop during playback → load happens off-thread, no audio
  dropouts.

## Out of scope (deferred)

- **Multi-zone mapping** — single sample only for v1. Multi-sample
  needs a popup editor and pack-style sample bundling.
- **Loop start/end markers** — whole-sample loop only. Markers need
  waveform interaction (drag handles), not enough vertical space.
- **Sample stretching / pitch-decoupling** — pitch follows playback
  rate. No time-stretching algorithm.
- **MIDI velocity → amplitude curve** — velocity sets the voice gain
  linearly (0..1 from velocity 0..127). Curve control is a v2.
- **Per-voice playhead overlays** — single-voice playhead only.
- **Sample browser** — no built-in sample library. User loads from
  filesystem.

## Migration / compatibility

- `INPUT_SOURCE` default = 0 (Input) so existing presets and existing
  user sessions behave identically to today.
- Existing presets parse identically (the new params get their
  defaults; the `<Sampler>` child is absent → sampler empty).
- Pack export (.kaipack) inherits any embedded samples automatically
  via the existing `.fxp` bundling. Larger packs when presets carry
  samples; no special handling needed.
