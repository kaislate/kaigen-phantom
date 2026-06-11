#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "DSP/SamplerMidiPreprocessor.h"
#include <vector>

using namespace kaigen::phantom;

namespace
{
    struct Event
    {
        juce::MidiMessage msg;
        int samplePosition;
    };

    std::vector<Event> collect(const juce::MidiBuffer& mb)
    {
        std::vector<Event> out;
        for (const auto meta : mb)
            out.push_back({ meta.getMessage(), meta.samplePosition });
        return out;
    }

    // 120 bpm at 44.1 kHz: 1 beat = 0.5 s = 22050 samples.
    SamplerMidiPreprocessor::Settings quantSettings(double blockStartPpq)
    {
        SamplerMidiPreprocessor::Settings s;
        s.gridPpq       = 1.0;            // quantize to beats
        s.havePpq       = true;
        s.blockStartPpq = blockStartPpq;
        s.ppqPerSample  = (120.0 / 60.0) / 44100.0;
        s.sampleRate    = 44100.0;
        return s;
    }
}

TEST_CASE("SamplerMidiPreprocessor: passes events through unchanged without quantize", "[samplermidi]")
{
    SamplerMidiPreprocessor pre;
    pre.prepare();

    juce::MidiBuffer in;
    in.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 99), 37);
    in.addEvent(juce::MidiMessage::noteOff(1, 60), 200);

    SamplerMidiPreprocessor::Settings s;   // defaults: no quantize, no override
    const auto& out = pre.process(in, 512, s);

    auto ev = collect(out);
    REQUIRE(ev.size() == 2);
    CHECK(ev[0].msg.isNoteOn());
    CHECK(ev[0].samplePosition == 37);
    CHECK((int) ev[0].msg.getVelocity() == 99);
    CHECK(ev[1].msg.isNoteOff());
    CHECK(ev[1].samplePosition == 200);
}

TEST_CASE("SamplerMidiPreprocessor: fixed velocity overrides note-ons only", "[samplermidi]")
{
    SamplerMidiPreprocessor pre;
    pre.prepare();

    juce::MidiBuffer in;
    in.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 99), 0);
    in.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8) 64), 100);

    SamplerMidiPreprocessor::Settings s;
    s.velFixed = true;
    s.velValue = 31;
    const auto& out = pre.process(in, 512, s);

    auto ev = collect(out);
    REQUIRE(ev.size() == 2);
    CHECK((int) ev[0].msg.getVelocity() == 31);
    CHECK(ev[1].msg.isNoteOff());
}

TEST_CASE("SamplerMidiPreprocessor: quantize snaps note-on to next grid inside block", "[samplermidi]")
{
    SamplerMidiPreprocessor pre;
    pre.prepare();

    // Block starts 100 samples before the beat: blockStartPpq such that the
    // grid lands at sample 100 of this block.
    const double ppqPerSample = (120.0 / 60.0) / 44100.0;
    auto s = quantSettings(1.0 - 100.0 * ppqPerSample);

    juce::MidiBuffer in;
    in.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 10);
    const auto& out = pre.process(in, 512, s);

    auto ev = collect(out);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].msg.isNoteOn());
    CHECK(ev[0].samplePosition == 100);
}

TEST_CASE("SamplerMidiPreprocessor: quantize defers note-on across block boundary", "[samplermidi]")
{
    SamplerMidiPreprocessor pre;
    pre.prepare();

    // Grid point lands 700 samples after block start → beyond this 512-block.
    const double ppqPerSample = (120.0 / 60.0) / 44100.0;
    auto s = quantSettings(1.0 - 700.0 * ppqPerSample);

    juce::MidiBuffer in;
    in.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    const auto& out1 = pre.process(in, 512, s);
    CHECK(collect(out1).empty());

    // Next block: the queued note-on fires at 700 - 512 = 188.
    juce::MidiBuffer empty;
    s.blockStartPpq += 512.0 * ppqPerSample;
    const auto& out2 = pre.process(empty, 512, s);

    auto ev = collect(out2);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].msg.isNoteOn());
    CHECK(ev[0].samplePosition == 188);
}

TEST_CASE("SamplerMidiPreprocessor: note-off waits for its deferred note-on", "[samplermidi]")
{
    SamplerMidiPreprocessor pre;
    pre.prepare();

    const double ppqPerSample = (120.0 / 60.0) / 44100.0;
    auto s = quantSettings(1.0 - 700.0 * ppqPerSample);

    // Note-on (queued past block end) and its note-off in the same block.
    juce::MidiBuffer in;
    in.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    in.addEvent(juce::MidiMessage::noteOff(1, 60), 50);
    const auto& out1 = pre.process(in, 512, s);
    CHECK(collect(out1).empty());

    // Block 2: only the note-on fires (abs sample 700 → offset 188). The
    // note-off is held to note-on + 10 ms min-gap = abs 700 + 441 = 1141,
    // which is beyond this block.
    juce::MidiBuffer empty;
    s.blockStartPpq += 512.0 * ppqPerSample;
    const auto& out2 = pre.process(empty, 512, s);
    auto ev2 = collect(out2);
    REQUIRE(ev2.size() == 1);
    CHECK(ev2[0].msg.isNoteOn());
    CHECK(ev2[0].samplePosition == 188);

    // Block 3: the deferred note-off lands at abs 1141 → offset 117.
    s.blockStartPpq += 512.0 * ppqPerSample;
    const auto& out3 = pre.process(empty, 512, s);
    auto ev3 = collect(out3);
    REQUIRE(ev3.size() == 1);
    CHECK(ev3[0].msg.isNoteOff());
    CHECK(ev3[0].samplePosition == 117);
}

TEST_CASE("SamplerMidiPreprocessor: note-off never precedes its same-block quantized note-on", "[samplermidi]")
{
    SamplerMidiPreprocessor pre;
    pre.prepare();

    // Grid lands at sample 100 of this block. Note-on at 10 quantizes to
    // 100; its note-off arrives at 30 — BEFORE the shifted note-on. The
    // off must be deferred to note-on + 10 ms min-gap (abs 541, block 2 at
    // offset 29), never emitted ahead of the on.
    const double ppqPerSample = (120.0 / 60.0) / 44100.0;
    auto s = quantSettings(1.0 - 100.0 * ppqPerSample);

    juce::MidiBuffer in;
    in.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 10);
    in.addEvent(juce::MidiMessage::noteOff(1, 60), 30);
    const auto& out1 = pre.process(in, 512, s);

    auto ev1 = collect(out1);
    REQUIRE(ev1.size() == 1);
    CHECK(ev1[0].msg.isNoteOn());
    CHECK(ev1[0].samplePosition == 100);

    juce::MidiBuffer empty;
    s.blockStartPpq += 512.0 * ppqPerSample;
    const auto& out2 = pre.process(empty, 512, s);

    auto ev2 = collect(out2);
    REQUIRE(ev2.size() == 1);
    CHECK(ev2[0].msg.isNoteOff());
    CHECK(ev2[0].samplePosition == 29);
}
