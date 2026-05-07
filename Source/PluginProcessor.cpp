#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/NativePluginEditor.h"
#include "PresetMigration.h"
#include "EngineFocus.h"
#include "Modulation/Routing.h"

PhantomProcessor::PhantomProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",     juce::AudioChannelSet::stereo(), true)
        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PHANTOM_STATE", makeLayout()),
      dualEngineHost(apvts)
{
    // The Recipe Preset selector is per-engine: subscribe to both so picking a
    // preset from either tab populates that engine's harmonic amps.
    apvts.addParameterListener(ParamID::A_RECIPE_PRESET, this);
    apvts.addParameterListener(ParamID::B_RECIPE_PRESET, this);
    presetManager.initialize();

    // ── Modulation engines (PR3a) ─────────────────────────────────────
    // Engine A owns Macros 1+2 (scoped to a_* params); Engine B owns 3+4
    // (scoped to b_*). The Macro modulators back onto APVTS so host
    // automation drives them.
    using kaigen::phantom::Macro;
    modEngineA.addModulator(std::make_unique<Macro>("macro1", apvts, ParamID::MACRO1));
    modEngineA.addModulator(std::make_unique<Macro>("macro2", apvts, ParamID::MACRO2));
    modEngineB.addModulator(std::make_unique<Macro>("macro3", apvts, ParamID::MACRO3));
    modEngineB.addModulator(std::make_unique<Macro>("macro4", apvts, ParamID::MACRO4));

    // Wire the modulation engines into the per-block param sync. This must
    // happen after the engines are populated above so the host caches
    // pointers to fully-configured engines.
    dualEngineHost.setModulationEngines(&modEngineA, &modEngineB);
}

