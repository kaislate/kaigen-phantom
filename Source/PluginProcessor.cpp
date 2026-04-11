#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include <cmath>

PhantomProcessor::PhantomProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",     juce::AudioChannelSet::stereo(), true)
        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PHANTOM_STATE", createParameterLayout())
{
    apvts.addParameterListener(ParamID::RECIPE_PRESET, this);
}

PhantomProcessor::~PhantomProcessor()
{
    apvts.removeParameterListener(ParamID::RECIPE_PRESET, this);
}

void PhantomProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    pitchTracker.prepare(sampleRate, samplesPerBlock);
    harmonicGen .prepare(sampleRate, samplesPerBlock);
    binauralStage.prepare(sampleRate, samplesPerBlock);
    perceptualOpt.prepare(sampleRate, samplesPerBlock);
    crossoverBlend.prepare(sampleRate, samplesPerBlock);

    this->sampleRate = sampleRate;
    stratStagger.setDelayMs(8.0f, sampleRate);

    phantomBuf.setSize(2, samplesPerBlock, false, false, false);
    dryBuf    .setSize(2, samplesPerBlock, false, false, false);
    lastDeconflictionMode = -1;
}

void PhantomProcessor::releaseResources()
{
    pitchTracker.reset();
    harmonicGen.reset();
    binauralStage.reset();
    crossoverBlend.reset();
}

void PhantomProcessor::syncEnginesFromApvts(bool isInstrumentMode)
{
    using namespace ParamID;

    const float ghost    = apvts.getRawParameterValue(GHOST)->load() / 100.0f;
    const int   ghostModeIdx = (int)apvts.getRawParameterValue(GHOST_MODE)->load();
    crossoverBlend.setGhost(ghost);
    crossoverBlend.setGhostMode(ghostModeIdx == 0 ? GhostMode::Replace : GhostMode::Add);

    const float threshold = apvts.getRawParameterValue(PHANTOM_THRESHOLD)->load();
    crossoverBlend.setThresholdHz(threshold);

    const float gainDb = apvts.getRawParameterValue(OUTPUT_GAIN)->load();
    crossoverBlend.setOutputGain(std::pow(10.0f, gainDb / 20.0f));

    crossoverBlend.setStereoWidth(apvts.getRawParameterValue(STEREO_WIDTH)->load() / 100.0f);

    crossoverBlend.setSidechainDuckAmount(
        apvts.getRawParameterValue(SIDECHAIN_DUCK_AMOUNT)->load() / 100.0f);
    crossoverBlend.setDuckAttackMs(apvts.getRawParameterValue(SIDECHAIN_DUCK_ATTACK)->load());
    crossoverBlend.setDuckReleaseMs(apvts.getRawParameterValue(SIDECHAIN_DUCK_RELEASE)->load());

    harmonicGen.setPhantomStrength(
        apvts.getRawParameterValue(PHANTOM_STRENGTH)->load() / 100.0f);

    static const char* ampIDs[7] = {
        RECIPE_H2, RECIPE_H3, RECIPE_H4, RECIPE_H5, RECIPE_H6, RECIPE_H7, RECIPE_H8
    };
    for (int i = 0; i < 7; ++i)
        harmonicGen.setHarmonicAmp(i + 2, apvts.getRawParameterValue(ampIDs[i])->load() / 100.0f);

    static const char* phaseIDs[7] = {
        RECIPE_PHASE_H2, RECIPE_PHASE_H3, RECIPE_PHASE_H4, RECIPE_PHASE_H5,
        RECIPE_PHASE_H6, RECIPE_PHASE_H7, RECIPE_PHASE_H8
    };
    for (int i = 0; i < 7; ++i)
        harmonicGen.setHarmonicPhase(i + 2, apvts.getRawParameterValue(phaseIDs[i])->load());

    harmonicGen.setRotation(apvts.getRawParameterValue(RECIPE_ROTATION)->load());
    harmonicGen.setSaturation(apvts.getRawParameterValue(HARMONIC_SATURATION)->load() / 100.0f);

    if (!isInstrumentMode)
    {
        const float sensitivity = apvts.getRawParameterValue(TRACKING_SENSITIVITY)->load() / 100.0f;
        pitchTracker.setConfidenceThreshold(0.30f - sensitivity * 0.25f);
        pitchTracker.setGlideMs(apvts.getRawParameterValue(TRACKING_GLIDE)->load());
    }

    if (isInstrumentMode)
    {
        const int deconMode = (int)apvts.getRawParameterValue(DECONFLICTION_MODE)->load();
        if (deconMode != lastDeconflictionMode)
        {
            updateDeconflictionStrategy(deconMode);
            lastDeconflictionMode = deconMode;
        }
        harmonicGen.setMaxVoices((int)apvts.getRawParameterValue(MAX_VOICES)->load());
        stratStagger.setDelayMs(apvts.getRawParameterValue(STAGGER_DELAY)->load(),
                                sampleRate);
    }

    const int binMode = (int)apvts.getRawParameterValue(BINAURAL_MODE)->load();
    binauralStage.setMode(binMode == 0 ? BinauralMode::Off
                        : binMode == 1 ? BinauralMode::Spread
                                       : BinauralMode::VoiceSplit);
    binauralStage.setWidth(apvts.getRawParameterValue(BINAURAL_WIDTH)->load() / 100.0f);
}

