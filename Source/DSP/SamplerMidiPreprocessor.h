#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace kaigen::phantom
{

/**
 * SamplerMidiPreprocessor — rewrites the host MIDI stream into the
 * sampler's per-block buffer, applying the fixed-velocity override and
 * beat quantization.
 *
 * Note-ons whose grid point lands beyond the current block are queued and
 * emitted in the block where the grid point falls. Their note-offs are
 * deferred alongside (with a small minimum gap after the note-on) so a
 * release can never reach the synth before its attack — which would leave
 * the queued note-on stuck on forever when it finally fired.
 *
 * Owns its output buffer and pending-note storage; process() performs no
 * heap allocation after prepare() while at most kReservedPendingNotes
 * deferred events are in flight.
 */
class SamplerMidiPreprocessor
{
public:
    struct Settings
    {
        bool   velFixed      = false;   // override note-on velocity
        int    velValue      = 127;     // [1, 127]
        double gridPpq       = 0.0;     // quantize grid in ppq; 0 = off
        bool   havePpq       = false;   // host supplied a ppq position
        double blockStartPpq = 0.0;
        double ppqPerSample  = 0.0;
        double sampleRate    = 44100.0;
    };

    /** Reset the sample counter and reserve pending storage. Call from
     *  prepareToPlay — allocates, so not on the audio thread mid-stream. */
    void prepare();

    /** Rewrite one block of host MIDI. The returned buffer is owned by
     *  this object and valid until the next call. Audio thread. */
    const juce::MidiBuffer& process(const juce::MidiBuffer& hostMidi,
                                    int blockSize,
                                    const Settings& s);

private:
    struct PendingNote
    {
        juce::MidiMessage msg;
        int64_t           triggerSamples;   // absolute sample time
    };

    // A quantized note-on emitted within the current block. A note-off for
    // the same note arriving later in the same host buffer must not be
    // emitted ahead of this (the off would reach the voice before the on
    // and leave it stuck); cleared at the top of every process() call.
    struct PlacedNote
    {
        int     channel;
        int     note;
        int64_t triggerSamples;   // absolute sample time
    };

    static constexpr int kReservedPendingNotes = 64;

    std::vector<PendingNote> pendingNotes;
    std::vector<PlacedNote>  placedThisBlock;
    juce::MidiBuffer         outBuffer;
    int64_t                  sampleCounter { 0 };
};

} // namespace kaigen::phantom
