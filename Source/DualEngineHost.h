// Source/DualEngineHost.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engines/PhantomEngine.h"
#include "MorphCrossfader.h"
#include "Modulation/ModulationEngine.h"
#include <atomic>
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

    /** Inject the modulation engines that intercept per-param value lookup
     *  in syncEngineFromPrefix. Owned by PhantomProcessor; pointers are
     *  non-owning. Pass nullptr for either side to disable modulation
     *  on that engine (default state — must be set after construction
     *  for routings to take effect). */
    void setModulationEngines(kaigen::phantom::ModulationEngine* modA,
                              kaigen::phantom::ModulationEngine* modB) noexcept;

    /** Diagnostic: returns true iff every "a_*" / "b_*" param in the APVTS
     *  layout has a populated entry in the corresponding cache. Used by a
     *  unit test to catch the case where someone adds a per-engine APVTS
     *  param but forgets to register it in `buildParamCache`. Any IDs
     *  found missing are appended to `missing`. Compares by full string
     *  (NOT pointer equality), so it also validates the cache against
     *  drift in the layout-vs-cache invariant. */
    bool validateParamCachesCoverAPVTS(juce::StringArray& missing) const;

private:
    /** Per-engine pre-resolved parameter cache. Each entry holds the full
     *  param ID (e.g. "a_phantom_threshold") AND a direct atomic pointer.
     *  Built once at construction so the audio-thread sync loop does ZERO
     *  string allocation and ZERO APVTS hash lookups per block. The leaf-key
     *  is the const char* literal pointer (string literals dedupe at link
     *  time, so pointer equality is reliable for ParamID::LEAF_* lookups). */
    struct CachedLeaf
    {
        const char*                 leafKey  { nullptr };
        juce::String                fullId;
        std::atomic<float>*         baseAtom { nullptr };
        juce::RangedAudioParameter* param    { nullptr };
    };

    void syncEngineFromPrefix(PhantomEngine& target,
                              const std::vector<CachedLeaf>& cache,
                              kaigen::phantom::ModulationEngine* modEng);

    void buildParamCache(std::vector<CachedLeaf>& cache, const char* prefix);
    const CachedLeaf& findCached(const std::vector<CachedLeaf>& cache, const char* leaf) const noexcept;

    std::vector<CachedLeaf> cacheA;
    std::vector<CachedLeaf> cacheB;

    juce::AudioProcessorValueTreeState& apvts;
    PhantomEngine    engineA, engineB;
    MorphCrossfader  crossfader;

    // Pre-allocated scratch for engine B's output. Engine A processes
    // into aScratch; engine B processes into bScratch; crossfader mixes
    // them into the caller's buffer. aScratch starts as a copy of the
    // input so that A and B both process the same pre-engine signal.
    juce::AudioBuffer<float> aScratch;
    juce::AudioBuffer<float> bScratch;

    std::atomic<kaigen::phantom::ModulationEngine*> modA { nullptr };
    std::atomic<kaigen::phantom::ModulationEngine*> modB { nullptr };
};

} // namespace kaigen::phantom