void PhantomProcessor::updateDeconflictionStrategy(int modeIndex)
{
    IDeconflictionStrategy* strategies[] = {
        &stratPartition, &stratLane, &stratStagger,
        &stratOddEven,   &stratResidue, &stratBinaural
    };
    harmonicGen.setDeconflictionStrategy(
        (modeIndex >= 0 && modeIndex < 6) ? strategies[modeIndex] : nullptr);
}

void PhantomProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    processBlockCount.fetch_add(1, std::memory_order_relaxed);

    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();
    if (numCh < 2 || n == 0) return;

    // Bypass — pass audio straight through, skip all processing
    if (apvts.getRawParameterValue(ParamID::BYPASS)->load() > 0.5f)
    {
        // Still update peak meters so the UI shows signal
        const float* inL  = buffer.getReadPointer(0);
        const float* inR  = (buffer.getNumChannels() > 1) ? buffer.getReadPointer(1) : inL;
        float pL = 0.0f, pR = 0.0f;
        for (int i = 0; i < n; ++i) { pL = juce::jmax(pL, std::abs(inL[i])); pR = juce::jmax(pR, std::abs(inR[i])); }
        peakInL .store(pL, std::memory_order_relaxed);
        peakInR .store(pR, std::memory_order_relaxed);
        peakOutL.store(pL, std::memory_order_relaxed);
        peakOutR.store(pR, std::memory_order_relaxed);
        return;
    }

    const bool isInstrumentMode = ((int)apvts.getRawParameterValue(ParamID::MODE)->load() == 1);
    syncEnginesFromApvts(isInstrumentMode);

    if (isInstrumentMode)
    {
        for (const auto msg : midi)
        {
            const auto m = msg.getMessage();
            if (m.isNoteOn() && m.getVelocity() > 0)
            {
                harmonicGen.noteOn(m.getNoteNumber(), m.getVelocity());
                // Update perceptual optimizer with note pitch
                const float noteHz = 440.0f * std::pow(2.0f, (m.getNoteNumber() - 69) / 12.0f);
                perceptualOpt.setFundamental(noteHz);
                currentPitch.store(noteHz, std::memory_order_relaxed);
            }
            else if (m.isNoteOff() || (m.isNoteOn() && m.getVelocity() == 0))
                harmonicGen.noteOff(m.getNoteNumber());
        }
    }

    if (!isInstrumentMode)
    {
        // Compute input RMS — gate harmonics when signal is below threshold
        float rms = 0.0f;
        {
            const float* in = buffer.getReadPointer(0);
            for (int i = 0; i < n; ++i) rms += in[i] * in[i];
            rms = std::sqrt(rms / (float)n);
        }
        const bool signalPresent = (rms > 0.002f);  // ~-54 dBFS gate

        const float detectedHz = pitchTracker.detectPitch(buffer.getReadPointer(0), n);
        currentPitch.store(signalPresent ? pitchTracker.getSmoothedPitch() : -1.0f,
                           std::memory_order_relaxed);

        // Always update pitch — pass -1 when no signal to deactivate the effect voice
        const float pitchToUse = (signalPresent && detectedHz > 0.0f) ? detectedHz : -1.0f;
        harmonicGen.setEffectModePitch(pitchToUse);
        if (pitchToUse > 0.0f)
            perceptualOpt.setFundamental(pitchToUse);
    }

    phantomBuf.clear();
    harmonicGen.process(phantomBuf);

    binauralStage.process(phantomBuf);
    perceptualOpt.process(phantomBuf);

    const juce::AudioBuffer<float>* sidechainBuf = nullptr;
    juce::AudioBuffer<float> scBufStorage;
    if (getBusCount(true) > 1)
    {
        auto* scBus = getBus(true, 1);
        if (scBus != nullptr && scBus->isEnabled())
        {
            scBufStorage = getBusBuffer(buffer, true, 1);
            sidechainBuf = &scBufStorage;
        }
    }

    // Copy dry to pre-allocated member buffer (crossoverBlend modifies in-place)
    for (int ch = 0; ch < numCh; ++ch)
        dryBuf.copyFrom(ch, 0, buffer, ch, 0, n);

    crossoverBlend.process(dryBuf, phantomBuf, sidechainBuf);

    for (int ch = 0; ch < numCh; ++ch)
        buffer.copyFrom(ch, 0, dryBuf, ch, 0, n);

    // -------------------------------------------------------------------------
    // Peak levels — input from dryBuf (pre-crossover copy), output from buffer
    // -------------------------------------------------------------------------
    {
        const float* inL  = dryBuf.getReadPointer(0);
        const float* inR  = (dryBuf.getNumChannels() > 1) ? dryBuf.getReadPointer(1) : inL;
        const float* outL = buffer.getReadPointer(0);
        const float* outR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer(1) : outL;

        float pInL = 0.0f, pInR = 0.0f, pOutL = 0.0f, pOutR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            pInL  = juce::jmax(pInL,  std::abs(inL[i]));
            pInR  = juce::jmax(pInR,  std::abs(inR[i]));
            pOutL = juce::jmax(pOutL, std::abs(outL[i]));
            pOutR = juce::jmax(pOutR, std::abs(outR[i]));
        }
        peakInL .store(pInL,  std::memory_order_relaxed);
        peakInR .store(pInR,  std::memory_order_relaxed);
        peakOutL.store(pOutL, std::memory_order_relaxed);
        peakOutR.store(pOutR, std::memory_order_relaxed);
    }

    // -------------------------------------------------------------------------
    // Spectrum FFT — accumulate output samples; compute when kFftSize collected
    // -------------------------------------------------------------------------
    {
        const float* outL = buffer.getReadPointer(0);

        for (int i = 0; i < n; ++i)
        {
            fftBuffer[(size_t)fftWritePos++] = outL[i];

            if (fftWritePos >= kFftSize)
            {
                fftWritePos = 0;

                // Apply Hann window to first kFftSize samples
                for (int k = 0; k < kFftSize; ++k)
                {
                    const float w = 0.5f * (1.0f - std::cos(
                        juce::MathConstants<float>::twoPi * k / (float)(kFftSize - 1)));
                    fftBuffer[(size_t)k] *= w;
                }

                // Zero-pad upper half
                for (int k = kFftSize; k < kFftSize * 2; ++k)
                    fftBuffer[(size_t)k] = 0.0f;

                // In-place forward FFT (magnitudes in first kFftSize/2 entries)
                spectrumFFT.performFrequencyOnlyForwardTransform(fftBuffer.data());

                fftRunCount.fetch_add(1, std::memory_order_relaxed);
                // Track max magnitude for debugging
                float localMax = 0.0f;
                for (int k = 1; k < kFftSize / 2; ++k)
                    localMax = juce::jmax(localMax, fftBuffer[(size_t)k]);
                fftMaxMagnitude.store(localMax, std::memory_order_relaxed);

                // Bin into 80 log-spaced bands: 30 Hz – 16 kHz
                const float sr      = static_cast<float>(sampleRate);
                const float fftSizeF = static_cast<float>(kFftSize);
                const int   maxBin   = kFftSize / 2 - 1;  // Nyquist is at kFftSize/2
                const float logMin  = std::log10(30.0f);
                const float logMax  = std::log10(16000.0f);

                // Normalization factor: JUCE FFT outputs unnormalized magnitudes.
                // A full-scale sine wave produces magnitude ~kFftSize/4 per bin.
                const float normalizer = 2.0f / (float)(kFftSize / 2);

                for (int b = 0; b < kSpectrumBins; ++b)
                {
                    const float fLow  = std::pow(10.0f, logMin + (logMax - logMin) *  b      / kSpectrumBins);
                    const float fHigh = std::pow(10.0f, logMin + (logMax - logMin) * (b + 1) / kSpectrumBins);

                    const int binLow  = juce::jmax(1,      static_cast<int>(std::floor(fLow  * fftSizeF / sr)));
                    const int binHigh = juce::jmin(maxBin, static_cast<int>(std::ceil (fHigh * fftSizeF / sr)));

                    float mag = 0.0f;
                    for (int k = binLow; k <= binHigh; ++k)
                        mag = juce::jmax(mag, fftBuffer[(size_t)k]);

                    // Normalize to 0-1 range, then convert to dB scale (-60 to 0)
                    const float normMag = mag * normalizer;
                    const float dB      = juce::Decibels::gainToDecibels(normMag, -96.0f);
                    spectrumData[(size_t)b] = juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 60.0f);
                }

                spectrumReady.store(true, std::memory_order_release);
            }
        }
    }
}

void PhantomProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::RECIPE_PRESET)
    {
        int idx = juce::roundToInt(newValue);
        const float* tables[] = { kWarmAmps, kAggressiveAmps, kHollowAmps, kDenseAmps, nullptr };

        if (idx >= 0 && idx < 4 && tables[idx] != nullptr)
        {
            const char* hIds[] = {
                ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
                ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8
            };
            for (int i = 0; i < 7; ++i)
                if (auto* p = apvts.getParameter(hIds[i]))
                    p->setValueNotifyingHost(p->convertTo0to1(tables[idx][i] * 100.0f));
        }
        // Index 4 = Custom — don't overwrite, user has manual control
    }
}

juce::AudioProcessorEditor* PhantomProcessor::createEditor()
{
    return new PhantomEditor(*this);
}

void PhantomProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PhantomProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout
PhantomProcessor::createParameterLayout()
{
    return ::createParameterLayout();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhantomProcessor();
}
