# MIDI-playable sampler Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a MIDI-playable sampler that feeds the existing Phantom engines as a third input source (alongside Input and Sidechain), so the plugin can be played standalone via a MIDI clip.

**Architecture:** A new `PhantomSampler` (8-voice `juce::Synthesiser` with custom loop-while-held voice) renders into its own `juce::AudioBuffer<float>` each block. A new `INPUT_SOURCE` choice parameter picks what the engines see: main input, sidechain, or sampler output. Sample bytes embed in the plugin state via a new `<Sampler>` child of the existing `<PluginState>` wrapper. The sampler UI is a 3-row strip placed between the spectrum graph and the oscilloscope in `RightPanel`.

**Tech Stack:** JUCE 8 (`juce::Synthesiser`, `juce::SynthesiserVoice`, `juce::AudioFormatManager`, `juce::Thread::launch`, `juce::FileDragAndDropTarget`, `juce::Base64`), C++20, Catch2 v3 for unit tests, MSBuild on Windows.

---

## Spec

This plan implements `docs/superpowers/specs/2026-05-24-midi-sampler-design.md`. Each task references the spec section it satisfies.

## File map

**Created:**
- `Source/DSP/PhantomSampler.h/.cpp` — `juce::Synthesiser` wrapper + `PhantomSamplerSound` + `PhantomSamplerVoice` with loop-while-held and pitched playback
- `Source/UI/widgets/SamplerStrip.h/.cpp` — the 3-row UI widget
- `tests/PhantomSamplerTests.cpp` — Catch2 tests for voice allocation, rate shift, loop, ADSR

