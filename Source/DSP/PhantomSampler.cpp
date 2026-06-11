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
                                     int currentPitchWheelPosition)
{
    auto* sound = dynamic_cast<PhantomSamplerSound*>(s);
    if (sound == nullptr) return;
    activeSound = sound;

    // Playback rate = note ratio × (source rate / live rate).
    const double noteRatio = std::pow(2.0, (midiNoteNumber - rootNote) / 12.0);
    const double srcRate   = sound->getSourceSampleRate();
    const double liveRate  = getSampleRate();
    const int srcLenForStart = sound->getAudioBuffer().getNumSamples();

    int regionStartSamp = 0;
    int regionEndSamp   = srcLenForStart;

    if (sliceMode && slicePoints != nullptr && ! slicePoints->empty())
    {
        // MIDI note → slice index, mapped C2 (MIDI 36) = slice 0.
        const int numSlices = (int) slicePoints->size();
        const int sliceIdx  = juce::jlimit(0, numSlices - 1, midiNoteNumber - 36);
        regionStartSamp = (*slicePoints)[(size_t) sliceIdx];
        regionEndSamp   = (sliceIdx + 1 < numSlices)
                            ? (*slicePoints)[(size_t) (sliceIdx + 1)]
                            : srcLenForStart;
        sliceStartSample = regionStartSamp;
        sliceEndSample   = regionEndSamp;
        // Slice playback always at native source rate — no pitch shift.
        pitchRatio = srcRate / liveRate;
    }
    else
    {
        pitchRatio = noteRatio * (srcRate / liveRate);
        regionStartSamp = (int) ((double) startFrac * (double) srcLenForStart);
        regionEndSamp   = juce::jmax(regionStartSamp + 1,
                                      (int) ((double) endFrac * (double) srcLenForStart));
        sliceStartSample = 0;
        sliceEndSample   = 0;   // unused outside slice mode
    }

    // Reverse → start at the END of the region and play backwards. The
    // render loop reads `reverse` per-sample for the position update and
    // chooses the corresponding wrap-around direction.
    sourcePosition = reverse ? (double) (regionEndSamp - 1)
                             : (double) regionStartSamp;

    velocityGain = juce::jlimit(0.0f, 1.0f, velocity);
    // Apply the current pitch-wheel position so a held bend on a fresh
    // note arrives pre-bent rather than snapping to 0 at note-on.
    pitchWheelMoved(currentPitchWheelPosition);

    // Flag the stretcher for a reset on the next render call. We don't reset
    // here because the message thread mustn't poke a stretcher that might be
    // mid-process on the audio thread — startNote IS the audio thread, but
    // the reset still needs to happen at the top of the next render so the
    // pitch ratio is applied to a clean state.
    needsStretcherReset = true;

    adsr.setSampleRate(liveRate);
    adsr.noteOn();
    playheadAtomic.store((int) sourcePosition, std::memory_order_relaxed);
}

void PhantomSamplerVoice::pitchWheelMoved(int newPitchWheelValue)
{
    // Standard ±2 semitones around the 8192 centre. Stored as a frequency
    // ratio that the render loop multiplies into pitchRatio per sample.
    const double semitones = ((double) newPitchWheelValue - 8192.0) / 8192.0 * 2.0;
    bendRatio = std::pow(2.0, semitones / 12.0);
}

