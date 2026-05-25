#include "PhantomSampler.h"
#include <cmath>

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
    // Start from the start marker (or 0 if no start was set).
    const int srcLenForStart = sound->getAudioBuffer().getNumSamples();
    sourcePosition   = (double) startFrac * (double) srcLenForStart;
    velocityGain     = juce::jlimit(0.0f, 1.0f, velocity);
    adsr.setSampleRate(liveRate);
    adsr.noteOn();
    playheadAtomic.store((int) sourcePosition, std::memory_order_relaxed);
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
        const int  i0 = juce::jlimit(0, srcLen - 1, (int) pos);
        const int  i1 = juce::jmin(srcLen - 1, i0 + 1);
        const float f = (float) juce::jlimit(0.0, 1.0, pos - (double) i0);
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
            const float v = sampleAt(ch, sourcePosition) * env * baseGainLinear * velocityGain;
            outputBuffer.addSample(ch, startSample + n, v);
        }

        // Effective playback window respects start/end markers. Clamp
        // computed bounds against srcLen so a marker beyond the actual
        // sample length doesn't read out of bounds.
        const int startSamp = juce::jlimit(0, srcLen - 1, (int) (startFrac * srcLen));
        const int endSamp   = juce::jlimit(startSamp + 1, srcLen, (int) (endFrac   * srcLen));
        const int regionLen = endSamp - startSamp;

        sourcePosition += pitchRatio;
        if (sourcePosition >= (double) endSamp)
        {
            if (loopEnabled && regionLen > 0)
            {
                // Wrap back to startSamp, preserving any overshoot.
                const double overshoot = std::fmod(sourcePosition - (double) startSamp,
                                                    (double) regionLen);
                sourcePosition = (double) startSamp +
                                 (overshoot < 0.0 ? overshoot + regionLen : overshoot);
            }
            else
            {
                sourcePosition = (double) (endSamp - 1);
                adsr.noteOff();   // start release; voice continues to silence
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

    for (int i = 0; i < synth.getNumVoices(); ++i)
        phantomVoices.push_back(static_cast<PhantomSamplerVoice*>(synth.getVoice(i)));
}

void PhantomSampler::prepareToPlay(double sampleRate, int /*blockSize*/)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void PhantomSampler::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                      const juce::MidiBuffer& midi)
{
    std::lock_guard<std::mutex> lock(soundsMutex);
    synth.renderNextBlock(outputBuffer, midi, 0, outputBuffer.getNumSamples());
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

double PhantomSampler::getLoadedSourceSampleRate() const noexcept
{
    // Reaches into the currently-loaded sound to read its source rate.
    // synth.getSound is safe from the message thread; sounds list is
    // mutated under soundsMutex by loadSample/clearSample.
    if (synth.getNumSounds() == 0) return 0.0;
    if (auto* sound = dynamic_cast<PhantomSamplerSound*>(synth.getSound(0).get()))
        return sound->getSourceSampleRate();
    return 0.0;
}

void PhantomSampler::setRootNote(int n) noexcept
{
    for (auto* v : phantomVoices) v->setRootNote(n);
}

void PhantomSampler::setStartEnd(float start01, float end01) noexcept
{
    for (auto* v : phantomVoices) v->setStartEnd(start01, end01);
}

void PhantomSampler::setLoopEnabled(bool l) noexcept
{
    for (auto* v : phantomVoices) v->setLoopEnabled(l);
}

void PhantomSampler::setGainDb(float gainDb) noexcept
{
    const float linear = juce::Decibels::decibelsToGain(gainDb);
    for (auto* v : phantomVoices) v->setGainLinear(linear);
}

void PhantomSampler::setEnvelope(float a, float d, float s, float r)
{
    juce::ADSR::Parameters p{ a, d, juce::jlimit(0.0f, 1.0f, s), r };
    for (auto* v : phantomVoices) v->setEnvelopeParameters(p);
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
    // Returns the first active voice scanning voice indices from highest
    // to lowest — a cheap, deterministic heuristic for the SamplerStrip's
    // playhead overlay. Not strictly "most recently started"; JUCE's
    // Synthesiser voice allocator doesn't guarantee that ordering.
    for (auto it = phantomVoices.rbegin(); it != phantomVoices.rend(); ++it)
    {
        if ((*it)->isVoiceActive())
            return (*it)->getPlayheadPosition();
    }
    return -1;
}

} // namespace kaigen::phantom