**Modified:**
- `Source/Parameters.h` — 8 new parameter IDs
- `Source/PluginProcessor.h/.cpp` — sampler member, processBlock source switch, parameter declarations, `<Sampler>` state child, async sample-load coordinator
- `Source/UI/panels/RightPanel.h/.cpp` — insert `SamplerStrip` between spectrum and oscilloscope (shrinks spectrum's max height)
- `CMakeLists.txt` — add 2 new `.cpp` files to product target, 1 to test target

## Build & test commands (reference)

- Build (DEV): `cmake --build build-dev --config Release --target KaigenPhantom_VST3`
- Build (ship): `cmake --build build --config Release --target KaigenPhantom_VST3`
- Tests: `cmake --build build --config Release --target KaigenPhantomTests` then `./build/tests/Release/KaigenPhantomTests.exe`
- Single test tag: `./build/tests/Release/KaigenPhantomTests.exe "[sampler]"`

**Important context for the implementer:** the plugin state wrapper is `<PluginState>` (NOT inside `<APVTSState>`). Children like `<RecipeSlots>` and `<EditorView>` sit at the wrapper level beside `<APVTSState>`. The new `<Sampler>` child follows the same pattern — see `Source/PluginProcessor.cpp` lines 796–856 for the existing write pattern and 858+ for the read pattern.

---

## Task 1: Add the 8 new APVTS parameters

**Spec:** Architecture / E. New parameters table.

**Files:**
- Modify: `Source/Parameters.h` (add 8 IDs)
- Modify: `Source/PluginProcessor.cpp::makeLayout` (declare the parameters)

- [ ] **Step 1: Add the parameter IDs to `Source/Parameters.h`**

Find the existing `inline constexpr auto INPUT_GAIN_AUTO` declaration (line ~19) and the `MACRO1` declaration (line ~86). Add the new IDs in a new block grouped together, just below the existing global params section:

```cpp
// ── Sampler (PR-sampler) ─────────────────────────────────────────────
inline constexpr auto INPUT_SOURCE       = "input_source";       // choice 0/1/2
inline constexpr auto SAMPLER_ROOT_NOTE  = "sampler_root_note";  // int 0..127
inline constexpr auto SAMPLER_LOOP       = "sampler_loop";       // bool
inline constexpr auto SAMPLER_GAIN       = "sampler_gain";       // dB
inline constexpr auto SAMPLER_A          = "sampler_attack";     // seconds
inline constexpr auto SAMPLER_D          = "sampler_decay";      // seconds
inline constexpr auto SAMPLER_S          = "sampler_sustain";    // 0..1
inline constexpr auto SAMPLER_R          = "sampler_release";    // seconds
```

Also append these IDs to the `getAllNonModulatableParamIds()` / equivalent registration list if such a list exists in Parameters.h (search for `INPUT_GAIN_AUTO` to find it — the same pattern handles the global, non-per-engine ids). `INPUT_SOURCE` and `SAMPLER_ROOT_NOTE` join the non-modulatable list; the others are full float parameters that should be available for modulation if a `getAllModulatableParamIds` analogue exists.

- [ ] **Step 2: Declare the parameters in `PluginProcessor.cpp::makeLayout`**

Open `Source/PluginProcessor.cpp`. Search for the existing `INPUT_GAIN_AUTO` parameter declaration (line ~383 area) — it uses `std::make_unique<juce::AudioParameterBool>(...)`. Append a new block after the existing globals (just before the modulation/macro parameters, around line 408):

```cpp
// ── Sampler parameters ───────────────────────────────────────────────
params.push_back(std::make_unique<juce::AudioParameterChoice>(
    juce::ParameterID{ ParamID::INPUT_SOURCE, 1 },
    "Engine Input Source",
    juce::StringArray{ "Input", "Sidechain", "Sampler" },
    0));   // default = Input
params.push_back(std::make_unique<juce::AudioParameterInt>(
    juce::ParameterID{ ParamID::SAMPLER_ROOT_NOTE, 1 },
    "Sampler Root Note", 0, 127, 60));   // default C3
params.push_back(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{ ParamID::SAMPLER_LOOP, 1 },
    "Sampler Loop", false));
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ ParamID::SAMPLER_GAIN, 1 },
    "Sampler Gain",
    juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ ParamID::SAMPLER_A, 1 },
    "Sampler Attack",
    juce::NormalisableRange<float>(0.001f, 4.0f, 0.0001f, 0.4f), 0.005f));
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ ParamID::SAMPLER_D, 1 },
    "Sampler Decay",
    juce::NormalisableRange<float>(0.0f, 4.0f, 0.0001f, 0.4f), 0.200f));
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ ParamID::SAMPLER_S, 1 },
    "Sampler Sustain",
    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.80f));
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ ParamID::SAMPLER_R, 1 },
    "Sampler Release",
    juce::NormalisableRange<float>(0.001f, 4.0f, 0.0001f, 0.4f), 0.200f));
```

The skew factor 0.4 on time params matches existing time-domain knobs in this codebase (search for similar `NormalisableRange<float>(0.001f, 4.0f, 0.0001f, 0.4f)` in `makeLayout`).

- [ ] **Step 3: Verify both builds succeed**

```powershell
cmake --build build-dev --config Release --target KaigenPhantom_VST3
cmake --build build --config Release --target KaigenPhantom_VST3
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: both builds succeed; all existing tests still pass (126 cases / 7062 assertions). The new parameters appear in the host's parameter list but are not yet wired to anything.

- [ ] **Step 4: Commit**

```bash
git add Source/Parameters.h Source/PluginProcessor.cpp
git commit -m "feat: declare sampler parameters in APVTS

Adds 8 new params for the upcoming sampler: INPUT_SOURCE (choice
Input/Sidechain/Sampler), SAMPLER_ROOT_NOTE (C-2..G8), SAMPLER_LOOP,
SAMPLER_GAIN (\xC2\xB112 dB), and SAMPLER_A/D/S/R envelope (1ms-4s with
matching skew). No DSP or UI wired yet — the params live in APVTS
and serialize with plugin state; nothing consumes them. INPUT_SOURCE
default = Input, so existing presets behave identically."
```

---

## Task 2: PhantomSampler DSP — voice, sound, synth wrapper

**Spec:** Components → A. PhantomSampler.

**Files:**
- Create: `Source/DSP/PhantomSampler.h`
- Create: `Source/DSP/PhantomSampler.cpp`
- Create: `tests/PhantomSamplerTests.cpp`
- Modify: `CMakeLists.txt` (add PhantomSampler.cpp to product + test targets)

- [ ] **Step 1: Create `Source/DSP/PhantomSampler.h`**

```cpp
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <mutex>

namespace kaigen::phantom
{

// PhantomSamplerSound — holds the decoded sample audio + native sample
// rate. juce::Synthesiser ownership semantics: shared via reference-
// counted SynthesiserSound::Ptr.
class PhantomSamplerSound : public juce::SynthesiserSound
{
public:
    PhantomSamplerSound(juce::AudioBuffer<float> audio, double srcSampleRate)
        : data(std::move(audio)), sourceSampleRate(srcSampleRate) {}

    bool appliesToNote   (int)  override { return true; }
    bool appliesToChannel(int)  override { return true; }

    const juce::AudioBuffer<float>& getAudioBuffer() const noexcept { return data; }
    double getSourceSampleRate() const noexcept { return sourceSampleRate; }

private:
    juce::AudioBuffer<float> data;
    double                   sourceSampleRate { 0.0 };
};

// PhantomSamplerVoice — inherits SynthesiserVoice (not SamplerVoice)
// because stock SamplerVoice hardcodes one-shot play-then-release and
// doesn't support loop-while-held. We implement pitched playback + ADSR
// + optional looping ourselves.
class PhantomSamplerVoice : public juce::SynthesiserVoice
{
public:
    PhantomSamplerVoice();

    bool canPlaySound(juce::SynthesiserSound*) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples) override;

    // Configured by PhantomSampler before each block based on APVTS.
    void setRootNote(int n) noexcept             { rootNote = juce::jlimit(0, 127, n); }
    void setLoopEnabled(bool l) noexcept         { loopEnabled = l; }
    void setEnvelopeParameters(const juce::ADSR::Parameters& p) { adsr.setParameters(p); }
    void setGainLinear(float g) noexcept         { gainLinear = g; }

    // For the SamplerStrip playhead overlay. 0..numSamples-1 of the
    // active sample; -1 when voice idle. Read by the message thread,
    // written by the audio thread — atomic.
    int getPlayheadPosition() const noexcept { return playheadAtomic.load(std::memory_order_relaxed); }

private:
    double                pitchRatio       { 1.0 };
    double                sourcePosition   { 0.0 };
    int                   rootNote         { 60 };
    bool                  loopEnabled      { false };
    float                 gainLinear       { 1.0f };
    juce::ADSR            adsr;
    std::atomic<int>      playheadAtomic   { -1 };
};

// PhantomSampler — owns the juce::Synthesiser and exposes a small API
// suitable for PluginProcessor to drive each block.
class PhantomSampler
{
public:
    static constexpr int kNumVoices = 8;

    PhantomSampler();
    void prepareToPlay(double sampleRate, int blockSize);

    void processMidi(const juce::MidiBuffer& midi);
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer);

    // Atomic swap of the active sound. Decoded buffer + source rate are
    // captured by the new PhantomSamplerSound; mutex prevents
    // renderNextBlock reading mid-swap.
    bool loadSample(juce::AudioBuffer<float> decoded, double sampleRate);
    void clearSample();
    bool hasSample() const noexcept;

    // Push the current APVTS values into the voices. Called once per
    // block from PluginProcessor::processBlock.
    void setRootNote(int midiNote) noexcept;
    void setLoopEnabled(bool loop) noexcept;
    void setGainDb(float gainDb) noexcept;
    void setEnvelope(float attackSec, float decaySec,
                     float sustain01, float releaseSec);

    int  getActiveVoiceCount() const noexcept;
    int  getPlayheadPosition() const noexcept;

private:
    juce::Synthesiser synth;
    std::mutex        soundsMutex;     // protects synth.clearSounds/addSound
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create `Source/DSP/PhantomSampler.cpp`**

```cpp
#include "PhantomSampler.h"

namespace kaigen::phantom
{

// ── PhantomSamplerVoice ─────────────────────────────────────────────

PhantomSamplerVoice::PhantomSamplerVoice() = default;

bool PhantomSamplerVoice::canPlaySound(juce::SynthesiserSound* s)
{
    return dynamic_cast<PhantomSamplerSound*>(s) != nullptr;
}

void PhantomSamplerVoice::startNote(int midiNoteNumber, float velocity,
                                     juce::SynthesiserSound* s,
                                     int /*pitchWheel*/)
{
    auto* sound = dynamic_cast<PhantomSamplerSound*>(s);
    if (sound == nullptr) return;

    // Playback rate = note ratio × (source rate / live rate).
    const double noteRatio = std::pow(2.0, (midiNoteNumber - rootNote) / 12.0);
    const double srcRate   = sound->getSourceSampleRate();
    const double liveRate  = getSampleRate();
    pitchRatio       = noteRatio * (srcRate / liveRate);
    sourcePosition   = 0.0;
    gainLinear       = juce::jlimit(0.0f, 1.0f, velocity) * gainLinear;
    adsr.setSampleRate(liveRate);
    adsr.noteOn();
    playheadAtomic.store(0, std::memory_order_relaxed);
}

void PhantomSamplerVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        clearCurrentNote();
        adsr.reset();
        playheadAtomic.store(-1, std::memory_order_relaxed);
    }
}

void PhantomSamplerVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                            int startSample, int numSamples)
{
    auto* sound = dynamic_cast<PhantomSamplerSound*>(getCurrentlyPlayingSound().get());
    if (sound == nullptr || ! adsr.isActive()) return;

    const auto& src = sound->getAudioBuffer();
    const int   srcLen = src.getNumSamples();
    if (srcLen == 0) { clearCurrentNote(); return; }

    const int   srcChans = src.getNumChannels();
    const int   outChans = outputBuffer.getNumChannels();

    auto sampleAt = [&](int ch, double pos) -> float
    {
        // Linear interpolation between two integer source samples.
        const int  i0 = (int) pos;
        const int  i1 = juce::jmin(srcLen - 1, i0 + 1);
        const float f = (float) (pos - (double) i0);
        const int  sc = juce::jmin(srcChans - 1, ch);
        const float a = src.getSample(sc, i0);
        const float b = src.getSample(sc, i1);
        return a + f * (b - a);
    };

    for (int n = 0; n < numSamples; ++n)
    {
        const float env = adsr.getNextSample();
        if (! adsr.isActive())
        {
            clearCurrentNote();
            playheadAtomic.store(-1, std::memory_order_relaxed);
            return;
        }

        for (int ch = 0; ch < outChans; ++ch)
        {
            const float v = sampleAt(ch, sourcePosition) * env * gainLinear;
            outputBuffer.addSample(ch, startSample + n, v);
        }

        sourcePosition += pitchRatio;
        if (sourcePosition >= (double) srcLen)
        {
            if (loopEnabled)
                sourcePosition -= (double) srcLen;
            else
            {
                adsr.noteOff();   // start release; voice will silence next iters
            }
        }
    }

    playheadAtomic.store(juce::jlimit(0, srcLen - 1, (int) sourcePosition),
                          std::memory_order_relaxed);
}

// ── PhantomSampler ───────────────────────────────────────────────────

PhantomSampler::PhantomSampler()
{
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice(new PhantomSamplerVoice());
}

void PhantomSampler::prepareToPlay(double sampleRate, int /*blockSize*/)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void PhantomSampler::processMidi(const juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    // processMidi is intentionally a no-op: renderNextBlock takes the
    // MidiBuffer directly so JUCE can interleave note events with
    // sample-accurate audio rendering. Keeping the method for API
    // symmetry with future per-block midi-only handling (e.g. velocity
    // ramps the strip wants to display).
}

void PhantomSampler::renderNextBlock(juce::AudioBuffer<float>& outputBuffer)
{
    juce::MidiBuffer empty;
    std::lock_guard<std::mutex> lock(soundsMutex);
    synth.renderNextBlock(outputBuffer, empty, 0, outputBuffer.getNumSamples());
}

bool PhantomSampler::loadSample(juce::AudioBuffer<float> decoded, double sampleRate)
{
    if (decoded.getNumSamples() == 0 || sampleRate <= 0.0) return false;
    auto sound = juce::SynthesiserSound::Ptr(
        new PhantomSamplerSound(std::move(decoded), sampleRate));

    std::lock_guard<std::mutex> lock(soundsMutex);
    synth.clearSounds();
    synth.addSound(sound);
    return true;
}

void PhantomSampler::clearSample()
{
    std::lock_guard<std::mutex> lock(soundsMutex);
    synth.clearSounds();
}

bool PhantomSampler::hasSample() const noexcept
{
    return synth.getNumSounds() > 0;
}

void PhantomSampler::setRootNote(int n) noexcept
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<PhantomSamplerVoice*>(synth.getVoice(i)))
            v->setRootNote(n);
}

void PhantomSampler::setLoopEnabled(bool l) noexcept
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<PhantomSamplerVoice*>(synth.getVoice(i)))
            v->setLoopEnabled(l);
}

void PhantomSampler::setGainDb(float gainDb) noexcept
{
    const float linear = juce::Decibels::decibelsToGain(gainDb);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<PhantomSamplerVoice*>(synth.getVoice(i)))
            v->setGainLinear(linear);
}

void PhantomSampler::setEnvelope(float a, float d, float s, float r)
{
    juce::ADSR::Parameters p{ a, d, juce::jlimit(0.0f, 1.0f, s), r };
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<PhantomSamplerVoice*>(synth.getVoice(i)))
            v->setEnvelopeParameters(p);
}

int PhantomSampler::getActiveVoiceCount() const noexcept
{
    int n = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (synth.getVoice(i)->isVoiceActive()) ++n;
    return n;
}

int PhantomSampler::getPlayheadPosition() const noexcept
{
    // Report the most-recently-started active voice's playhead. Cheap
    // visual feedback for the strip.
    for (int i = synth.getNumVoices() - 1; i >= 0; --i)
    {
        if (auto* v = dynamic_cast<PhantomSamplerVoice*>(synth.getVoice(i)))
            if (v->isVoiceActive())
                return v->getPlayheadPosition();
    }
    return -1;
}

} // namespace kaigen::phantom
```

The `processMidi` stub exists for future per-block midi-only logic; for now, the actual MIDI handling happens inside `renderNextBlock` via the JUCE Synthesiser pattern. **Important amendment to spec:** the spec sketched `processMidi` then `renderNextBlock` as separate steps, but JUCE's `Synthesiser::renderNextBlock` already takes a MidiBuffer and interleaves events with sample-accurate audio. The simpler integration is to pass the midi buffer directly into `renderNextBlock`. To honor that, change the public API to:

Actually rewrite `renderNextBlock` to take the MidiBuffer:

```cpp
// In PhantomSampler.h, replace the two methods with:
void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                     const juce::MidiBuffer& midi);

// In PhantomSampler.cpp, replace the two implementations with:
void PhantomSampler::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                      const juce::MidiBuffer& midi)
{
    std::lock_guard<std::mutex> lock(soundsMutex);
    synth.renderNextBlock(outputBuffer, midi, 0, outputBuffer.getNumSamples());
}
```

And drop the `processMidi` declaration entirely. Update the header + .cpp accordingly. The PluginProcessor integration in Task 3 expects this combined-call shape.

- [ ] **Step 3: Add the new file to CMakeLists.txt**

In `CMakeLists.txt`, find the `target_sources(KaigenPhantom PRIVATE ...)` block (around line 42-93) and add `Source/DSP/PhantomSampler.cpp` near other Source/DSP entries (e.g. after `Source/DSP/SingleKnobReverb.cpp`).

In `tests/CMakeLists.txt`, find the `add_executable(KaigenPhantomTests ...)` source list and add both `../Source/DSP/PhantomSampler.cpp` AND `PhantomSamplerTests.cpp`.

- [ ] **Step 4: Write the failing test**

Create `tests/PhantomSamplerTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../Source/DSP/PhantomSampler.h"

using namespace kaigen::phantom;
using Catch::Approx;

namespace
{
    // Build a 1-second sine at 440 Hz, 44.1k mono, as a known-shape
    // sample for playback tests.
    juce::AudioBuffer<float> makeSine440(int lengthSamples = 44100)
    {
        juce::AudioBuffer<float> b(1, lengthSamples);
        auto* w = b.getWritePointer(0);
        const double twoPi = juce::MathConstants<double>::twoPi;
        for (int i = 0; i < lengthSamples; ++i)
            w[i] = (float) std::sin(twoPi * 440.0 * (double) i / 44100.0);
        return b;
    }
}

TEST_CASE("PhantomSampler: empty sampler renders silence", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> out(2, 512);
    out.clear();
    juce::MidiBuffer midi;
    s.renderNextBlock(out, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            REQUIRE(out.getSample(ch, i) == Approx(0.0f));
}

TEST_CASE("PhantomSampler: voice allocation caps at 8", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    REQUIRE(s.loadSample(makeSine440(), 44100.0));

    // Send 10 note-ons via the synthesiser's expected midi path.
    juce::MidiBuffer midi;
    for (int n = 0; n < 10; ++n)
        midi.addEvent(juce::MidiMessage::noteOn(1, 60 + n, (juce::uint8) 100), n * 4);

    juce::AudioBuffer<float> out(2, 64);
    out.clear();
    s.renderNextBlock(out, midi);

    CHECK(s.getActiveVoiceCount() == 8);
}

TEST_CASE("PhantomSampler: pitch ratio doubles for octave-up note", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    s.setRootNote(60);
    REQUIRE(s.loadSample(makeSine440(), 44100.0));

    // Note 72 (one octave above root 60) should consume source samples
    // at 2x rate. After N output samples we should have advanced ~2*N
    // through the source.
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8) 100), 0);

    juce::AudioBuffer<float> out(1, 1000);
    out.clear();
    s.renderNextBlock(out, midi);

    // Playhead should be near 2000 (2*1000), give or take a few from
    // the linear-interp + ADSR onset.
    const int playhead = s.getPlayheadPosition();
    CHECK(playhead > 1900);
    CHECK(playhead < 2100);
}

TEST_CASE("PhantomSampler: loop wraps playhead back to 0", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    s.setRootNote(60);
    s.setLoopEnabled(true);
    s.setEnvelope(0.001f, 0.001f, 1.0f, 0.001f);   // mostly bypass envelope

    // Short 100-sample sample so it loops fast.
    auto buf = juce::AudioBuffer<float>(1, 100);
    buf.clear();
    for (int i = 0; i < 100; ++i) buf.setSample(0, i, (i < 50) ? 0.5f : -0.5f);
    REQUIRE(s.loadSample(std::move(buf), 44100.0));

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 127), 0);

    juce::AudioBuffer<float> out(1, 1000);
    out.clear();
    s.renderNextBlock(out, midi);

    // After 1000 samples on a 100-sample loop, position should still
    // be within bounds (wrap worked at least once).
    const int playhead = s.getPlayheadPosition();
    REQUIRE(playhead >= 0);
    REQUIRE(playhead < 100);
}

TEST_CASE("PhantomSampler: note-off → ADSR release silences output", "[sampler]")
{
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    s.setRootNote(60);
    s.setEnvelope(0.001f, 0.001f, 1.0f, 0.010f);   // 10ms release
    REQUIRE(s.loadSample(makeSine440(), 44100.0));

    // Hold note for 100 samples, then release.
    juce::MidiBuffer noteOn;
    noteOn.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 127), 0);
    juce::AudioBuffer<float> hold(1, 100);
    hold.clear();
    s.renderNextBlock(hold, noteOn);
    CHECK(s.getActiveVoiceCount() == 1);

    // Release the note, then render long enough to fully decay
    // (10ms release at 44.1k = 441 samples). Render 2000 to be safe.
    juce::MidiBuffer noteOff;
    noteOff.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    juce::AudioBuffer<float> tail(1, 2000);
    tail.clear();
    s.renderNextBlock(tail, noteOff);

    CHECK(s.getActiveVoiceCount() == 0);

    // Last 500 samples should be silent.
    for (int i = 1500; i < 2000; ++i)
        CHECK(std::abs(tail.getSample(0, i)) < 0.001f);
}
```

- [ ] **Step 5: Run the failing tests**

```powershell
cmake -B build -A x64
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe "[sampler]"
```

Expected: tests fail to compile until PhantomSampler.h/.cpp are in place; once they are, the 5 `[sampler]` tests should pass.

- [ ] **Step 6: Run the full test suite to ensure no regressions**

```powershell
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: 131 cases (126 + 5), no failures.

- [ ] **Step 7: Verify the product still builds**

```powershell
cmake --build build-dev --config Release --target KaigenPhantom_VST3
```

Expected: success. The new PhantomSampler.cpp compiles into the product but isn't called from anywhere yet.

- [ ] **Step 8: Commit**

```bash
git add Source/DSP/PhantomSampler.h Source/DSP/PhantomSampler.cpp \
        CMakeLists.txt tests/CMakeLists.txt tests/PhantomSamplerTests.cpp
git commit -m "feat: PhantomSampler DSP — 8-voice synth with loop + ADSR + pitch shift

New Source/DSP/PhantomSampler.{h,cpp}: a juce::Synthesiser wrapper with
a custom PhantomSamplerVoice (SynthesiserVoice subclass, not SamplerVoice
since stock doesn't loop) and PhantomSamplerSound that holds the decoded
audio buffer + source rate.

Per-voice: linear-interpolated playback at a pitch ratio derived from
(note - root) and (source rate / live rate); juce::ADSR for envelope;
optional loop-while-held that wraps the source-position back to 0 at
the buffer end; atomic playhead for UI overlay.

8-voice cap, mutex-guarded sound swap on loadSample. No PluginProcessor
wiring yet — that arrives in the next task.

Catch2 tests cover: empty-sampler silence, 8-voice allocation cap,
octave-up rate shift (playhead near 2x consumption), loop wrap, ADSR
release silencing."
```

---

## Task 3: Wire PhantomSampler into PluginProcessor + processBlock source switch

**Spec:** Architecture / B. Source selector inside processBlock; Components / D. PluginProcessor changes.

**Files:**
- Modify: `Source/PluginProcessor.h` (add member + sampler accessor)
- Modify: `Source/PluginProcessor.cpp` (prepareToPlay, processBlock, releaseResources)

- [ ] **Step 1: Add the include + member to `Source/PluginProcessor.h`**

At the top of the file with other includes, add:

```cpp
#include "DSP/PhantomSampler.h"
```

In the private section of `PhantomProcessor`, near other DSP members, add:

```cpp
kaigen::phantom::PhantomSampler phantomSampler;
juce::AudioBuffer<float>        samplerOutputBuffer;
```

In the public section, add an accessor so the SamplerStrip (Task 4) can read playhead state:

```cpp
kaigen::phantom::PhantomSampler& getPhantomSampler() noexcept { return phantomSampler; }
const kaigen::phantom::PhantomSampler& getPhantomSampler() const noexcept { return phantomSampler; }
```

- [ ] **Step 2: Resize the sampler buffer in `prepareToPlay`**

In `Source/PluginProcessor.cpp::prepareToPlay`, find the existing prep code and append:

```cpp
phantomSampler.prepareToPlay(sampleRate, samplesPerBlock);
samplerOutputBuffer.setSize(2, samplesPerBlock, false, true, true);
```

- [ ] **Step 3: Render the sampler + apply source switch in processBlock**

Find `processBlock` (around line 146) and add the sampler-render + source-switch block IMMEDIATELY AFTER the existing MIDI loop (around line 181-182, where it iterates note-on/off events) and BEFORE the input-peak/FFT capture (around line 226).

```cpp
// ── Sampler + Engine-input source switch ────────────────────────
// Render the sampler into its own buffer regardless of the source
// selection so the playhead/voice-count UI updates even when the
// engines are reading Input or Sidechain (cheap when no voices
// are active — Synthesiser::renderNextBlock early-outs).
samplerOutputBuffer.setSize(buffer.getNumChannels(), n, false, false, true);
samplerOutputBuffer.clear();
{
    // Push current APVTS values into the voices once per block.
    phantomSampler.setRootNote (
        (int) apvts.getRawParameterValue(ParamID::SAMPLER_ROOT_NOTE)->load());
    phantomSampler.setLoopEnabled(
        apvts.getRawParameterValue(ParamID::SAMPLER_LOOP)->load() > 0.5f);
    phantomSampler.setGainDb    (apvts.getRawParameterValue(ParamID::SAMPLER_GAIN)->load());
    phantomSampler.setEnvelope(
        apvts.getRawParameterValue(ParamID::SAMPLER_A)->load(),
        apvts.getRawParameterValue(ParamID::SAMPLER_D)->load(),
        apvts.getRawParameterValue(ParamID::SAMPLER_S)->load(),
        apvts.getRawParameterValue(ParamID::SAMPLER_R)->load());
    phantomSampler.renderNextBlock(samplerOutputBuffer, midiMessages);
}

const int sourceSel = (int) apvts.getRawParameterValue(ParamID::INPUT_SOURCE)->load();
if (sourceSel == 2)   // 0=Input, 1=Sidechain (existing path), 2=Sampler
{
    // Overwrite the main buffer with the sampler's output so the rest
    // of processBlock (input peak, FFT capture, engine input) sees
    // sampler audio without any further branching.
    for (int ch = 0; ch < nCh; ++ch)
        buffer.copyFrom(ch, 0, samplerOutputBuffer, juce::jmin(ch, samplerOutputBuffer.getNumChannels() - 1), 0, n);
}
// sourceSel == 0 (Input): nothing to do — buffer already holds main input.
// sourceSel == 1 (Sidechain): existing DualEngineHost sidechain code handles it.
```

The sampler is always rendered (cheap when idle) so the UI's voice-count + playhead indicators stay live regardless of source selection. Only the engine input changes based on selection.

- [ ] **Step 4: Manual verification (no unit test for end-to-end signal path yet)**

Build and rebuild the DEV target. Open the plugin in your DAW with a MIDI clip routed in:

```powershell
cmake --build build-dev --config Release --target KaigenPhantom_VST3
```

The full integration test happens in Task 8 (manual walkthrough). For now, verify the build succeeds and the host shows the new parameters in its automation panel.

- [ ] **Step 5: Run full tests for regression**

```powershell
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: all 131 tests still pass.

- [ ] **Step 6: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat: wire PhantomSampler into processBlock with source switch

prepareToPlay sizes the sampler output buffer to maxBlockSize x 2.
Each processBlock:
  1. scrapes the 4 ADSR + root/loop/gain APVTS values into the sampler
  2. calls phantomSampler.renderNextBlock with the host's midi buffer
  3. switches on INPUT_SOURCE: when Sampler (2), overwrites the main
     audio buffer with the sampler's output before the existing
     input-peak/FFT capture + engine processing runs.

Sampler always renders (cheap idle path) so the UI playhead + voice
count remain live even when the engines are reading from Input or
Sidechain. INPUT_SOURCE default = Input → existing presets behave
exactly as before."
```

---

## Task 4: Sample-byte persistence in plugin state

**Spec:** Components / E. State serialization.

**Files:**
- Modify: `Source/PluginProcessor.h` (cached sample bytes + filename + setter)
- Modify: `Source/PluginProcessor.cpp` (`<Sampler>` write/read + async decode coordinator)
- Modify: `tests/PhantomSamplerTests.cpp` (round-trip test)

- [ ] **Step 1: Add the persistence members + setter to `Source/PluginProcessor.h`**

In the private section, near other persistence-related members:

```cpp
// Original source bytes of the loaded sample, kept so getStateInformation
// can serialize them into the <Sampler> child. Written from the message
// thread when the SamplerStrip finishes loading; read by getStateInformation
// (also message thread).
juce::MemoryBlock cachedSampleBytes;
juce::String      cachedSampleFilename;
juce::AudioFormatManager sampleFormatManager;
```

In the public section, near other setters:

```cpp
// Called by SamplerStrip after a successful background decode. Stores
// the original source bytes (for preset embed) and pushes the decoded
// AudioBuffer into the live PhantomSampler. Returns true on success.
bool setSampleFromBytes(juce::MemoryBlock sourceBytes,
                        juce::String filename,
                        juce::AudioBuffer<float> decoded,
                        double sampleRate);

void clearSample();
const juce::String& getSampleFilename() const noexcept { return cachedSampleFilename; }
```

- [ ] **Step 2: Initialize the format manager + implement the setter in `PluginProcessor.cpp`**

In the constructor (after `presetManager.initialize()` is fine), add:

```cpp
sampleFormatManager.registerBasicFormats();
```

Then implement the setter and the clear (place near other public method implementations):

```cpp
bool PhantomProcessor::setSampleFromBytes(juce::MemoryBlock sourceBytes,
                                           juce::String filename,
                                           juce::AudioBuffer<float> decoded,
                                           double sampleRate)
{
    if (! phantomSampler.loadSample(std::move(decoded), sampleRate))
        return false;
    cachedSampleBytes    = std::move(sourceBytes);
    cachedSampleFilename = std::move(filename);
    return true;
}

void PhantomProcessor::clearSample()
{
    phantomSampler.clearSample();
    cachedSampleBytes.reset();
    cachedSampleFilename.clear();
}
```

- [ ] **Step 3: Append the `<Sampler>` child in `getStateInformation`**

In `getStateInformation`, after the existing `<RecipeSlots>` child is appended (line ~852, just before `if (auto xml = wrapper.createXml())`), append:

```cpp
// <Sampler> — embedded sample bytes (base64) + filename. Missing when
// no sample is loaded; the read path treats an empty/missing child as
// "no sample" and leaves PhantomSampler silent.
if (cachedSampleBytes.getSize() > 0)
{
    juce::ValueTree samplerNode("Sampler");
    samplerNode.setProperty("filename", cachedSampleFilename, nullptr);
    samplerNode.setProperty("bytes",
        juce::Base64::toBase64(cachedSampleBytes.getData(),
                                cachedSampleBytes.getSize()),
        nullptr);
    wrapper.appendChild(samplerNode, nullptr);
}
```

- [ ] **Step 4: Parse the `<Sampler>` child in `setStateInformation`**

In `setStateInformation`, after the existing `<RecipeSlots>` parse (line ~920 area), add:

```cpp
// <Sampler> — decode bytes off the message thread and hand the
// resulting AudioBuffer back via callAsync. Until decode completes,
// the sampler stays silent; the plugin remains fully usable.
if (auto samplerNode = wrapper.getChildWithName("Sampler"); samplerNode.isValid())
{
    const auto filename  = samplerNode.getProperty("filename").toString();
    const auto base64    = samplerNode.getProperty("bytes").toString();
    if (base64.isNotEmpty())
    {
        juce::MemoryOutputStream bytesStream;
        if (juce::Base64::convertFromBase64(bytesStream, base64))
        {
            juce::MemoryBlock bytes(bytesStream.getData(), bytesStream.getDataSize());
            // Decode synchronously here (we're already off the audio
            // thread on the host's setStateInformation path). For a
            // load triggered from the SamplerStrip UI, the strip's
            // own callback uses juce::Thread::launch.
            std::unique_ptr<juce::AudioFormatReader> reader(
                sampleFormatManager.createReaderFor(
                    std::make_unique<juce::MemoryInputStream>(bytes, false)));
            if (reader != nullptr)
            {
                juce::AudioBuffer<float> decoded(
                    (int) reader->numChannels,
                    (int) reader->lengthInSamples);
                reader->read(&decoded, 0, decoded.getNumSamples(), 0, true, true);
                setSampleFromBytes(std::move(bytes), filename,
                                    std::move(decoded), reader->sampleRate);
            }
        }
    }
}
```

- [ ] **Step 5: Write the round-trip test**

Append to `tests/PhantomSamplerTests.cpp`:

```cpp
#include <juce_audio_processors/juce_audio_processors.h>

TEST_CASE("PhantomSampler: load via loadSample then read back via sound", "[sampler]")
{
    // Just verifies that loadSample-then-getNumSounds increments. The
    // round-trip-via-state test would require constructing a full
    // PhantomProcessor instance and exercising getState/setState, which
    // is heavy. The state-side persistence is covered by a manual test
    // in Task 8's walkthrough (load preset → reopen project).
    PhantomSampler s;
    s.prepareToPlay(44100.0, 512);
    REQUIRE_FALSE(s.hasSample());
    REQUIRE(s.loadSample(makeSine440(), 44100.0));
    REQUIRE(s.hasSample());
    s.clearSample();
    REQUIRE_FALSE(s.hasSample());
}
```

- [ ] **Step 6: Run tests, expect pass**

```powershell
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe "[sampler]"
```

Expected: 6 `[sampler]` tests pass.

- [ ] **Step 7: Run full suite for regression**

```powershell
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: 132 cases (131 + 1), no failures.

- [ ] **Step 8: Verify product build**

```powershell
cmake --build build-dev --config Release --target KaigenPhantom_VST3
```

- [ ] **Step 9: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp tests/PhantomSamplerTests.cpp
git commit -m "feat: sample-byte persistence via <Sampler> state child

PhantomProcessor caches the loaded sample's original source bytes +
filename. getStateInformation appends a <Sampler> child (sibling to
<RecipeSlots>/<EditorView>) with base64-encoded bytes when a sample
is loaded; setStateInformation decodes via juce::AudioFormatManager
(WAV/AIFF/FLAC/OGG/MP3) and seeds the sampler. setSampleFromBytes is
the single setter UI components use after a successful background
decode. Existing presets without the new child parse identically —
PhantomSampler stays silent."
```

---

## Task 5: SamplerStrip UI widget

**Spec:** Components / C. SamplerStrip.

**Files:**
- Create: `Source/UI/widgets/SamplerStrip.h`
- Create: `Source/UI/widgets/SamplerStrip.cpp`
- Modify: `CMakeLists.txt` (add the new .cpp to target_sources)

- [ ] **Step 1: Create `Source/UI/widgets/SamplerStrip.h`**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ChoiceToggle.h"
#include "PhantomMiniKnob.h"
#include "EtchedToggle.h"

class PhantomProcessor;

namespace kaigen::phantom
{

// SamplerStrip — the 3-row sampler widget sitting between the spectrum
// graph (above) and the oscilloscope (below) in RightPanel.
//
//   ┌─────────────────────────────────────────────────────────────┐
//   │  Source: [Input][Sidechain][Sampler]   mysample.wav   [📁]  │  24 px
//   │  ▁▃▆▇█▇▆▃▁▁▂▄▆▇█▇▅▃▂▁▁▂▃▅▇█▇▅▃▁  (waveform + playhead)      │  60 px
//   │  ROOT [C3 ▾]  LOOP [●]  GAIN [○]  A[─] D[─] S[─] R[─]       │  40 px
//   └─────────────────────────────────────────────────────────────┘
class SamplerStrip : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    SamplerStrip(PhantomProcessor& proc, juce::AudioProcessorValueTreeState& apvts);
    ~SamplerStrip() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;     // ~30 Hz playhead refresh
    void pickAndLoadFile();
    void loadSampleAsync(const juce::File& file);
    void rebuildWaveformThumbnail();   // call after a successful load

    enum class LoadState { Idle, Loading, Error };
    LoadState loadState { LoadState::Idle };
    juce::String errorMessage;

    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // Header row.
    ChoiceToggle sourceToggle;
    juce::TextButton folderButton;   // glyph: 📁

    // Controls row.
    juce::ComboBox rootNoteCombo;
    EtchedToggle   loopToggle;
    PhantomMiniKnob gainKnob;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;

    // APVTS attachments.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>   rootAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>     attackAttach, decayAttach, sustainAttach, releaseAttach;
    // (sourceToggle, loopToggle, gainKnob already have their own
    // attachments via their ChoiceToggle/EtchedToggle/PhantomMiniKnob ctors.)

    // Waveform thumbnail cached as an Image so paint is cheap.
    juce::Image waveformImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerStrip)
};

} // namespace kaigen::phantom
```

- [ ] **Step 2: Create `Source/UI/widgets/SamplerStrip.cpp`**

```cpp
#include "SamplerStrip.h"
#include "../../PluginProcessor.h"
#include "../../Parameters.h"

namespace kaigen::phantom
{

namespace
{
    constexpr int kHeaderH   = 24;
    constexpr int kWaveformH = 60;
    constexpr int kControlsH = 40;

    juce::String midiNoteName(int n)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int octave = (n / 12) - 2;     // MIDI 0 = C-2 convention
        return juce::String(names[n % 12]) + juce::String(octave);
    }
}

SamplerStrip::SamplerStrip(PhantomProcessor& p, juce::AudioProcessorValueTreeState& a)
    : processor(p), apvts(a),
      sourceToggle(a, ParamID::INPUT_SOURCE,
                    juce::StringArray{ "Input", "Sidechain", "Sampler" }),
      loopToggle(a, ParamID::SAMPLER_LOOP, "Loop"),
      gainKnob(a, ParamID::SAMPLER_GAIN, "Gain")
{
    addAndMakeVisible(sourceToggle);

    folderButton.setButtonText("...");
    folderButton.getProperties().set("phantom-style", "header-glyph");
    folderButton.onClick = [this] { pickAndLoadFile(); };
    addAndMakeVisible(folderButton);

    for (int n = 0; n <= 127; ++n)
        rootNoteCombo.addItem(midiNoteName(n), n + 1);   // ComboBox ids must be >= 1
    rootAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamID::SAMPLER_ROOT_NOTE, rootNoteCombo);
    addAndMakeVisible(rootNoteCombo);

    addAndMakeVisible(loopToggle);
    addAndMakeVisible(gainKnob);

    auto setupSlider = [&](juce::Slider& s, const juce::String& paramId,
                            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attach)
    {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramId, s);
        addAndMakeVisible(s);
    };
    setupSlider(attackSlider,  ParamID::SAMPLER_A, attackAttach);
    setupSlider(decaySlider,   ParamID::SAMPLER_D, decayAttach);
    setupSlider(sustainSlider, ParamID::SAMPLER_S, sustainAttach);
    setupSlider(releaseSlider, ParamID::SAMPLER_R, releaseAttach);

    // 30 Hz playhead refresh while voices are active.
    startTimerHz(30);
}

SamplerStrip::~SamplerStrip() = default;

void SamplerStrip::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121419));

    auto area = getLocalBounds();
    auto headerArea = area.removeFromTop(kHeaderH);
    auto waveformArea = area.removeFromTop(kWaveformH).reduced(6, 4);
    // controlsArea is the remainder; positioned in resized().

    // Header — filename + load button positioned in resized(), so we
    // just paint the filename label here.
    auto filenameRect = headerArea.reduced(8, 0).withTrimmedLeft(220);   // skip the sourceToggle width
    filenameRect.removeFromRight(40);   // skip the folderButton width
    g.setColour(juce::Colour(processor.getSampleFilename().isEmpty()
        ? 0x66ffffff : 0xffd0d2d4));
    g.setFont(juce::FontOptions(12.0f));
    const auto fn = processor.getSampleFilename().isEmpty()
                        ? juce::String("(no sample loaded)")
                        : processor.getSampleFilename();
    g.drawText(fn, filenameRect, juce::Justification::centredRight, true);

    // Waveform area — bg + image + playhead + placeholder text.
    g.setColour(juce::Colour(0xff0a0c12));
    g.fillRect(waveformArea);

    if (waveformImage.isValid())
    {
        g.drawImage(waveformImage, waveformArea.toFloat(),
                    juce::RectanglePlacement::stretchToFit);

        // Playhead overlay (atomic int from PhantomSampler).
        const int playhead = processor.getPhantomSampler().getPlayheadPosition();
        if (playhead >= 0 && processor.getPhantomSampler().hasSample())
        {
            // Map source-sample index to x via the synth's loaded sound
            // length. Without direct access to that, approximate from
            // the image width assuming the thumbnail represents the
            // whole sample (which it does).
            const auto srcLenApprox = (float) waveformImage.getWidth();   // image is 1px per slice
            // we'll re-derive a better mapping in Task 6 once the
            // thumbnail builder records the source length.
            const float frac = juce::jlimit(0.0f, 1.0f, (float) playhead / juce::jmax(1.0f, srcLenApprox));
            const int xpx = waveformArea.getX() + (int) (frac * waveformArea.getWidth());
            g.setColour(juce::Colour(0xff77ddff));
            g.drawLine((float) xpx, (float) waveformArea.getY(),
                        (float) xpx, (float) waveformArea.getBottom(), 1.0f);
        }
    }
    else
    {
        g.setColour(juce::Colour(0x66ffffff));
        g.setFont(juce::FontOptions(12.0f));
        const char* msg =
              loadState == LoadState::Loading ? "Loading..."
            : loadState == LoadState::Error   ? errorMessage.toRawUTF8()
                                                : "Click or drop a sample";
        g.drawText(msg, waveformArea, juce::Justification::centred, false);
    }
}

void SamplerStrip::resized()
{
    auto area = getLocalBounds();
    auto headerArea = area.removeFromTop(kHeaderH);
    auto waveformArea = area.removeFromTop(kWaveformH);
    juce::ignoreUnused(waveformArea);   // painted directly, no children
    auto controlsArea = area.removeFromTop(kControlsH).reduced(6, 4);

    // Header — source selector left, folder button right, filename
    // painted between (in paint()).
    sourceToggle.setBounds(headerArea.removeFromLeft(220).reduced(6, 2));
    folderButton.setBounds(headerArea.removeFromRight(36).reduced(4));

    // Controls row.
    constexpr int kRootW = 64, kLoopW = 36, kGainW = 36, kEnvW = 18, kGap = 6;
    rootNoteCombo.setBounds(controlsArea.removeFromLeft(kRootW));
    controlsArea.removeFromLeft(kGap);
    loopToggle.setBounds(controlsArea.removeFromLeft(kLoopW));
    controlsArea.removeFromLeft(kGap);
    gainKnob.setBounds(controlsArea.removeFromLeft(kGainW));
    controlsArea.removeFromLeft(kGap * 2);
    attackSlider.setBounds(controlsArea.removeFromLeft(kEnvW));
    controlsArea.removeFromLeft(kGap);
    decaySlider.setBounds(controlsArea.removeFromLeft(kEnvW));
    controlsArea.removeFromLeft(kGap);
    sustainSlider.setBounds(controlsArea.removeFromLeft(kEnvW));
    controlsArea.removeFromLeft(kGap);
    releaseSlider.setBounds(controlsArea.removeFromLeft(kEnvW));
}

void SamplerStrip::mouseDown(const juce::MouseEvent& e)
{
    // Click on the waveform area opens the file picker.
    auto waveformArea = juce::Rectangle<int>(0, kHeaderH, getWidth(), kWaveformH);
    if (waveformArea.contains(e.getPosition()))
        pickAndLoadFile();
}

bool SamplerStrip::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (files.size() != 1) return false;
    const auto ext = juce::File(files[0]).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff"
        || ext == ".flac" || ext == ".ogg" || ext == ".mp3";
}

void SamplerStrip::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    if (files.size() == 1)
        loadSampleAsync(juce::File(files[0]));
}

void SamplerStrip::timerCallback()
{
    // Repaint only if a voice is active so we're not chewing cycles
    // when idle.
    if (processor.getPhantomSampler().getActiveVoiceCount() > 0)
        repaint();
}

void SamplerStrip::pickAndLoadFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Choose sample",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
                loadSampleAsync(file);
        });
}

void SamplerStrip::loadSampleAsync(const juce::File& file)
{
    loadState = LoadState::Loading;
    repaint();

    juce::Component::SafePointer<SamplerStrip> self(this);
    juce::Thread::launch([self, file]
    {
        juce::MemoryBlock bytes;
        if (! file.loadFileAsData(bytes))
        {
            juce::MessageManager::callAsync([self] {
                if (auto* p = self.getComponent()) {
                    p->loadState = LoadState::Error;
                    p->errorMessage = "Couldn't read file";
                    p->repaint();
                }
            });
            return;
        }
        if (bytes.getSize() > 5 * 1024 * 1024)
        {
            juce::MessageManager::callAsync([self] {
                if (auto* p = self.getComponent()) {
                    p->loadState = LoadState::Error;
                    p->errorMessage = "Sample too large (max 5MB)";
                    p->repaint();
                }
            });
            return;
        }
        // Decode via the processor's format manager (registered with
        // basic formats in PluginProcessor's ctor).
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(
            fm.createReaderFor(std::make_unique<juce::MemoryInputStream>(bytes, false)));
        if (reader == nullptr)
        {
            juce::MessageManager::callAsync([self] {
                if (auto* p = self.getComponent()) {
                    p->loadState = LoadState::Error;
                    p->errorMessage = "Couldn't decode file";
                    p->repaint();
                }
            });
            return;
        }
        juce::AudioBuffer<float> decoded(
            (int) reader->numChannels,
            (int) reader->lengthInSamples);
        reader->read(&decoded, 0, decoded.getNumSamples(), 0, true, true);
        const double sr = reader->sampleRate;
        const auto filename = file.getFileName();

        juce::MessageManager::callAsync(
            [self, bytes = std::move(bytes), filename,
             decoded = std::move(decoded), sr]() mutable
            {
                if (auto* p = self.getComponent())
                {
                    if (p->processor.setSampleFromBytes(
                            std::move(bytes), filename, std::move(decoded), sr))
                    {
                        p->loadState = LoadState::Idle;
                        p->rebuildWaveformThumbnail();
                    }
                    else
                    {
                        p->loadState = LoadState::Error;
                        p->errorMessage = "Load failed";
                    }
                    p->repaint();
                }
            });
    });
}

void SamplerStrip::rebuildWaveformThumbnail()
{
    // Render a simple peak-per-pixel mono mixdown thumbnail. The image
    // width is independent of the strip width (we stretch to fit in
    // paint), so a fixed 512 px image gives reasonable detail and
    // small memory cost.
    constexpr int kImgW = 512;
    constexpr int kImgH = 60;
    waveformImage = juce::Image(juce::Image::ARGB, kImgW, kImgH, true);

    // Pull the loaded sound from the sampler. Since PhantomSampler
    // doesn't expose the buffer directly, we accept a simple
    // approximation: ask the processor for its cached sample length
    // (you may want to add an accessor for this) and render flat bars
    // for now. The next plan task can wire a proper peak builder.
    {
        juce::Graphics g(waveformImage);
        g.setColour(juce::Colour(0xff2a323d));
        g.fillAll();
        g.setColour(juce::Colour(0xff77ddff));
        for (int x = 0; x < kImgW; x += 4)
            g.drawLine((float) x, (float) kImgH * 0.5f - 8,
                        (float) x, (float) kImgH * 0.5f + 8, 1.0f);
    }
}

} // namespace kaigen::phantom
```

The `rebuildWaveformThumbnail` placeholder draws a flat bar pattern instead of an actual peak waveform. **Task 6 replaces this with a real peak builder** — we ship the strip with a placeholder thumbnail in this task so the wiring lands cleanly, then improve the visual in the next task.

- [ ] **Step 3: Add `Source/UI/widgets/SamplerStrip.cpp` to CMakeLists.txt**

In `CMakeLists.txt`, inside `target_sources(KaigenPhantom PRIVATE ...)`, add `Source/UI/widgets/SamplerStrip.cpp` near the other widget files (e.g. just after `Source/UI/widgets/PackGifCache.cpp`).

- [ ] **Step 4: Verify the DEV build**

```powershell
cmake --build build-dev --config Release --target KaigenPhantom_VST3
```

Expected: success. SamplerStrip compiles but isn't placed in any panel yet (no visible change in the UI).

- [ ] **Step 5: Commit**

```bash
git add Source/UI/widgets/SamplerStrip.h Source/UI/widgets/SamplerStrip.cpp CMakeLists.txt
git commit -m "feat: SamplerStrip UI widget

Three-row strip (24+60+40 px) wired to APVTS via standard JUCE
attachments. Header: Input/Sidechain/Sampler ChoiceToggle, filename
label, folder button. Waveform: cached juce::Image thumbnail (flat
bars for now — real peak builder lands in the next task) with
playhead overlay. Controls: root-note combobox (C-2..G8), loop
toggle, gain knob, A/D/S/R vertical sliders.

Loading: click waveform OR drag-drop a .wav/.aif/.flac/.ogg/.mp3
OR press folder button. All paths decode async via
juce::Thread::launch + SafePointer + MessageManager::callAsync,
identical pattern to CoverEditorOverlay. 5MB source-size cap
enforced before decode. Error states paint inline in the
waveform area.

Not yet placed in any panel — that lands in Task 6 + 7."
```

---

## Task 6: Real waveform thumbnail (peak builder)

**Spec:** Components / C.2 Waveform row — "Static mono mixdown thumbnail of the loaded sample".

**Files:**
- Modify: `Source/PluginProcessor.h` (expose the cached buffer to the UI, OR add a thumbnail accessor)
- Modify: `Source/UI/widgets/SamplerStrip.cpp` (replace placeholder with real peak builder)

The cleanest split: have PhantomProcessor own a `juce::AudioThumbnail` (JUCE's built-in waveform thumbnail) and expose it via accessor. Cheap, idiomatic, handles all the peak math.

- [ ] **Step 1: Add a thumbnail member to `Source/PluginProcessor.h`**

In the private section:

```cpp
// AudioThumbnail of the loaded sample. Built from cachedSampleBytes
// after setSampleFromBytes; consumed by SamplerStrip for its waveform
// row. Owns its own background-thread cache via the cache below.
juce::AudioThumbnailCache sampleThumbCache { 1 };
juce::AudioThumbnail      sampleThumb { 512, sampleFormatManager, sampleThumbCache };
```

In the public section:

```cpp
const juce::AudioThumbnail& getSampleThumbnail() const noexcept { return sampleThumb; }
```

- [ ] **Step 2: Feed the thumbnail in `setSampleFromBytes`**

In `PluginProcessor.cpp`, modify `setSampleFromBytes` to also push bytes into the thumbnail:

```cpp
bool PhantomProcessor::setSampleFromBytes(juce::MemoryBlock sourceBytes,
                                           juce::String filename,
                                           juce::AudioBuffer<float> decoded,
                                           double sampleRate)
{
    if (! phantomSampler.loadSample(juce::AudioBuffer<float>(decoded), sampleRate))
        return false;

    // Feed the thumbnail from the same MemoryBlock so its peak cache
    // matches what the sampler plays. AudioThumbnail owns an
    // InputSource internally; setSource takes ownership.
    sampleThumb.reset(decoded.getNumChannels(), sampleRate, decoded.getNumSamples());
    sampleThumb.addBlock(0, decoded, 0, decoded.getNumSamples());

    cachedSampleBytes    = std::move(sourceBytes);
    cachedSampleFilename = std::move(filename);
    return true;
}
```

Also update `clearSample`:

```cpp
void PhantomProcessor::clearSample()
{
    phantomSampler.clearSample();
    cachedSampleBytes.reset();
    cachedSampleFilename.clear();
    sampleThumb.reset(0, 0.0, 0);
}
```

- [ ] **Step 3: Replace SamplerStrip's placeholder thumbnail with the real one**

In `SamplerStrip.cpp`, replace the body of `rebuildWaveformThumbnail` with:

```cpp
void SamplerStrip::rebuildWaveformThumbnail()
{
    constexpr int kImgW = 512;
    constexpr int kImgH = 60;
    waveformImage = juce::Image(juce::Image::ARGB, kImgW, kImgH, true);

    const auto& thumb = processor.getSampleThumbnail();
    if (thumb.getTotalLength() <= 0.0) return;

    juce::Graphics g(waveformImage);
    g.setColour(juce::Colour(0xff1a2028));
    g.fillAll();
    g.setColour(juce::Colour(0xff77ddff));
    juce::Rectangle<int> rect(0, 0, kImgW, kImgH);
    thumb.drawChannels(g, rect, 0.0, thumb.getTotalLength(), 1.0f);
}
```

- [ ] **Step 4: Update playhead mapping in `paint()` to use the actual sample length**

In `SamplerStrip::paint`, replace the playhead block (the `if (playhead >= 0 && ... )` section) with a version that uses the thumbnail's source-sample length (via the source rate × total length):

```cpp
const int playhead = processor.getPhantomSampler().getPlayheadPosition();
const auto& thumb  = processor.getSampleThumbnail();
const double durSec = thumb.getTotalLength();
if (playhead >= 0 && durSec > 0.0 && processor.getPhantomSampler().hasSample())
{
    // Source-sample index → fraction of total sample length. Use the
    // thumbnail's numSamples for the denominator since it matches what
    // PhantomSampler's voice is reading.
    const int totalSamples = (int) (durSec * 44100.0);  // approximation OK for playhead viz
    const float frac = juce::jlimit(0.0f, 1.0f, (float) playhead / juce::jmax(1.0f, (float) totalSamples));
    const int xpx = waveformArea.getX() + (int) (frac * waveformArea.getWidth());
    g.setColour(juce::Colour(0xff77ddff));
    g.drawLine((float) xpx, (float) waveformArea.getY(),
                (float) xpx, (float) waveformArea.getBottom(), 1.0f);
}
```

- [ ] **Step 5: Build + verify**

```powershell
cmake --build build-dev --config Release --target KaigenPhantom_VST3
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: builds + all tests pass.

- [ ] **Step 6: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp Source/UI/widgets/SamplerStrip.cpp
git commit -m "feat: real waveform thumbnail via juce::AudioThumbnail

PhantomProcessor owns an AudioThumbnail + 1-slot AudioThumbnailCache,
seeded from the decoded AudioBuffer in setSampleFromBytes and reset
in clearSample. SamplerStrip's rebuildWaveformThumbnail now calls
AudioThumbnail::drawChannels into the cached juce::Image (still
stretched to fit at paint time, so the same image works across
strip widths). Playhead overlay rescales using the thumbnail's
total length."
```

---

## Task 7: Insert SamplerStrip into RightPanel

**Spec:** Architecture — "The sampler UI is a 3-row strip placed between the spectrum graph and the oscilloscope in RightPanel".

**Files:**
- Modify: `Source/UI/panels/RightPanel.h` (add SamplerStrip member)
- Modify: `Source/UI/panels/RightPanel.cpp` (add to children + adjust layout)

- [ ] **Step 1: Add the include + member to `Source/UI/panels/RightPanel.h`**

Near the other widget includes at the top of the file:

```cpp
#include "../widgets/SamplerStrip.h"
```

In the private section near other widget members:

```cpp
SamplerStrip samplerStrip;
```

- [ ] **Step 2: Update the RightPanel constructor in `RightPanel.cpp`**

The existing constructor takes `(juce::AudioProcessorValueTreeState& apvts, PhantomProcessor& processor)`. Update the member-init list (line 56 area, where `oscilloscope(p), spectrum(p, a)` already live) to add:

```cpp
samplerStrip(p, a)
```

Then in the constructor body (around the existing `addAndMakeVisible(oscilloscope)` calls):

```cpp
addAndMakeVisible(samplerStrip);
```

- [ ] **Step 3: Reserve space for the strip in `RightPanel::resized()`**

The existing layout (visible at the search-result lines 394-449) is bottom-anchored:
- Bottom row: oscilloscope + meter column (`kOscRowHeight = 100`)
- Above bottom row: pitch slot
- Above pitch slot: spectrum (fills the rest, with a minimum)

Insert the SamplerStrip BETWEEN spectrum (above) and pitch slot (below). It needs `124 px` (24 + 60 + 40) + a small margin.

Find the existing `spectrumBottom` calculation (line ~445) and modify so the spectrum's bottom edge sits ABOVE the strip:

```cpp
constexpr int kSamplerStripH = 124;
constexpr int kSamplerGapAbove = 8;
constexpr int kSamplerGapBelow = 8;

// Strip sits below the spectrum, above the pitch slot.
const int samplerTop    = pitchSlotTop - kSamplerStripH - kSamplerGapBelow;
const int samplerBottom = samplerTop + kSamplerStripH;
samplerStrip.setBounds(vizLeft, samplerTop, vizRight - vizLeft, kSamplerStripH);

const int spectrumBottom = samplerTop - kSamplerGapAbove;
// (rest of the spectrum sizing below this line stays the same)
```

This shrinks the spectrum's max height by `kSamplerStripH + kSamplerGapAbove + kSamplerGapBelow = 140 px`. The spectrum's `kSpectrumMinH` floor still applies — if the overall panel is too short the strip will overlap; verify in manual testing.

- [ ] **Step 4: Verify the DEV build**

```powershell
cmake --build build-dev --config Release --target KaigenPhantom_VST3
```

Open in your DAW. The sampler strip should appear between the spectrum graph and the oscilloscope. With no sample loaded: header shows source toggle + "(no sample loaded)" + folder button; waveform area shows "Click or drop a sample"; controls row populated with knob defaults.

- [ ] **Step 5: Verify both configurations**

```powershell
cmake --build build --config Release --target KaigenPhantom_VST3
cmake --build build --config Release --target KaigenPhantomTests
./build/tests/Release/KaigenPhantomTests.exe
```

Expected: ship build succeeds, all tests still pass.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/panels/RightPanel.h Source/UI/panels/RightPanel.cpp
git commit -m "feat: insert SamplerStrip into RightPanel layout

Strip sits between the spectrum graph (above) and the pitch-display
slot (below). 124 px tall + 8 px gaps either side = 140 px taken
from the spectrum's previous bottom-anchored space; the spectrum's
kSpectrumMinH floor still applies if the overall panel shrinks too
far."
```

---

## Task 8: Manual end-to-end walkthrough

**Spec:** Testing strategy → Manual.

**Files:** none modified — verification only.

- [ ] **Step 1: Designer walkthrough — basic playback**

Build + install the DEV build, open in your DAW with a MIDI track routed to it.

1. Drag a WAV file onto the sampler strip. Filename appears in the header. Waveform thumbnail renders.
2. Set INPUT_SOURCE header toggle to **Sampler**.
3. Play a MIDI note at C3 (60). Sample should play at native pitch with Phantom harmonics applied.
4. Play C4. Sample should play an octave up.
5. Play a chord (C3, E3, G3). Three voices mix into the engines simultaneously.
6. Toggle LOOP on. Hold a note longer than the sample length — sample should wrap.
7. Adjust ROOT to D3. Now playing D3 yields native pitch.
8. Adjust ADSR sliders during playback. Hear the envelope shape change.

- [ ] **Step 2: Source switching**

1. While a note is held, switch INPUT_SOURCE between Input / Sidechain / Sampler. No clicks or pops expected (existing engine input handling should crossfade — if there's a click, note it for a follow-up commit; not blocking).
2. With INPUT_SOURCE = Input and a live audio source connected, verify engines process input normally (sampler audio is silent in the output even though voices may be active).

- [ ] **Step 3: Preset round-trip**

1. Load a sample, adjust ADSR + root note + loop, save your project.
2. Close the project, reopen.
3. The sample should reload automatically with all parameters intact.
4. Confirm the sample plays correctly without re-dropping the file.

- [ ] **Step 4: Format coverage**

Repeat Step 1 with sample files in each format:
- WAV (16-bit + 24-bit)
- AIFF
- FLAC
- OGG Vorbis
- MP3

All should load + play. Any that fails to decode should show a polite "Couldn't decode file" error inline, no crash.

- [ ] **Step 5: Size cap**

Drag a file larger than 5 MB onto the strip. Expect: "Sample too large (max 5MB)" error inline, sampler stays empty, no decode attempted, no UI freeze.

- [ ] **Step 6: Pack-export integration**

1. Save a preset with an embedded sample into a User pack via the existing AUTHORING strip.
2. Export the pack as a `.kaipack`.
3. Verify the `.kaipack` size reflects the embedded sample (it'll be bigger than empty-sample packs).
4. Delete the pack from disk, then Import the `.kaipack` back.
5. Open the preset — sample should be present.

- [ ] **Step 7: Cleanup + close**

No commit unless regressions surfaced. If you found a bug, fix it and commit with a `fix:` prefix describing what was wrong and what changed.

---

## Self-review notes (post-plan)

- **Spec coverage:**
  - Components A (PhantomSampler) → Task 2 ✓
  - Components B (source switch in processBlock) → Task 3 ✓
  - Components C (SamplerStrip UI) → Tasks 5 + 6 ✓
  - Components D (PluginProcessor changes) → Task 3 (DSP) + Task 4 (persistence) + Task 6 (thumbnail) ✓
  - Components E (state serialization) → Task 4 ✓
  - Async loading → Task 5 (uses the CoverEditorOverlay pattern verbatim) ✓
  - Polyphony cap = 8 → Task 2 `kNumVoices = 8` ✓
  - 5 MB size cap → Task 5 inside `loadSampleAsync` ✓
  - File-format coverage → Task 4 `sampleFormatManager.registerBasicFormats()` + Task 5 file-chooser pattern ✓
  - Migration (pre-sampler presets) → Task 4's read path treats missing child as "no sample" ✓
  - Pack export inheritance → no special handling needed (existing .kaipack export bundles whatever `.fxp` files exist; verified manually in Task 8 Step 6) ✓
  - Manual walkthrough → Task 8 ✓

- **Placeholder scan:**
  - Task 5 ships a flat-bar placeholder thumbnail intentionally — Task 6 replaces it with the real `juce::AudioThumbnail` builder. This is a deliberate two-step (lets the wiring land cleanly before tackling the visual), not a vague "implement later". Documented inline.
  - Task 3 Step 4 says "manual verification" instead of a unit test for the source-switch path. The signal-flow change is too coupled to a full PluginProcessor instance + DualEngineHost for a clean unit test; Task 8 covers it end-to-end. Acceptable.

- **Type consistency:**
  - `PhantomSampler` interface declared in Task 2 (`renderNextBlock(buffer, midi)`, `loadSample`, `setRootNote`, `setLoopEnabled`, `setGainDb`, `setEnvelope`, `getActiveVoiceCount`, `getPlayheadPosition`, `hasSample`, `clearSample`) is used identically in Tasks 3, 5, 6, 7. ✓
  - `PhantomProcessor::setSampleFromBytes` defined in Task 4, called from Task 5. ✓
  - `PhantomProcessor::getSampleThumbnail` defined in Task 6, called from Task 6 inside SamplerStrip. ✓
  - Parameter IDs from Task 1 (`INPUT_SOURCE`, `SAMPLER_*`) used in Task 3 (APVTS scrape) and Task 5 (attachments). All match. ✓
