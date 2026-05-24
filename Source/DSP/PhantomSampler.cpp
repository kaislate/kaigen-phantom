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