PhantomProcessor::~PhantomProcessor()
{
    apvts.removeParameterListener(ParamID::A_RECIPE_PRESET, this);
    apvts.removeParameterListener(ParamID::B_RECIPE_PRESET, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PhantomProcessor::makeLayout()
{
    return createParameterLayout();
}

void PhantomProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRate = sr;
    // Engines only ever process the main stereo bus; sidechain is read
    // separately and passed as a parameter to process(). Match the prior
    // behavior of hardcoding 2 here so sidechain configurations don't
    // silently double internal-buffer memory in both engines.
    dualEngineHost.prepareToPlay(sr, samplesPerBlock, 2);

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

    // Per-engine spectrum capture (split-mode view).
    fftBufferEngineA.fill(0.0f);
    fftBufferEngineB.fill(0.0f);
    fftWritePosEngineA.store(0, std::memory_order_relaxed);
    fftWritePosEngineB.store(0, std::memory_order_relaxed);
    fftScratchEngineA.fill(0.0f);
    fftScratchEngineB.fill(0.0f);
    samplesSinceEngineFftA = 0;
    samplesSinceEngineFftB = 0;
    for (auto& a : engineASpectrum) a.store(0.0f, std::memory_order_relaxed);
    for (auto& a : engineBSpectrum) a.store(0.0f, std::memory_order_relaxed);
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

    // ── MIDI events → both engines ────────────────────────────────────
    // The engines' own gating (setMidiTriggerEnabled / setMidiGateRelease)
    // decides whether to act on these — we just forward every event to both
    // engines. DualEngineHost fans out the call to engine A and B internally.
    for (const auto meta : midiMessages)
    {
        const auto& m = meta.getMessage();
        if      (m.isNoteOn())  dualEngineHost.handleMidiNoteOn();
        else if (m.isNoteOff()) dualEngineHost.handleMidiNoteOff();
    }

    // ── Input Gain → engine detection only ────────────────────────────
    // Buffer audio stays at unity. The gain is forwarded to both engines where
    // it scales the signal feeding the synth's period/gate/upward-expander
    // detectors only — so raising Input Gain helps the engines track quiet
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
    dualEngineHost.setInputDetectionGain(detectionGainLin);

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

            oscInputBuf[(size_t) oscInWp].store(inL[i], std::memory_order_relaxed);
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

                // (pitch is now sourced from the active engine's crossing tracker — see below)
            }
        }
        oscInputWrPos.store(oscInWp, std::memory_order_relaxed);
        peakInL.store(pL, std::memory_order_relaxed);
        peakInR.store(pR, std::memory_order_relaxed);
    }

    // ── Process through the dual-engine host ─────────────────────────
    // Read sidechain bus (bus index 1) if enabled. Both engines see the
    // same sidechain pointer; DualEngineHost forwards it to both internally.
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

    // The host owns the per-engine APVTS sync, runs both engines on the same
    // input, and crossfades into `buffer` (in place).
    dualEngineHost.process(buffer, sidechainPtr);

    // ── Per-engine FFT capture + transform (split-mode spectrum view) ──
    // aScratch / bScratch hold each engine's post-process / pre-crossfader
    // output. They're valid until the next dualEngineHost.process() call,
    // i.e. until the next processBlock — so capture them now into the two
    // ring buffers, then run the FFT on the audio thread on the same
    // sub-rate cadence as the input/output FFTs (one transform per
    // kFftSize samples accumulated). The bin magnitudes are published into
    // atomic snapshot arrays the WebView native binding reads as plain
    // atomic loads — no FFT, no allocation on the message thread.
    {
        const auto& aOut = dualEngineHost.getEngineAOutput();
        const auto& bOut = dualEngineHost.getEngineBOutput();

        if (aOut.getNumChannels() > 0 && bOut.getNumChannels() > 0
            && aOut.getNumSamples() == n && bOut.getNumSamples() == n)
        {
            const float* aL = aOut.getReadPointer(0);
            const float* bL = bOut.getReadPointer(0);
            int posA = fftWritePosEngineA.load(std::memory_order_relaxed);
            int posB = fftWritePosEngineB.load(std::memory_order_relaxed);
            for (int i = 0; i < n; ++i)
            {
                fftBufferEngineA[(size_t) posA] = aL[i];
                fftBufferEngineB[(size_t) posB] = bL[i];
                posA = (posA + 1) & kEngineRingMask;
                posB = (posB + 1) & kEngineRingMask;
            }
            fftWritePosEngineA.store(posA, std::memory_order_relaxed);
            fftWritePosEngineB.store(posB, std::memory_order_relaxed);

            samplesSinceEngineFftA += n;
            samplesSinceEngineFftB += n;

            // Shared log-bin parameters — reused for both engines below.
            const float srHz       = (float) sampleRate;
            const float fftSizeF   = (float) kFftSize;
            const int   maxBin     = kFftSize / 2 - 1;
            const float logMin     = std::log10(30.0f);
            const float logMax     = std::log10(16000.0f);
            const float normalizer = 2.0f / (float) (kFftSize / 2);

            // Helper: run Hann-windowed FFT + log-binning on `ring` starting
            // from the most-recent kFftSize samples ending at `wrPos`,
            // writing magnitudes into `dst`. Scratch is the per-engine
            // pre-allocated FFT buffer (size kFftSize * 2).
            auto runEngineFft = [&](const std::array<float, kFftSize * 2>& ring,
                                    int wrPos,
                                    std::array<float, kFftSize * 2>& scratch,
                                    std::array<std::atomic<float>, kSpectrumBins>& dst)
            {
                int readPos = (wrPos - kFftSize) & kEngineRingMask;
                for (int k = 0; k < kFftSize; ++k)
                {
                    const float w = 0.5f * (1.0f - std::cos(
                        juce::MathConstants<float>::twoPi * k / (float)(kFftSize - 1)));
                    scratch[(size_t) k] = ring[(size_t) readPos] * w;
                    readPos = (readPos + 1) & kEngineRingMask;
                }
                for (int k = kFftSize; k < kFftSize * 2; ++k)
                    scratch[(size_t) k] = 0.0f;

                spectrumFFT.performFrequencyOnlyForwardTransform(scratch.data());

                for (int b = 0; b < kSpectrumBins; ++b)
                {
                    const float fLow  = std::pow(10.0f, logMin + (logMax - logMin) *  b      / kSpectrumBins);
                    const float fHigh = std::pow(10.0f, logMin + (logMax - logMin) * (b + 1) / kSpectrumBins);
                    const int binLow  = juce::jmax(1,      (int) std::floor(fLow  * fftSizeF / srHz));
                    const int binHigh = juce::jmin(maxBin, (int) std::ceil (fHigh * fftSizeF / srHz));

                    float mag = 0.0f;
                    for (int k = binLow; k <= binHigh; ++k)
                        mag = juce::jmax(mag, scratch[(size_t) k]);

                    const float normMag = mag * normalizer;
                    const float dB      = juce::Decibels::gainToDecibels(normMag, -96.0f);
                    dst[(size_t) b].store(juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 60.0f),
                                          std::memory_order_relaxed);
                }
            };

            if (samplesSinceEngineFftA >= kFftSize)
            {
                samplesSinceEngineFftA = 0;
                runEngineFft(fftBufferEngineA, posA, fftScratchEngineA, engineASpectrum);
            }
            if (samplesSinceEngineFftB >= kFftSize)
            {
                samplesSinceEngineFftB = 0;
                runEngineFft(fftBufferEngineB, posB, fftScratchEngineB, engineBSpectrum);
            }
        }
    }

    // Pitch display: use the active engine's zero-crossing tracker — it reflects
    // exactly what the visible engine is synthesising and covers the full frequency
    // range (not FFT's 30-500 Hz). Returns 0 when input is quiet (so UI shows "---").
    currentPitch.store(getActiveEngine().getEstimatedHz(), std::memory_order_relaxed);

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

            oscOutputBuf[(size_t) oscOutWp].store(outL[i], std::memory_order_relaxed);
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
    // Recipe Preset is a per-engine choice: when either engine's selector changes,
    // populate that engine's H2..H8 amplitudes from the chosen recipe table.
    // "Custom" (index 6) leaves the harmonics untouched.
    const bool isA = (parameterID == ParamID::A_RECIPE_PRESET);
    const bool isB = (parameterID == ParamID::B_RECIPE_PRESET);
    if (!isA && !isB) return;

    const int idx = juce::roundToInt(newValue);
    const float* tables[] = {
        kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps,
        kStableAmps, kWeirdAmps,
        nullptr   // Custom (index 6)
    };

    if (idx < 0 || idx >= 6 || tables[idx] == nullptr) return;

    const char* hIds[7] = {
        isA ? ParamID::A_RECIPE_H2 : ParamID::B_RECIPE_H2,
        isA ? ParamID::A_RECIPE_H3 : ParamID::B_RECIPE_H3,
        isA ? ParamID::A_RECIPE_H4 : ParamID::B_RECIPE_H4,
        isA ? ParamID::A_RECIPE_H5 : ParamID::B_RECIPE_H5,
        isA ? ParamID::A_RECIPE_H6 : ParamID::B_RECIPE_H6,
        isA ? ParamID::A_RECIPE_H7 : ParamID::B_RECIPE_H7,
        isA ? ParamID::A_RECIPE_H8 : ParamID::B_RECIPE_H8,
    };
    for (int i = 0; i < 7; ++i)
        if (auto* p = apvts.getParameter(hIds[i]))
            p->setValueNotifyingHost(p->convertTo0to1(tables[idx][i] * 100.0f));
}

