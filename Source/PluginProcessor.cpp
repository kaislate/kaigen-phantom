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

    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();
    if (numCh < 2 || n == 0) return;

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
            }
            else if (m.isNoteOff() || (m.isNoteOn() && m.getVelocity() == 0))
                harmonicGen.noteOff(m.getNoteNumber());
        }
    }

    if (!isInstrumentMode)
    {
        float detectedHz = pitchTracker.detectPitch(buffer.getReadPointer(0), n);
        if (detectedHz > 0.0f)
        {
            harmonicGen.setEffectModePitch(detectedHz);
            perceptualOpt.setFundamental(detectedHz);
        }
    }

    phantomBuf.clear();
    harmonicGen.process(phantomBuf);

    binauralStage.process(phantomBuf);
    perceptualOpt.process(phantomBuf);

    const juce::AudioBuffer<float>* sidechainBuf = nullptr;
    if (getBusCount(true) > 1)
    {
        auto* scBus = getBus(true, 1);
        if (scBus != nullptr && scBus->isEnabled())
            sidechainBuf = &getBusBuffer(buffer, true, 1);
    }

    // Copy dry to pre-allocated member buffer (crossoverBlend modifies in-place)
    for (int ch = 0; ch < numCh; ++ch)
        dryBuf.copyFrom(ch, 0, buffer, ch, 0, n);

    crossoverBlend.process(dryBuf, phantomBuf, sidechainBuf);

    for (int ch = 0; ch < numCh; ++ch)
        buffer.copyFrom(ch, 0, dryBuf, ch, 0, n);
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
