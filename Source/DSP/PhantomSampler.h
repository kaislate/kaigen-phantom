#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <mutex>
#include <vector>

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
    void setGainLinear(float g) noexcept { baseGainLinear = g; }
    void setStartEnd(float start01, float end01) noexcept
    {
        startFrac = juce::jlimit(0.0f, 1.0f, start01);
        endFrac   = juce::jlimit(startFrac + 0.0001f, 1.0f, end01);
    }
    void setSliceMode(bool slice) noexcept { sliceMode = slice; }
    /** Pointer-to-shared slice table (lives on PhantomSampler).
     *  PhantomSampler keeps it alive; voices read by const reference. */
    void setSliceTable(const std::vector<int>* table) noexcept { slicePoints = table; }

    // For the SamplerStrip playhead overlay. 0..numSamples-1 of the
    // active sample; -1 when voice idle. Read by the message thread,
    // written by the audio thread — atomic.
    int getPlayheadPosition() const noexcept { return playheadAtomic.load(std::memory_order_relaxed); }

private:
    double                pitchRatio       { 1.0 };
    double                sourcePosition   { 0.0 };
    int                   rootNote         { 60 };
    bool                  loopEnabled      { false };
    float                 baseGainLinear { 1.0f };   // settable from outside via setGainLinear
    float                 velocityGain   { 1.0f };   // re-set every startNote
    float                 startFrac      { 0.0f };   // [0,1] of source length
    float                 endFrac        { 1.0f };
    bool                  sliceMode      { false };
    const std::vector<int>* slicePoints  { nullptr };   // owned by PhantomSampler
    int                   sliceEndSample { 0 };         // computed at startNote in slice mode
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

    // Renders the synth output into outputBuffer using the host's midi
    // buffer for sample-accurate note timing. Single combined call —
    // juce::Synthesiser::renderNextBlock interleaves events with audio.
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         const juce::MidiBuffer& midi);

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
    void setStartEnd(float start01, float end01) noexcept;
    void setSliceMode(bool slice) noexcept;

    /** Detects onsets in the loaded sample and stores them as slice
     *  points. Returns the new number of slices (always >= 1; a sample
     *  with no detected transients yields a single full-length slice). */
    int detectSlices();

    /** Replace the slice table with an externally-provided list (used
     *  when restoring from plugin state). Sorted + clamped + dedup'd. */
    void setSliceTable(std::vector<int> slices);

    /** Read-only access to the current slice table. */
    const std::vector<int>& getSliceTable() const noexcept { return sliceTable; }

    int  getActiveVoiceCount() const noexcept;
    int  getPlayheadPosition() const noexcept;

    // Source sample rate of the currently-loaded sample, or 0.0 when no
    // sample is loaded. Used by the SamplerStrip's playhead overlay to
    // correctly map source-sample index to time without assuming the host
    // rate matches the source rate.
    double getLoadedSourceSampleRate() const noexcept;

private:
    juce::Synthesiser synth;
    // Protects synth.clearSounds/addSound from concurrent renderNextBlock.
    // The audio thread blocks at most a few microseconds during the rare
    // UI-driven sample swap (clearSounds + addSound is a couple of pointer
    // updates inside JUCE). Acceptable trade-off for a UI-rate operation;
    // a SpinLock or atomic-pointer swap would be a follow-up if profiling
    // flags this on a contended system.
    std::mutex        soundsMutex;

    // Cached PhantomSamplerVoice* — populated in the ctor since voices
    // are added once and never replaced. Removes 32 dynamic_casts per
    // audio block compared to walking synth.getVoice(i) every setter call.
    std::vector<PhantomSamplerVoice*> phantomVoices;

    // Slice points in source samples (sorted, dedup'd, always includes 0
    // as the first entry; the last slice extends to srcLen). Owned here;
    // each voice keeps a const pointer for startNote lookups.
    std::vector<int> sliceTable;
};

} // namespace kaigen::phantom
