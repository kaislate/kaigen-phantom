#include "PluginProcessor.h"
#include "PluginEditor.h"

PhantomProcessor::PhantomProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",     juce::AudioChannelSet::stereo(), true)
        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PHANTOM_STATE", makeLayout())
{
    apvts.addParameterListener(ParamID::RECIPE_PRESET, this);
    presetManager.initialize();
}

PhantomProcessor::~PhantomProcessor()
{
    apvts.removeParameterListener(ParamID::RECIPE_PRESET, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PhantomProcessor::makeLayout()
{
    return createParameterLayout();
}

void PhantomProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRate = sr;
    engine.prepare(sr, samplesPerBlock, 2);

    // Auto input gain coefficients.
    // Per-sample rates converted to per-block by raising (1-alpha) to the power of blockSize,
    // which is the exact equivalent of running the IIR on every sample in the block.
    const float perSampleAttack  = 1.0f - std::exp(-1.0f / (0.005f  * (float) sr)); // 5ms
    const float perSampleRelease = 1.0f - std::exp(-1.0f / (0.400f  * (float) sr)); // 400ms
    const float perSampleSmooth  = 1.0f - std::exp(-1.0f / (0.050f  * (float) sr)); // 50ms
    const float n = (float) samplesPerBlock;
    autoAttackCoef  = 1.0f - std::pow(1.0f - perSampleAttack,  n);
    autoReleaseCoef = 1.0f - std::pow(1.0f - perSampleRelease, n);
    autoSmoothCoef  = 1.0f - std::pow(1.0f - perSampleSmooth,  n);
    autoEnvelope = 0.0f;
    autoGain     = 1.0f;

    // Pre-allocate sidechain buffer to avoid heap allocation on the audio thread
    const int scChannels = getChannelCountOfBus(true, 1);
    if (scChannels > 0)
        sidechainBuf.setSize(scChannels, samplesPerBlock, false, true, false);

    fftWritePos = 0;
    fftBuffer.fill(0.0f);
    spectrumData.fill(0.0f);
    spectrumReady.store(false);

    #ifdef KAIGEN_PRO_BUILD
    morph.prepareToPlay(sr, samplesPerBlock);
    #endif
}

void PhantomProcessor::releaseResources() {}

bool PhantomProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Accept stereo or mono main output. Input must match output channel count.
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::stereo() &&
        mainOut != juce::AudioChannelSet::mono())
        return false;

    // Input must match output (stereo→stereo or mono→mono)
    return mainIn == mainOut;
}

