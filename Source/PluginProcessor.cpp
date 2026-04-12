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
    fftWritePos = 0;
    fftBuffer.fill(0.0f);
    spectrumData.fill(0.0f);
    spectrumReady.store(false);
}

void PhantomProcessor::releaseResources() {}

void PhantomProcessor::syncParamsToEngine()
{
    engine.setCrossoverHz  (apvts.getRawParameterValue(ParamID::PHANTOM_THRESHOLD)->load());
    engine.setPhantomStrength(apvts.getRawParameterValue(ParamID::PHANTOM_STRENGTH)->load() / 100.0f);
    engine.setDrive        (apvts.getRawParameterValue(ParamID::HARMONIC_SATURATION)->load() / 100.0f);
    engine.setSaturation   (apvts.getRawParameterValue(ParamID::HARMONIC_SATURATION)->load() / 100.0f);
    engine.setGhostAmount  (apvts.getRawParameterValue(ParamID::GHOST)->load() / 100.0f);
    engine.setGhostReplace (((int) apvts.getRawParameterValue(ParamID::GHOST_MODE)->load()) == 0);
    engine.setOutputGainDb (apvts.getRawParameterValue(ParamID::OUTPUT_GAIN)->load());
    engine.setEnvelopeAttackMs (apvts.getRawParameterValue(ParamID::ENV_ATTACK_MS)->load());
    engine.setEnvelopeReleaseMs(apvts.getRawParameterValue(ParamID::ENV_RELEASE_MS)->load());
    engine.setBinauralMode ((int) apvts.getRawParameterValue(ParamID::BINAURAL_MODE)->load());
    engine.setBinauralWidth(apvts.getRawParameterValue(ParamID::BINAURAL_WIDTH)->load() / 100.0f);
    engine.setStereoWidth  (apvts.getRawParameterValue(ParamID::STEREO_WIDTH)->load() / 100.0f);

    static const char* hIds[7] = {
        ParamID::RECIPE_H2, ParamID::RECIPE_H3, ParamID::RECIPE_H4,
        ParamID::RECIPE_H5, ParamID::RECIPE_H6, ParamID::RECIPE_H7, ParamID::RECIPE_H8
    };
    std::array<float, 7> amps;
    for (int i = 0; i < 7; ++i)
        amps[(size_t) i] = apvts.getRawParameterValue(hIds[i])->load() / 100.0f;
    engine.setHarmonicAmplitudes(amps);
}

void PhantomProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    // ── Input peak levels ─────────────────────────────────────────────
    {
        float pL = 0, pR = 0;
        const float* inL = buffer.getReadPointer(0);
        const float* inR = (nCh > 1) ? buffer.getReadPointer(1) : inL;
        for (int i = 0; i < n; ++i)
        {
            pL = juce::jmax(pL, std::abs(inL[i]));
            pR = juce::jmax(pR, std::abs(inR[i]));
        }
        peakInL.store(pL, std::memory_order_relaxed);
        peakInR.store(pR, std::memory_order_relaxed);
    }

    // ── Sync params → engine ──────────────────────────────────────────
    syncParamsToEngine();

    // ── Process through the waveshaper engine ────────────────────────
    engine.process(buffer);

    // ── Output peak levels + input "pitch" (just peak level for UI) ──
    {
        float pL = 0, pR = 0;
        const float* outL = buffer.getReadPointer(0);
        const float* outR = (nCh > 1) ? buffer.getReadPointer(1) : outL;
        for (int i = 0; i < n; ++i)
        {
            pL = juce::jmax(pL, std::abs(outL[i]));
            pR = juce::jmax(pR, std::abs(outR[i]));
        }
        peakOutL.store(pL, std::memory_order_relaxed);
        peakOutR.store(pR, std::memory_order_relaxed);
    }

    // ── Spectrum FFT ──────────────────────────────────────────────────
    {
        const float* outL = buffer.getReadPointer(0);
        for (int i = 0; i < n; ++i)
        {
            fftBuffer[(size_t) fftWritePos++] = outL[i];

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

                const float sr      = (float) sampleRate;
                const float fftSizeF = (float) kFftSize;
                const int   maxBin   = kFftSize / 2 - 1;
                const float logMin  = std::log10(30.0f);
                const float logMax  = std::log10(16000.0f);
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

                // Detect dominant frequency from FFT (simple peak-bin method)
                // Search bins from ~30Hz to ~500Hz for the strongest peak
                const int lowSearchBin  = juce::jmax(1, (int)(30.0f  * fftSizeF / sr));
                const int highSearchBin = juce::jmin(maxBin, (int)(500.0f * fftSizeF / sr));
                float peakMag = 0.0f;
                int   peakBin = 0;
                for (int k = lowSearchBin; k <= highSearchBin; ++k)
                {
                    if (fftBuffer[(size_t) k] > peakMag)
                    {
                        peakMag = fftBuffer[(size_t) k];
                        peakBin = k;
                    }
                }
                // Only report pitch if it's significantly above noise floor
                if (peakMag * normalizer > 0.01f)
                    currentPitch.store(peakBin * sr / fftSizeF, std::memory_order_relaxed);
                else
                    currentPitch.store(-1.0f, std::memory_order_relaxed);
            }
        }
    }
}

void PhantomProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::RECIPE_PRESET)
    {
        const int idx = juce::roundToInt(newValue);
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
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhantomProcessor();
}