juce::AudioProcessorEditor* PhantomProcessor::createEditor()
{
    if (editorView.useNativeEditor)
        return new kaigen::phantom::NativePluginEditor(*this, apvts);
    return new PhantomEditor(*this);
}

void PhantomProcessor::setEngineFocus(EngineFocus newFocus) noexcept
{
    engineFocus = newFocus;
}

void PhantomProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // New format: a wrapper <PluginState> tree containing a single
    // <APVTSState> child. The APVTS's own state type ("PHANTOM_STATE") is
    // intentionally renamed at the wrapper level — PresetMigration and the
    // load path both look up the child by the literal name "APVTSState" so
    // future schema changes don't have to chase the APVTS-side identifier.
    // Legacy formats (with <SlotB>/<MorphConfig> children, or un-prefixed
    // params) are handled at load time by PresetMigration.
    auto srcState = apvts.copyState();
    juce::ValueTree apvtsChild("APVTSState");
    apvtsChild.copyPropertiesFrom(srcState, nullptr);
    for (int i = 0; i < srcState.getNumChildren(); ++i)
        apvtsChild.appendChild(srcState.getChild(i).createCopy(), nullptr);

    juce::ValueTree wrapper("PluginState");
    wrapper.appendChild(apvtsChild, nullptr);

    kaigen::phantom::writeEngineFocusToTree(wrapper, engineFocus);
    kaigen::phantom::writeSpectrumViewModeToTree(wrapper, spectrumViewMode);
    kaigen::phantom::writeMatrixViewToTree(wrapper, matrixView);
    kaigen::phantom::writeEditorViewToTree(wrapper, editorView);

    // <ModulationConfig> — per-engine modulator + routing tables. Preset-side
    // persistence (within an APVTS-state child or sibling) lands in PR3b; for
    // PR3a we just stage the data alongside <EditorFocus> / <SpectrumView>.
    juce::ValueTree modConfig("ModulationConfig");
    modConfig.appendChild(modEngineA.toValueTree(), nullptr);
    modConfig.appendChild(modEngineB.toValueTree(), nullptr);
    wrapper.appendChild(modConfig, nullptr);

    if (auto xml = wrapper.createXml())
        copyXmlToBinary(*xml, destData);
}

void PhantomProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = std::unique_ptr<juce::XmlElement>(getXmlFromBinary(data, sizeInBytes)))
    {
        auto wrapper = juce::ValueTree::fromXml(*xml);
        if (!wrapper.isValid()) return;

        // Run migration on whatever the host handed us. migrateInPlace is a
        // no-op on already-new-format states (idempotent), so this is safe to
        // run unconditionally.
        kaigen::phantom::PresetMigration::migrateInPlace(wrapper);

        // Locate the APVTS-state subtree. New format: <PluginState> wrapper with
        // an <APVTSState> child. Older format: the root IS the APVTS state.
        auto apvtsState = wrapper.getChildWithName("APVTSState");
        if (apvtsState.isValid())
        {
            // Re-cast to the APVTS's expected root type before replaceState —
            // some JUCE codepaths assert on the type matching apvts.state's.
            juce::ValueTree retyped(apvts.state.getType());
            retyped.copyPropertiesFrom(apvtsState, nullptr);
            for (int i = 0; i < apvtsState.getNumChildren(); ++i)
                retyped.appendChild(apvtsState.getChild(i).createCopy(), nullptr);
            apvts.replaceState(retyped);
        }
        else if (wrapper.getType() == apvts.state.getType())
        {
            apvts.replaceState(wrapper);
        }

        if (wrapper.getChildWithName("EditorFocus").isValid())
            engineFocus = kaigen::phantom::readEngineFocusFromTree(wrapper);
        // else: leave in-memory engineFocus untouched — preserves user state on
        // partial wrapper loads or on plugin-state restores from pre-PR2 hosts.

        if (wrapper.getChildWithName("SpectrumView").isValid())
            spectrumViewMode = kaigen::phantom::readSpectrumViewModeFromTree(wrapper);

        if (wrapper.getChildWithName("MatrixView").isValid())
            matrixView = kaigen::phantom::readMatrixViewFromTree(wrapper);

        if (wrapper.getChildWithName("EditorView").isValid())
            editorView = kaigen::phantom::readEditorViewFromTree(wrapper);

        // <ModulationConfig> — restore per-engine modulators + routings. The
        // ValueTree contains one <Engine> child per engine, each tagged with
        // a "prefix" property ("a_" or "b_"); dispatch on that to the right
        // engine. Missing or invalid nodes leave the in-memory state alone.
        if (auto modConfig = wrapper.getChildWithName("ModulationConfig"); modConfig.isValid())
        {
            for (int i = 0; i < modConfig.getNumChildren(); ++i)
            {
                auto engineNode = modConfig.getChild(i);
                if (! engineNode.hasType("Engine")) continue;
                const auto p = engineNode.getProperty("prefix").toString();
                if      (p == "a_") modEngineA.fromValueTree(engineNode);
                else if (p == "b_") modEngineB.fromValueTree(engineNode);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhantomProcessor();
}