void PhantomProcessor::syncParamsToEngine()
{
    engine.setCrossoverHz    (apvts.getRawParameterValue(ParamID::PHANTOM_THRESHOLD)->load());
    engine.setPhantomStrength(apvts.getRawParameterValue(ParamID::PHANTOM_STRENGTH)->load() / 100.0f);
    engine.setSaturation     (apvts.getRawParameterValue(ParamID::HARMONIC_SATURATION)->load() / 100.0f);
    engine.setSynthStep      (apvts.getRawParameterValue(ParamID::SYNTH_STEP)->load() / 100.0f);
    engine.setSynthDuty      (apvts.getRawParameterValue(ParamID::SYNTH_DUTY)->load() / 100.0f);
    engine.setSynthSkip      ((int) apvts.getRawParameterValue(ParamID::SYNTH_SKIP)->load());
    engine.setGhostAmount  (apvts.getRawParameterValue(ParamID::GHOST)->load() / 100.0f);
    engine.setGhostMode    ((int) apvts.getRawParameterValue(ParamID::GHOST_MODE)->load());
    engine.setOutputGainDb (apvts.getRawParameterValue(ParamID::OUTPUT_GAIN)->load());
    engine.setEnvelopeAttackMs (apvts.getRawParameterValue(ParamID::ENV_ATTACK_MS)->load());
    engine.setEnvelopeReleaseMs(apvts.getRawParameterValue(ParamID::ENV_RELEASE_MS)->load());
    engine.setEnvSource((int) apvts.getRawParameterValue(ParamID::ENV_SOURCE)->load());
    engine.setBinauralMode ((int) apvts.getRawParameterValue(ParamID::BINAURAL_MODE)->load());
    engine.setBinauralWidth(apvts.getRawParameterValue(ParamID::BINAURAL_WIDTH)->load() / 100.0f);
    engine.setStereoWidth  (apvts.getRawParameterValue(ParamID::STEREO_WIDTH)->load() / 100.0f);
    engine.setSynthLPF(apvts.getRawParameterValue(ParamID::SYNTH_LPF_HZ)->load());
    engine.setSynthHPF(apvts.getRawParameterValue(ParamID::SYNTH_HPF_HZ)->load());
    {
        const int idx = (int) apvts.getRawParameterValue(ParamID::SYNTH_FILTER_SLOPE)->load();
        const int dBPerOct = (idx == 0) ? 6 : (idx == 2) ? 24 : 12;
        engine.setSynthFilterSlope(dBPerOct);
    }

    static const char* hIds[7] = {
        ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
        ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8
    };
    std::array<float, 7> amps;
    for (int i = 0; i < 7; ++i)
        amps[(size_t) i] = apvts.getRawParameterValue(hIds[i])->load() / 100.0f;
    engine.setHarmonicAmplitudes(amps);
    engine.setSynthMode((int) apvts.getRawParameterValue(ParamID::MODE)->load());
    engine.setWaveletLength(apvts.getRawParameterValue(ParamID::SYNTH_WAVELET_LENGTH)->load() / 100.0f);
    engine.setGateThreshold(apvts.getRawParameterValue(ParamID::SYNTH_GATE_THRESHOLD)->load() / 100.0f);
    engine.setH1Amplitude  (apvts.getRawParameterValue(ParamID::SYNTH_H1)->load() / 100.0f);
    engine.setSubAmplitude (apvts.getRawParameterValue(ParamID::SYNTH_SUB)->load() / 100.0f);
    engine.setMinPeriodSamples(apvts.getRawParameterValue(ParamID::SYNTH_MIN_SAMPLES)->load());
    engine.setMaxPeriodSamples(apvts.getRawParameterValue(ParamID::SYNTH_MAX_SAMPLES)->load());
    engine.setTrackingSpeed(apvts.getRawParameterValue(ParamID::TRACKING_SPEED)->load() / 100.0f);
    engine.setUsePunch     (apvts.getRawParameterValue(ParamID::PUNCH_ENABLED)->load() > 0.5f);
    engine.setPunchAmount  (apvts.getRawParameterValue(ParamID::PUNCH_AMOUNT)->load() / 100.0f);
    engine.setBoostThreshold(apvts.getRawParameterValue(ParamID::SYNTH_BOOST_THRESHOLD)->load() / 100.0f);
    engine.setBoostAmount   (apvts.getRawParameterValue(ParamID::SYNTH_BOOST_AMOUNT)->load() / 100.0f);
    engine.setMidiTriggerEnabled(apvts.getRawParameterValue(ParamID::MIDI_TRIGGER_ENABLED)->load() > 0.5f);
    engine.setMidiGateRelease   (apvts.getRawParameterValue(ParamID::MIDI_GATE_RELEASE)   ->load() > 0.5f);
}

void PhantomProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int n   = buffer.getNumSamples();
    const int nCh = juce::jmin(buffer.getNumChannels(), 2);
    if (n == 0 || nCh == 0) return;

    // ── Bypass ────────────────────────────────────────────────────────
    if (apvts.getRawParameterValue(ParamID::BYPASS)->load() > 0.5f)
    {
        float pL = 0, pR = 0;
        const float* inL = buffer.getReadPointer(0);
        const float* inR = (nCh > 1) ? buffer.getReadPointer(1) : inL;
        for (int i = 0; i < n; ++i)
        {
            pL = juce::jmax(pL, std::abs(inL[i]));
            pR = juce::jmax(pR, std::abs(inR[i]));
        }
        peakInL .store(pL, std::memory_order_relaxed);
        peakInR .store(pR, std::memory_order_relaxed);
        peakOutL.store(pL, std::memory_order_relaxed);
        peakOutR.store(pR, std::memory_order_relaxed);
        return;
    }

    // ── MIDI events → engine ──────────────────────────────────────────
    // The engine's own gating (setMidiTriggerEnabled / setMidiGateRelease)
    // decides whether to act on these — we just forward every event.
    for (const auto meta : midiMessages)
    {
        const auto& m = meta.getMessage();
        if      (m.isNoteOn())  engine.handleMidiNoteOn();
        else if (m.isNoteOff()) engine.handleMidiNoteOff();
    }

    // ── Input Gain → engine detection only ────────────────────────────
    // Buffer audio stays at unity. The gain is forwarded to the engine where
    // it scales the signal feeding the synth's period/gate/upward-expander
    // detectors only — so raising Input Gain helps the engine track quiet
    // material without raising output level.
    float detectionGainLin;
    {
        const bool autoMode = apvts.getRawParameterValue(ParamID::INPUT_GAIN_AUTO)->load() > 0.5f;

        if (autoMode)
        {
            float blockPeak = 0.0f;
            for (int c = 0; c < nCh; ++c)
            {
                const float* ch = buffer.getReadPointer(c);
                for (int i = 0; i < n; ++i)
                    blockPeak = juce::jmax(blockPeak, std::abs(ch[i]));
            }

            const float coef = (blockPeak > autoEnvelope) ? autoAttackCoef : autoReleaseCoef;
            autoEnvelope += coef * (blockPeak - autoEnvelope);

            constexpr float kTarget  = 0.25f;   // -12 dBFS target for detection
            constexpr float kFloor   = 0.001f;  // below -60 dBFS, hold current gain
            constexpr float kMaxGain = 16.0f;   // +24 dB ceiling
            const float desiredGain = (autoEnvelope > kFloor)
                ? juce::jmin(kMaxGain, kTarget / autoEnvelope)
                : autoGain;

            autoGain += autoSmoothCoef * (desiredGain - autoGain);
            detectionGainLin = autoGain;
        }
        else
        {
            autoEnvelope = 0.0f;
            autoGain     = 1.0f;

            detectionGainLin = juce::Decibels::decibelsToGain(
                apvts.getRawParameterValue(ParamID::INPUT_GAIN)->load());
        }
    }
    engine.setInputDetectionGain(detectionGainLin);

    // ── Input peak levels + FFT/pitch capture (pre-engine) ──────────
    // Reading input here ensures pitch detection sees the dry fundamental,
    // not the phantom harmonics added by the engine. Spectrum also shows
    // input so the threshold crossover is clearly visible.
    {
        float pL = 0, pR = 0;
        const float* inL = buffer.getReadPointer(0);
        const float* inR = (nCh > 1) ? buffer.getReadPointer(1) : inL;
        int oscInWp = oscInputWrPos.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            pL = juce::jmax(pL, std::abs(inL[i]));
            pR = juce::jmax(pR, std::abs(inR[i]));

            oscInputBuf[(size_t) oscInWp] = inL[i];
            oscInWp = (oscInWp + 1) & (kOscBufSize - 1);

            fftBuffer[(size_t) fftWritePos++] = inL[i];

            if (fftWritePos >= kFftSize)
            {
                fftWritePos = 0;

                // Hann window
                for (int k = 0; k < kFftSize; ++k)
                {
                    const float w = 0.5f * (1.0f - std::cos(
                        juce::MathConstants<float>::twoPi * k / (float)(kFftSize - 1)));
                    fftBuffer[(size_t) k] *= w;
                }
                for (int k = kFftSize; k < kFftSize * 2; ++k)
                    fftBuffer[(size_t) k] = 0.0f;

                spectrumFFT.performFrequencyOnlyForwardTransform(fftBuffer.data());

                const float sr       = (float) sampleRate;
                const float fftSizeF = (float) kFftSize;
                const int   maxBin   = kFftSize / 2 - 1;
                const float logMin   = std::log10(30.0f);
                const float logMax   = std::log10(16000.0f);
                const float normalizer = 2.0f / (float) (kFftSize / 2);

                for (int b = 0; b < kSpectrumBins; ++b)
                {
                    const float fLow  = std::pow(10.0f, logMin + (logMax - logMin) *  b      / kSpectrumBins);
                    const float fHigh = std::pow(10.0f, logMin + (logMax - logMin) * (b + 1) / kSpectrumBins);

                    const int binLow  = juce::jmax(1,      (int) std::floor(fLow  * fftSizeF / sr));
                    const int binHigh = juce::jmin(maxBin, (int) std::ceil (fHigh * fftSizeF / sr));

                    float mag = 0.0f;
                    for (int k = binLow; k <= binHigh; ++k)
                        mag = juce::jmax(mag, fftBuffer[(size_t) k]);

                    const float normMag = mag * normalizer;
                    const float dB      = juce::Decibels::gainToDecibels(normMag, -96.0f);
                    spectrumData[(size_t) b] = juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 60.0f);
                }

                spectrumReady.store(true, std::memory_order_release);

                // (pitch is now sourced from the synth's crossing tracker — see below)
            }
        }
        oscInputWrPos.store(oscInWp, std::memory_order_relaxed);
        peakInL.store(pL, std::memory_order_relaxed);
        peakInR.store(pR, std::memory_order_relaxed);
    }

    #ifdef KAIGEN_PRO_BUILD
        morph.preProcessBlock();   // apply arc interpolations for this block
    #endif

    // ── Sync params → engine ──────────────────────────────────────────
    syncParamsToEngine();

    // ── Process through the waveshaper engine ────────────────────────
    // Read sidechain bus (bus index 1) if enabled
    const juce::AudioBuffer<float>* sidechainPtr = nullptr;
    {
        const int nSCBusChannels = getChannelCountOfBus(true, 1);
        if (nSCBusChannels > 0)
        {
            // Find where sidechain channels start in the processBlock buffer.
            // Main input (bus 0) is stereo (2 channels); sidechain starts at ch 2.
            const int scStartCh = getTotalNumInputChannels() - nSCBusChannels;
            if (scStartCh >= 0 && scStartCh + nSCBusChannels <= buffer.getNumChannels()
                && buffer.getNumSamples() <= sidechainBuf.getNumSamples())
            {
                // Use pre-allocated member buffer — avoidReallocating is safe because
                // we guard against oversized blocks above.
                sidechainBuf.setSize(nSCBusChannels, buffer.getNumSamples(), false, false, true);
                for (int c = 0; c < nSCBusChannels; ++c)
                    sidechainBuf.copyFrom(c, 0, buffer, scStartCh + c, 0, buffer.getNumSamples());
                sidechainPtr = &sidechainBuf;
            }
        }
    }
    engine.process(buffer, sidechainPtr);

  #ifdef KAIGEN_PRO_BUILD
    morph.postProcessBlock(buffer, sidechainPtr);
  #endif

    // Pitch display: use the synth's zero-crossing tracker directly — it reflects exactly
    // what is being synthesised and covers the full frequency range (not FFT's 30-500 Hz).
    // Returns 0 when input is quiet (so UI shows "---").
    currentPitch.store(engine.getEstimatedHz(), std::memory_order_relaxed);

    // ── Output peak levels + output spectrum FFT ─────────────────────
    {
        float pL = 0, pR = 0;
        const float* outL = buffer.getReadPointer(0);
        const float* outR = (nCh > 1) ? buffer.getReadPointer(1) : outL;
        int oscOutWp = oscOutputWrPos.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            pL = juce::jmax(pL, std::abs(outL[i]));
            pR = juce::jmax(pR, std::abs(outR[i]));

            oscOutputBuf[(size_t) oscOutWp] = outL[i];
            oscOutWp = (oscOutWp + 1) & (kOscBufSize - 1);

            fftOutputBuffer[(size_t) fftOutputWritePos++] = outL[i];

            if (fftOutputWritePos >= kFftSize)
            {
                fftOutputWritePos = 0;

                // Hann window
                for (int k = 0; k < kFftSize; ++k)
                {
                    const float w = 0.5f * (1.0f - std::cos(
                        juce::MathConstants<float>::twoPi * k / (float)(kFftSize - 1)));
                    fftOutputBuffer[(size_t) k] *= w;
                }
                for (int k = kFftSize; k < kFftSize * 2; ++k)
                    fftOutputBuffer[(size_t) k] = 0.0f;

                spectrumFFT.performFrequencyOnlyForwardTransform(fftOutputBuffer.data());

                const float sr2       = (float) sampleRate;
                const float fftSizeF2 = (float) kFftSize;
                const int   maxBin2   = kFftSize / 2 - 1;
                const float logMin2   = std::log10(30.0f);
                const float logMax2   = std::log10(16000.0f);
                const float norm2     = 2.0f / (float) (kFftSize / 2);

                for (int b = 0; b < kSpectrumBins; ++b)
                {
                    const float fLow  = std::pow(10.0f, logMin2 + (logMax2 - logMin2) *  b      / kSpectrumBins);
                    const float fHigh = std::pow(10.0f, logMin2 + (logMax2 - logMin2) * (b + 1) / kSpectrumBins);
                    const int binLow  = juce::jmax(1,      (int) std::floor(fLow  * fftSizeF2 / sr2));
                    const int binHigh = juce::jmin(maxBin2, (int) std::ceil (fHigh * fftSizeF2 / sr2));

                    float mag = 0.0f;
                    for (int k = binLow; k <= binHigh; ++k)
                        mag = juce::jmax(mag, fftOutputBuffer[(size_t) k]);

                    const float normMag = mag * norm2;
                    const float dB      = juce::Decibels::gainToDecibels(normMag, -96.0f);
                    spectrumOutputData[(size_t) b] = juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 60.0f);
                }
            }
        }

        oscOutputWrPos.store(oscOutWp, std::memory_order_relaxed);
        peakOutL.store(pL, std::memory_order_relaxed);
        peakOutR.store(pR, std::memory_order_relaxed);
    }
}

void PhantomProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::RECIPE_PRESET)
    {
        const int idx = juce::roundToInt(newValue);
        const float* tables[] = {
            kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps,
            kStableAmps, kWeirdAmps,
            nullptr   // Custom (index 6)
        };

        if (idx >= 0 && idx < 6 && tables[idx] != nullptr)
        {
            const char* hIds[] = {
                ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
                ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8
            };
            for (int i = 0; i < 7; ++i)
                if (auto* p = apvts.getParameter(hIds[i]))
                    p->setValueNotifyingHost(p->convertTo0to1(tables[idx][i] * 100.0f));
        }
    }
}

juce::AudioProcessorEditor* PhantomProcessor::createEditor()
{
    return new PhantomEditor(*this);
}

void PhantomProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    abSlots.syncActiveSlotFromLive();

    auto state = apvts.copyState();

    if (auto existing = state.getChildWithName("ABSlots"); existing.isValid())
        state.removeChild(existing, nullptr);
    state.appendChild(abSlots.toStateTree(), nullptr);

  #ifdef KAIGEN_PRO_BUILD
    if (auto existing = state.getChildWithName("MorphState"); existing.isValid())
        state.removeChild(existing, nullptr);
    state.appendChild(morph.toStateTree(), nullptr);
  #endif

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PhantomProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml(*xml);

    auto abSlotsTree = tree.getChildWithName("ABSlots");
    if (abSlotsTree.isValid())
        tree.removeChild(abSlotsTree, nullptr);

  #ifdef KAIGEN_PRO_BUILD
    auto morphStateTree = tree.getChildWithName("MorphState");
    if (morphStateTree.isValid())
        tree.removeChild(morphStateTree, nullptr);
  #endif

    apvts.replaceState(tree);
    abSlots.fromStateTree(abSlotsTree);

  #ifdef KAIGEN_PRO_BUILD
    morph.fromStateTree(morphStateTree);
  #endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhantomProcessor();
}
