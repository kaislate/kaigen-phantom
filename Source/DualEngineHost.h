// Source/DualEngineHost.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engines/PhantomEngine.h"
#include "MorphCrossfader.h"
#include <functional>

namespace kaigen::phantom
{

class DualEngineHost
{
public:
    DualEngineHost(juce::AudioProcessorValueTreeState& apvts);

    void prepareToPlay(double sampleRate, int blockSize, int numChannels);
    void reset();

    /** Process a block. The caller has already handled bypass + input gain
     *  detection sync — `process()` only handles the engine-pair work +
     *  crossfade. The input buffer is duplicated for both engines (each
     *  produces its own output); the output is the crossfaded sum and is
     *  written into `buffer` in place. */
    void process(juce::AudioBuffer<float>& buffer,
                 const juce::AudioBuffer<float>* sidechain = nullptr);

    /** MIDI event forwarding (each engine maintains its own envelope state). */
    void handleMidiNoteOn();
    void handleMidiNoteOff();

    /** Input detection gain forwarding (set on both engines). */
    void setInputDetectionGain(float gainLin);

    /** Engine accessors for UI / oscilloscope / pitch readout. The active
     *  engine is the one currently shown by the editor — for PR 1 always A.
     *  PR 2 will introduce an active-engine getter that varies with tab. */
    PhantomEngine& getEngineA() noexcept { return engineA; }
    PhantomEngine& getEngineB() noexcept { return engineB; }
    PhantomEngine& getActiveEngine() noexcept { return engineA; }   // PR 1: always A
    MorphCrossfader& getCrossfader() noexcept { return crossfader; }

    /** Per-engine output buffers (post-process, pre-crossfader). Caller
     *  may read these for visualization (FFT capture) but must NOT modify
     *  them — they're consumed by the next process() call. Valid only
     *  immediately after process() returns and until the next process()
     *  call. */
    const juce::AudioBuffer<float>& getEngineAOutput() const noexcept { return aScratch; }
    const juce::AudioBuffer<float>& getEngineBOutput() const noexcept { return bScratch; }

private:
    void syncEngineFromPrefix(PhantomEngine& target, const char* prefix);

    juce::AudioProcessorValueTreeState& apvts;
    PhantomEngine    engineA, engineB;
    MorphCrossfader  crossfader;

    // Pre-allocated scratch for engine B's output. Engine A processes
    // into aScratch; engine B processes into bScratch; crossfader mixes
    // them into the caller's buffer. aScratch starts as a copy of the
    // input so that A and B both process the same pre-engine signal.
    juce::AudioBuffer<float> aScratch;
    juce::AudioBuffer<float> bScratch;
};

} // namespace kaigen::phantom
