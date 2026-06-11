#include "SamplerMidiPreprocessor.h"
#include <algorithm>
#include <cmath>

namespace kaigen::phantom
{

void SamplerMidiPreprocessor::prepare()
{
    pendingNotes.clear();
    pendingNotes.reserve((size_t) kReservedPendingNotes);
    placedThisBlock.clear();
    placedThisBlock.reserve((size_t) kReservedPendingNotes);
    sampleCounter = 0;
    outBuffer.clear();
    outBuffer.ensureSize(512);
}

const juce::MidiBuffer& SamplerMidiPreprocessor::process(const juce::MidiBuffer& hostMidi,
                                                          int blockSize,
                                                          const Settings& s)
{
    outBuffer.clear();   // keeps allocated storage
    placedThisBlock.clear();

    const bool doQuantize = (s.gridPpq > 0.0) && s.havePpq && (s.ppqPerSample > 0.0);
    const int  velValue   = juce::jlimit(1, 127, s.velValue);

    auto applyVelOverride = [&](juce::MidiMessage& m)
    {
        if (s.velFixed && m.isNoteOn())
            m = juce::MidiMessage::noteOn(m.getChannel(), m.getNoteNumber(),
                                          (juce::uint8) velValue);
    };

    // Step 1: drain pending events whose trigger lands inside this block.
    for (auto it = pendingNotes.begin(); it != pendingNotes.end(); )
    {
        const int64_t relative = it->triggerSamples - sampleCounter;
        if (relative < (int64_t) blockSize)
        {
            const int offset = juce::jlimit(0, blockSize - 1,
                                            (int) juce::jmax<int64_t>(0, relative));
            outBuffer.addEvent(it->msg, offset);
            it = pendingNotes.erase(it);
        }
        else { ++it; }
    }

    // Step 2: rewrite host events, applying velocity override and quantize.
    for (const auto meta : hostMidi)
    {
        auto msg = meta.getMessage();
        const int offset = meta.samplePosition;

        applyVelOverride(msg);

        if (doQuantize && msg.isNoteOn())
        {
            const double notePpq    = s.blockStartPpq + offset * s.ppqPerSample;
            const double nextGrid   = std::ceil(notePpq / s.gridPpq) * s.gridPpq;
            const double deltaPpq   = nextGrid - notePpq;
            const int    deltaSamps = (int) std::round(deltaPpq / s.ppqPerSample);
            const int    target     = offset + deltaSamps;
            if (target < blockSize)
            {
                outBuffer.addEvent(msg, target);
                placedThisBlock.push_back({ msg.getChannel(), msg.getNoteNumber(),
                                            sampleCounter + (int64_t) target });
            }
            else
            {
                pendingNotes.push_back({ msg, sampleCounter + (int64_t) target });
            }
            continue;
        }

        // Note-off paired with a still-pending note-on: defer the note-off
        // as well so it can't reach a voice that hasn't started yet (which
        // would leave the queued note-on stuck on forever when it fires).
        // A small min-gap after the note-on guarantees the voice actually
        // starts before being released.
        if (doQuantize && (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0)))
        {
            auto matchPending = std::find_if(pendingNotes.begin(), pendingNotes.end(),
                [&](const PendingNote& p) {
                    return p.msg.isNoteOn()
                        && p.msg.getChannel()    == msg.getChannel()
                        && p.msg.getNoteNumber() == msg.getNoteNumber();
                });
            if (matchPending != pendingNotes.end())
            {
                const int64_t minGap  = (int64_t) (s.sampleRate * 0.010);   // 10 ms min note duration
                const int64_t natural = sampleCounter + (int64_t) offset;
                const int64_t offTrigger = juce::jmax(natural,
                                                      matchPending->triggerSamples + minGap);
                pendingNotes.push_back({ msg, offTrigger });
                continue;
            }
            // No pending match — the note-on may instead have been quantized
            // to a LATER offset within this same block. Emitting the off at
            // its natural (earlier) position would reach the voice first and
            // leave the on stuck; hold it to note-on + min-gap.
            auto matchPlaced = std::find_if(placedThisBlock.begin(), placedThisBlock.end(),
                [&](const PlacedNote& p) {
                    return p.channel == msg.getChannel()
                        && p.note    == msg.getNoteNumber();
                });
            if (matchPlaced != placedThisBlock.end())
            {
                const int64_t minGap  = (int64_t) (s.sampleRate * 0.010);
                const int64_t natural = sampleCounter + (int64_t) offset;
                const int64_t offTrigger = juce::jmax(natural,
                                                      matchPlaced->triggerSamples + minGap);
                if (offTrigger < sampleCounter + (int64_t) blockSize)
                    outBuffer.addEvent(msg, (int) (offTrigger - sampleCounter));
                else
                    pendingNotes.push_back({ msg, offTrigger });

                placedThisBlock.erase(matchPlaced);   // pair consumed
                continue;
            }
            // Still no match → note-on already fired in an earlier block;
            // the note-off passes through normally.
        }

        outBuffer.addEvent(msg, offset);
    }

    sampleCounter += blockSize;
    return outBuffer;
}

} // namespace kaigen::phantom
