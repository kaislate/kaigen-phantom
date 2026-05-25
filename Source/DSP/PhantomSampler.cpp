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
    const int srcLenForStart = sound->getAudioBuffer().getNumSamples();

    if (sliceMode && slicePoints != nullptr && ! slicePoints->empty())
    {
        // MIDI note → slice index, mapped C2 (MIDI 36) = slice 0.
        const int numSlices = (int) slicePoints->size();
        const int sliceIdx  = juce::jlimit(0, numSlices - 1, midiNoteNumber - 36);
        const int startSamp = (*slicePoints)[(size_t) sliceIdx];
        sliceEndSample = (sliceIdx + 1 < numSlices)
                            ? (*slicePoints)[(size_t) (sliceIdx + 1)]
                            : srcLenForStart;
        sourcePosition  = (double) startSamp;
        // Slice playback always at native source rate — no pitch shift.
        pitchRatio = srcRate / liveRate;
    }
    else
    {
        pitchRatio = noteRatio * (srcRate / liveRate);
        // Start from the start marker (or 0 if no start was set).
        sourcePosition  = (double) startFrac * (double) srcLenForStart;
        sliceEndSample  = 0;   // unused outside slice mode
    }

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

        // Effective playback window. In pitched mode this respects the
        // user-set start/end markers; in slice mode the bounds are the
        // current slice [sliceStart, sliceEndSample] computed at startNote.
        const int regionEnd = sliceMode
            ? juce::jmin(srcLen, sliceEndSample)
            : juce::jlimit(1, srcLen, (int) (endFrac * srcLen));
        const int regionStart = sliceMode
            ? juce::jlimit(0, regionEnd - 1, (int) sourcePosition)   // no loop-wrap target in slice
            : juce::jlimit(0, regionEnd - 1, (int) (startFrac * srcLen));
        const int regionLen = regionEnd - regionStart;

        sourcePosition += pitchRatio;
        if (sourcePosition >= (double) regionEnd)
        {
            // Loop wrap is disabled in slice mode (each slice is a one-shot).
            if (loopEnabled && ! sliceMode && regionLen > 0)
            {
                const double overshoot = std::fmod(sourcePosition - (double) regionStart,
                                                    (double) regionLen);
                sourcePosition = (double) regionStart +
                                 (overshoot < 0.0 ? overshoot + regionLen : overshoot);
            }
            else
            {
                sourcePosition = (double) (regionEnd - 1);
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

void PhantomSampler::setSliceMode(bool slice) noexcept
{
    for (auto* v : phantomVoices) v->setSliceMode(slice);
}

void PhantomSampler::setSliceTable(std::vector<int> slices)
{
    // Sort + dedup + clamp to [0, srcLen). Always include 0 as the
    // first slice so MIDI note 36 has a valid start point even on
    // a sample with no detected transients.
    std::sort(slices.begin(), slices.end());
    slices.erase(std::unique(slices.begin(), slices.end()), slices.end());
    if (slices.empty() || slices.front() != 0)
        slices.insert(slices.begin(), 0);

    sliceTable = std::move(slices);
    for (auto* v : phantomVoices) v->setSliceTable(&sliceTable);
}

int PhantomSampler::detectSlices()
{
    // Energy-based onset detection over the loaded sound. Adequate for
    // percussive material; tonal samples often yield just slice 0 (full
    // sample) which is the right fallback.
    std::vector<int> hits;
    hits.push_back(0);   // slice 0 always at sample 0

    if (synth.getNumSounds() > 0)
    {
        if (auto* sound = dynamic_cast<PhantomSamplerSound*>(synth.getSound(0).get()))
        {
            const auto& src = sound->getAudioBuffer();
            const int srcLen = src.getNumSamples();
            const double srcRate = sound->getSourceSampleRate();

            if (srcLen > 1024 && srcRate > 0.0)
            {
                constexpr int   kHopSize     = 256;     // ~5.8 ms @ 44.1k
                constexpr int   kFrameSize   = 1024;    // ~23 ms @ 44.1k
                constexpr int   kMaxSlices   = 32;
                const int       minGapSamp   = (int) (0.05 * srcRate);   // 50 ms
                const int       numFrames    = (srcLen - kFrameSize) / kHopSize;

                std::vector<float> rms((size_t) numFrames, 0.0f);
                for (int f = 0; f < numFrames; ++f)
                {
                    const int start = f * kHopSize;
                    double sum = 0.0;
                    for (int i = 0; i < kFrameSize; ++i)
                    {
                        const float s = src.getSample(0, start + i);
                        sum += (double) s * (double) s;
                    }
                    rms[(size_t) f] = (float) std::sqrt(sum / (double) kFrameSize);
                }

                // Positive RMS differences = energy flux.
                std::vector<float> flux((size_t) numFrames, 0.0f);
                for (int f = 1; f < numFrames; ++f)
                {
                    const float d = rms[(size_t) f] - rms[(size_t) (f - 1)];
                    flux[(size_t) f] = juce::jmax(0.0f, d);
                }

                // Peak-pick above an adaptive threshold (3× local median),
                // enforcing minimum slice spacing.
                for (int f = 1; f < numFrames - 1; ++f)
                {
                    const int wStart = juce::jmax(0, f - 20);
                    const int wEnd   = juce::jmin(numFrames, f + 20);
                    std::vector<float> window(flux.begin() + wStart, flux.begin() + wEnd);
                    std::sort(window.begin(), window.end());
                    const float median    = window[window.size() / 2];
                    const float threshold = juce::jmax(0.02f, median * 3.0f);

                    if (flux[(size_t) f] > threshold
                        && flux[(size_t) f] > flux[(size_t) (f - 1)]
                        && flux[(size_t) f] > flux[(size_t) (f + 1)])
                    {
                        const int samp = f * kHopSize;
                        if (samp - hits.back() >= minGapSamp)
                        {
                            hits.push_back(samp);
                            if ((int) hits.size() >= kMaxSlices) break;
                        }
                    }
                }
            }
        }
    }

    setSliceTable(std::move(hits));
    return (int) sliceTable.size();
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