void PhantomSamplerVoice::renderWarpBlock(juce::AudioBuffer<float>& outputBuffer,
                                            int startSample, int numSamples,
                                            const juce::AudioBuffer<float>& src,
                                            PhantomSamplerSound* /*sound*/)
{
    const int srcLen   = src.getNumSamples();
    const int srcChans = src.getNumChannels();
    const int outChans = outputBuffer.getNumChannels();

    if (needsStretcherReset)
    {
        stretcher.reset();
        needsStretcherReset = false;
    }

    // Transpose factor for the stretcher = pure MIDI note ratio × bend
    // ratio. startNote bakes (srcRate/liveRate) into pitchRatio so the
    // varispeed render advances sourcePosition at the right rate; here we
    // back that compensation out so the stretcher only sees the musical
    // pitch shift.
    const double liveRate = getSampleRate();
    const double srcRate  = sourceRateCache();
    const double rateComp = (liveRate > 0.0) ? srcRate / liveRate : 1.0;
    const double noteRatio = (rateComp > 0.0) ? pitchRatio / rateComp : 1.0;
    stretcher.setTransposeFactor((float) (noteRatio * bendRatio));

    // Effective region (markers, slice).
    const int regionEnd = juce::jlimit(1, srcLen, (int) (endFrac * srcLen));
    const int regionStart = juce::jlimit(0, juce::jmax(0, regionEnd - 1),
                                          (int) (startFrac * srcLen));
    const int regionLen = juce::jmax(1, regionEnd - regionStart);

    // Crossfade samples on the loop seam (input side — we blend the source
    // before it hits the stretcher so the spectral analysis stays continuous).
    const int xfadeSamples = juce::jlimit(0, regionLen / 3,
        (int) ((double) loopXfadeMs * 0.001 * sourceRateCache()));

    // Scratch is pre-sized in prepareStretcher; this resize only fires if
    // the host delivers a block larger than the prepared maximum (a host
    // bug, but allocate rather than read out of bounds).
    if ((int) warpInBufL.size() < numSamples)
    {
        jassertfalse;
        warpInBufL.resize((size_t) numSamples);
        warpInBufR.resize((size_t) numSamples);
        warpOutBufL.resize((size_t) numSamples);
        warpOutBufR.resize((size_t) numSamples);
    }

    // Fill warpInBufL/R from the source at sourcePosition (1 per sample)
    // with reverse, loop wrap, and input crossfade applied.
    const int direction = reverse ? -1 : 1;

    for (int n = 0; n < numSamples; ++n)
    {
        // Bounds: clamp position into source for the read. Wrap in loop
        // mode; force noteOff when not looping and we run off the region.
        if (! reverse && sourcePosition >= (double) regionEnd)
        {
            if (loopEnabled)
            {
                const double effLen = (double) juce::jmax(1, regionLen - xfadeSamples);
                const double overshoot = std::fmod(sourcePosition - (double) regionEnd, effLen);
                sourcePosition = (double) regionStart + (double) xfadeSamples
                                 + (overshoot < 0.0 ? overshoot + effLen : overshoot);
            }
            else
            {
                sourcePosition = (double) (regionEnd - 1);
                adsr.noteOff();
            }
        }
        else if (reverse && sourcePosition < (double) regionStart)
        {
            if (loopEnabled)
            {
                const double effLen = (double) juce::jmax(1, regionLen - xfadeSamples);
                const double overshoot = std::fmod((double) regionStart - sourcePosition, effLen);
                sourcePosition = (double) (regionEnd - 1) - (double) xfadeSamples
                                 - (overshoot < 0.0 ? overshoot + effLen : overshoot);
            }
            else
            {
                sourcePosition = (double) regionStart;
                adsr.noteOff();
            }
        }

        const int pos = juce::jlimit(0, srcLen - 1, (int) sourcePosition);

        // Input crossfade on the loop seam (mirror the varispeed path).
        float blendAlpha = 0.0f;
        int   partnerPos = pos;
        if (loopEnabled && xfadeSamples > 0)
        {
            if (! reverse)
            {
                const double distFromEnd = (double) regionEnd - sourcePosition;
                if (distFromEnd <= (double) xfadeSamples)
                {
                    const double k = (double) xfadeSamples - distFromEnd;
                    blendAlpha = (float) juce::jlimit(0.0, 1.0, k / (double) xfadeSamples);
                    partnerPos = juce::jlimit(0, srcLen - 1, regionStart + (int) k);
                }
            }
            else
            {
                const double distFromStart = sourcePosition - (double) regionStart;
                if (distFromStart <= (double) xfadeSamples)
                {
                    const double k = (double) xfadeSamples - distFromStart;
                    blendAlpha = (float) juce::jlimit(0.0, 1.0, k / (double) xfadeSamples);
                    partnerPos = juce::jlimit(0, srcLen - 1, (regionEnd - 1) - (int) k);
                }
            }
        }

        const float lA = src.getSample(0, pos);
        const float lB = (blendAlpha > 0.0f) ? src.getSample(0, partnerPos) : lA;
        const float rA = src.getSample(juce::jmin(srcChans - 1, 1), pos);
        const float rB = (blendAlpha > 0.0f)
                            ? src.getSample(juce::jmin(srcChans - 1, 1), partnerPos) : rA;

        warpInBufL[(size_t) n] = (1.0f - blendAlpha) * lA + blendAlpha * lB;
        warpInBufR[(size_t) n] = (1.0f - blendAlpha) * rA + blendAlpha * rB;

        sourcePosition += (double) direction;
    }

    // Process through the stretcher. Same input and output count = pitch
    // shift only, no time stretch.
    float* inPtrs[2]  = { warpInBufL.data(),  warpInBufR.data()  };
    float* outPtrs[2] = { warpOutBufL.data(), warpOutBufR.data() };
    stretcher.process(inPtrs, numSamples, outPtrs, numSamples);

    // Mix the stretched output into the voice's allocation in outputBuffer,
    // applying ADSR + gain on top. ADSR ticks per output sample.
    for (int n = 0; n < numSamples; ++n)
    {
        const float env = adsr.getNextSample();
        if (! adsr.isActive())
        {
            clearNote();
            playheadAtomic.store(-1, std::memory_order_relaxed);
            return;
        }
        const float lvl = env * baseGainLinear * velocityGain;
        outputBuffer.addSample(0, startSample + n, warpOutBufL[(size_t) n] * lvl);
        if (outChans > 1)
            outputBuffer.addSample(1, startSample + n, warpOutBufR[(size_t) n] * lvl);
    }

    playheadAtomic.store(juce::jlimit(0, srcLen - 1, (int) sourcePosition),
                          std::memory_order_relaxed);
}

