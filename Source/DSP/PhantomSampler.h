#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <vector>
#include <signalsmith-stretch/signalsmith-stretch.h>

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
    void pitchWheelMoved(int newPitchWheelValue) override;
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
    void setReverse(bool r) noexcept { reverse = r; }
    /** Loop crossfade in milliseconds (0 = hard wrap, no crossfade). The
     *  per-voice render converts this to source-rate samples each block. */
    void setLoopCrossfadeMs(float ms) noexcept { loopXfadeMs = juce::jmax(0.0f, ms); }
    /** Warp mode: 0 = Off (varispeed), 1 = Complex (pitch-shift via stretcher). */
    void setWarpMode(int m) noexcept { warpMode = m; }
    /** Called by PhantomSampler::prepareToPlay to configure the stretcher
     *  for the live sample rate and pre-size the warp scratch buffers to
     *  the host's worst-case block size. Allocates internal buffers — must
     *  NOT be called from the audio thread. */
    void prepareStretcher(double liveSampleRate, int maxBlockSize);
    /** Pointer-to-shared slice table (lives on PhantomSampler).
     *  PhantomSampler keeps it alive; voices read by const reference. */
    void setSliceTable(const std::vector<int>* table) noexcept { slicePoints = table; }

    // For the SamplerStrip playhead overlay. 0..numSamples-1 of the
    // active sample; -1 when voice idle. Read by the message thread,
    // written by the audio thread — atomic.
    int getPlayheadPosition() const noexcept { return playheadAtomic.load(std::memory_order_relaxed); }

private:
    // Complex warp mode render — broken out so the main renderNextBlock
    // doesn't get unreadable. Reads from the same `src` buffer the
    // varispeed path uses; advances sourcePosition at the source rate
    // (1 sample per output sample) rather than at pitchRatio.
    void renderWarpBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples,
                         const juce::AudioBuffer<float>& src,
                         PhantomSamplerSound* sound);

    /** Native sample rate of the currently-playing sound, or the live rate
     *  if no sound is playing. Used to convert ms-based params (e.g. loop
     *  crossfade) into source-rate samples in the warp path. */
    double sourceRateCache() const noexcept;

    /** Null activeSound alongside the base class's note-clear so the cached
     *  pointer can never outlive the ref the base class holds. */
    void clearNote() noexcept
    {
        clearCurrentNote();
        activeSound = nullptr;
    }

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
    int                   sliceStartSample { 0 };       // computed at startNote in slice mode
    int                   sliceEndSample { 0 };         // computed at startNote in slice mode
    bool                  reverse        { false };
    double                bendRatio      { 1.0 };       // pitch-bend multiplier (±2 semitones)
    float                 loopXfadeMs    { 0.0f };      // 0 = no crossfade
    int                   warpMode       { 0 };         // 0 = Off, 1 = Complex
    juce::ADSR            adsr;
    std::atomic<int>      playheadAtomic   { -1 };

    // Cached at startNote; valid while the base class holds its reference
    // to the playing sound (i.e. until clearNote). Avoids a per-block
    // dynamic_cast + refcount round-trip on getCurrentlyPlayingSound().
    PhantomSamplerSound*  activeSound      { nullptr };

    // SignalSmith stretcher — used only in Complex warp mode. Configured
    // for stereo at the live sample rate in prepareStretcher. Reset at
    // each note-on so we don't bleed the previous note's spectral state.
    signalsmith::stretch::SignalsmithStretch<float> stretcher;
    bool                  stretcherReady { false };
    std::vector<float>    warpInBufL, warpInBufR;        // scratch for source -> stretcher
    std::vector<float>    warpOutBufL, warpOutBufR;      // scratch for stretcher -> output
    bool                  needsStretcherReset { false };
};

// PhantomSampler — owns the juce::Synthesiser and exposes a small API
// suitable for PluginProcessor to drive each block.
class PhantomSampler
{
public:
    static constexpr int kNumVoices = 8;

    PhantomSampler();
    ~PhantomSampler();
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
    void setReverse(bool r) noexcept;
    void setLoopCrossfadeMs(float ms) noexcept;
    void setWarpMode(int m) noexcept;

    /** Detects onsets in the loaded sample and stores them as slice
     *  points. Returns the new number of slices (always >= 1; a sample
     *  with no detected transients yields a single full-length slice). */
    int detectSlices();

    /** Replace the slice table with an externally-provided list (used
     *  when restoring from plugin state, and by the UI's slice-handle
     *  drag). Sorted + clamped + dedup'd. Message thread only; the audio
     *  thread adopts the new table wait-free at the next block boundary. */
    void setSliceTable(std::vector<int> slices);

    /** Read-only access to the current slice table. Message thread only —
     *  returns the message-thread copy, which leads the audio thread's
     *  adopted table by at most one block. */
    const std::vector<int>& getSliceTable() const noexcept { return sliceTableMsgThread; }

    int  getActiveVoiceCount() const noexcept;
    int  getPlayheadPosition() const noexcept;

    // Source sample rate of the currently-loaded sample, or 0.0 when no
    // sample is loaded. Used by the SamplerStrip's playhead overlay to
    // correctly map source-sample index to time without assuming the host
    // rate matches the source rate.
    double getLoadedSourceSampleRate() const noexcept;

private:
    /** Message-thread side of the slice-table handoff: frees whatever the
     *  audio thread parked in retiredSliceTable. */
    void reclaimRetiredSliceTable() noexcept;

    juce::Synthesiser synth;

    // Cached PhantomSamplerVoice* — populated in the ctor since voices
    // are added once and never replaced. Removes 32 dynamic_casts per
    // audio block compared to walking synth.getVoice(i) every setter call.
    std::vector<PhantomSamplerVoice*> phantomVoices;

    // ── Slice table: wait-free message→audio handoff ──────────────────
    // The message thread keeps its own copy (UI drawing / state save) and
    // stages an immutable heap copy in `incomingSliceTable`. The audio
    // thread adopts the staged table at the top of renderNextBlock and
    // parks the displaced one in `retiredSliceTable` for the message
    // thread to delete — the audio thread never locks, allocates, or
    // frees. If the retire slot is still occupied the audio thread keeps
    // its current table for another block and retries. Slice points are
    // source-sample positions, sorted + dedup'd, always starting with 0;
    // the last slice extends to srcLen.
    std::vector<int>                     sliceTableMsgThread { 0 };
    std::atomic<const std::vector<int>*> incomingSliceTable  { nullptr };
    std::atomic<const std::vector<int>*> retiredSliceTable   { nullptr };
    const std::vector<int>*              activeSliceTable    { nullptr }; // audio-thread-owned

    // Message-thread reference to the loaded sound. hasSample /
    // getLoadedSourceSampleRate / detectSlices read THIS, never the
    // synth's sounds list, so they need no synchronisation with the audio
    // thread. The synth's own list is mutated from the message thread in
    // loadSample/clearSample under the Synthesiser's internal lock (the
    // documented JUCE threading model for sound swaps).
    juce::SynthesiserSound::Ptr currentSound;
};

} // namespace kaigen::phantom