double PhantomSamplerVoice::sourceRateCache() const noexcept
{
    if (activeSound != nullptr)
        return activeSound->getSourceSampleRate();
    return getSampleRate();
}

void PhantomSamplerVoice::prepareStretcher(double liveSampleRate, int maxBlockSize)
{
    // 2 channels matches the sampler's stereo output. presetDefault is the
    // recommended starting point per the SignalSmith docs; configuration is
    // an internal trade-off between latency and CPU. presetCheaper would
    // halve CPU but adds audible smearing on transients.
    stretcher.presetDefault(2, liveSampleRate);
    stretcherReady = true;
    needsStretcherReset = true;

    // Pre-size the warp scratch to the host's worst-case block so the
    // render path never allocates on the audio thread.
    const auto cap = (size_t) juce::jmax(1, maxBlockSize);
    warpInBufL.resize(cap);
    warpInBufR.resize(cap);
    warpOutBufL.resize(cap);
    warpOutBufR.resize(cap);
}

void PhantomSamplerVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        clearNote();
        adsr.reset();
        playheadAtomic.store(-1, std::memory_order_relaxed);
    }
}

void PhantomSamplerVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                            int startSample, int numSamples)
{
    auto* sound = activeSound;
    if (sound == nullptr || ! adsr.isActive()) return;

    const auto& src = sound->getAudioBuffer();
    const int   srcLen = src.getNumSamples();
    if (srcLen == 0) { clearNote(); return; }

    const int   srcChans = src.getNumChannels();
    const int   outChans = outputBuffer.getNumChannels();
    const double srcRate = sound->getSourceSampleRate();

    // ── Complex warp mode (pitch-shift via SignalSmith Stretch) ──────────
    // Disabled in slice mode (slices play at native rate per slice; pitch
    // isn't keyed to MIDI note there). Falls through to the classic
    // varispeed path below when off.
    if (warpMode == 1 && ! sliceMode && stretcherReady)
    {
        renderWarpBlock(outputBuffer, startSample, numSamples, src, sound);
        return;
    }

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

    // Cached region. In pitched mode this respects the user-set start/end
    // markers; in slice mode the bounds are the current slice. Recomputed
    // each block since startFrac/endFrac can change between blocks.
    const int regionEnd = sliceMode
        ? juce::jmin(srcLen, sliceEndSample)
        : juce::jlimit(1, srcLen, (int) (endFrac * srcLen));
    const int regionStart = sliceMode
        ? juce::jlimit(0, juce::jmax(0, regionEnd - 1), sliceStartSample)
        : juce::jlimit(0, juce::jmax(0, regionEnd - 1), (int) (startFrac * srcLen));
    const int regionLen = juce::jmax(1, regionEnd - regionStart);

    // Loop crossfade length in source samples. Clamped to at most one
    // third of the loop region so a tiny loop with a 100 ms crossfade
    // setting can't cover the entire region (which would produce silence).
    const int xfadeSamples = juce::jlimit(
        0,
        regionLen / 3,
        (int) ((double) loopXfadeMs * 0.001 * srcRate));

    const double direction = reverse ? -1.0 : 1.0;
    const bool   xfadeActive = loopEnabled && ! sliceMode && xfadeSamples > 0;

    for (int n = 0; n < numSamples; ++n)
    {
        const float env = adsr.getNextSample();
        if (! adsr.isActive())
        {
            clearNote();
            playheadAtomic.store(-1, std::memory_order_relaxed);
            return;
        }

        // Compute crossfade blend for the loop seam. In forward mode the
        // last xfadeSamples before regionEnd blend the end audio with the
        // matching offset from regionStart so the post-wrap audio is
        // already audible. Mirrored for reverse playback.
        float blendAlpha = 0.0f;
        double xfadePartnerPos = 0.0;
        if (xfadeActive)
        {
            if (! reverse)
            {
                const double distFromEnd = (double) regionEnd - sourcePosition;
                if (distFromEnd <= (double) xfadeSamples)
                {
                    const double k = (double) xfadeSamples - distFromEnd;   // 0..xfadeSamples
                    blendAlpha      = (float) juce::jlimit(0.0, 1.0, k / (double) xfadeSamples);
                    xfadePartnerPos = (double) regionStart + k;
                }
            }
            else
            {
                const double distFromStart = sourcePosition - (double) regionStart;
                if (distFromStart <= (double) xfadeSamples)
                {
                    const double k = (double) xfadeSamples - distFromStart;
                    blendAlpha      = (float) juce::jlimit(0.0, 1.0, k / (double) xfadeSamples);
                    xfadePartnerPos = (double) (regionEnd - 1) - k;
                }
            }
        }

        for (int ch = 0; ch < outChans; ++ch)
        {
            float s = sampleAt(ch, sourcePosition);
            if (blendAlpha > 0.0f)
            {
                const float partner = sampleAt(ch, xfadePartnerPos);
                s = (1.0f - blendAlpha) * s + blendAlpha * partner;
            }
            const float v = s * env * baseGainLinear * velocityGain;
            outputBuffer.addSample(ch, startSample + n, v);
        }

        sourcePosition += direction * pitchRatio * bendRatio;

        // Boundary check + wrap. Forward overshoots regionEnd; reverse
        // undershoots regionStart. The wrap target is offset by xfadeSamples
        // so the crossfade-blended start audio isn't replayed un-blended.
        if (! reverse && sourcePosition >= (double) regionEnd)
        {
            if (loopEnabled && regionLen > 0)
            {
                const double effLen = (double) juce::jmax(1, regionLen - xfadeSamples);
                const double overshoot = std::fmod(sourcePosition - (double) regionEnd, effLen);
                sourcePosition = (double) regionStart + (double) xfadeSamples
                                 + (overshoot < 0.0 ? overshoot + effLen : overshoot);
            }
            else
            {
                sourcePosition = (double) (regionEnd - 1);
                adsr.noteOff();
            }
        }
        else if (reverse && sourcePosition < (double) regionStart)
        {
            if (loopEnabled && regionLen > 0)
            {
                const double effLen = (double) juce::jmax(1, regionLen - xfadeSamples);
                const double overshoot = std::fmod((double) regionStart - sourcePosition, effLen);
                sourcePosition = (double) (regionEnd - 1) - (double) xfadeSamples
                                 - (overshoot < 0.0 ? overshoot + effLen : overshoot);
            }
            else
            {
                sourcePosition = (double) regionStart;
                adsr.noteOff();
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

    // Seed the audio side with the default single-slice table so voices
    // always have a valid pointer, even before the first setSliceTable.
    activeSliceTable = new std::vector<int>(sliceTableMsgThread);
    for (auto* v : phantomVoices) v->setSliceTable(activeSliceTable);
}

PhantomSampler::~PhantomSampler()
{
    delete activeSliceTable;
    delete incomingSliceTable.exchange(nullptr);
    delete retiredSliceTable.exchange(nullptr);
}

void PhantomSampler::prepareToPlay(double sampleRate, int blockSize)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    for (auto* v : phantomVoices) v->prepareStretcher(sampleRate, blockSize);
}

void PhantomSampler::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                      const juce::MidiBuffer& midi)
{
    // Adopt a freshly-published slice table — wait-free. Only proceeds
    // when the retire slot is empty so this thread never has to free the
    // displaced table; otherwise it retries next block.
    if (incomingSliceTable.load(std::memory_order_relaxed) != nullptr
        && retiredSliceTable.load(std::memory_order_relaxed) == nullptr)
    {
        if (const auto* fresh = incomingSliceTable.exchange(nullptr, std::memory_order_acq_rel))
        {
            retiredSliceTable.store(activeSliceTable, std::memory_order_release);
            activeSliceTable = fresh;
            for (auto* v : phantomVoices) v->setSliceTable(activeSliceTable);
        }
    }

    synth.renderNextBlock(outputBuffer, midi, 0, outputBuffer.getNumSamples());
}

bool PhantomSampler::loadSample(juce::AudioBuffer<float> decoded, double sampleRate)
{
    if (decoded.getNumSamples() == 0 || sampleRate <= 0.0) return false;
    reclaimRetiredSliceTable();

    auto sound = juce::SynthesiserSound::Ptr(
        new PhantomSamplerSound(std::move(decoded), sampleRate));

    // The Synthesiser serialises sound-list mutation against its render
    // under its own internal lock — the documented JUCE model for swapping
    // sounds from the message thread. Voices still playing the old sound
    // keep it alive via their own reference until the note ends.
    synth.clearSounds();
    synth.addSound(sound);
    currentSound = sound;
    return true;
}

void PhantomSampler::clearSample()
{
    reclaimRetiredSliceTable();
    synth.clearSounds();
    currentSound = nullptr;
}

bool PhantomSampler::hasSample() const noexcept
{
    return currentSound != nullptr;
}

double PhantomSampler::getLoadedSourceSampleRate() const noexcept
{
    if (auto* sound = dynamic_cast<PhantomSamplerSound*>(currentSound.get()))
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

void PhantomSampler::setReverse(bool r) noexcept
{
    for (auto* v : phantomVoices) v->setReverse(r);
}

void PhantomSampler::setLoopCrossfadeMs(float ms) noexcept
{
    for (auto* v : phantomVoices) v->setLoopCrossfadeMs(ms);
}

void PhantomSampler::setWarpMode(int m) noexcept
{
    for (auto* v : phantomVoices) v->setWarpMode(m);
}

void PhantomSampler::setSliceTable(std::vector<int> slices)
{
    // Sort + dedup. Always include 0 as the first slice so MIDI note 36
    // has a valid start point even on a sample with no detected transients.
    std::sort(slices.begin(), slices.end());
    slices.erase(std::unique(slices.begin(), slices.end()), slices.end());
    if (slices.empty() || slices.front() != 0)
        slices.insert(slices.begin(), 0);

    reclaimRetiredSliceTable();
    sliceTableMsgThread = slices;

    // Stage an immutable copy for the audio thread. A previously-staged
    // table the audio thread never adopted is superseded — safe to free
    // here since only this thread stages and only renderNextBlock adopts.
    auto* fresh = new std::vector<int>(std::move(slices));
    delete incomingSliceTable.exchange(fresh, std::memory_order_acq_rel);
}

void PhantomSampler::reclaimRetiredSliceTable() noexcept
{
    delete retiredSliceTable.exchange(nullptr, std::memory_order_acq_rel);
}

int PhantomSampler::detectSlices()
{
    // Energy-based onset detection over the loaded sound. Adequate for
    // percussive material; tonal samples often yield just slice 0 (full
    // sample) which is the right fallback.
    std::vector<int> hits;
    hits.push_back(0);   // slice 0 always at sample 0

    {
        if (auto* sound = dynamic_cast<PhantomSamplerSound*>(currentSound.get()))
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
    return (int) sliceTableMsgThread.size();
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
