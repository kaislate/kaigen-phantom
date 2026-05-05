// Source/DualEngineHost.cpp
#include "DualEngineHost.h"
#include "Parameters.h"

namespace kaigen::phantom
{

DualEngineHost::DualEngineHost(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
}

void DualEngineHost::setModulationEngines(kaigen::phantom::ModulationEngine* a,
                                          kaigen::phantom::ModulationEngine* b) noexcept
{
    modA = a;
    modB = b;
}

void DualEngineHost::prepareToPlay(double sampleRate, int blockSize, int numChannels)
{
    engineA.prepare(sampleRate, blockSize, numChannels);
    engineB.prepare(sampleRate, blockSize, numChannels);
    crossfader.prepare(sampleRate, blockSize);
    bScratch.setSize(numChannels, blockSize, false, true, true);
    aScratch.setSize(numChannels, blockSize, false, true, true);
}

void DualEngineHost::reset()
{
    engineA.reset();
    engineB.reset();
}

void DualEngineHost::handleMidiNoteOn()
{
    engineA.handleMidiNoteOn();
    engineB.handleMidiNoteOn();
}

void DualEngineHost::handleMidiNoteOff()
{
    engineA.handleMidiNoteOff();
    engineB.handleMidiNoteOff();
}

void DualEngineHost::setInputDetectionGain(float gainLin)
{
    engineA.setInputDetectionGain(gainLin);
    engineB.setInputDetectionGain(gainLin);
}

void DualEngineHost::syncEngineFromPrefix(PhantomEngine& target, const char* prefix)
{
    // Capture by-value: prefix is a string literal at all call sites
    // (see process()), so its lifetime is fine for the duration of this lambda.
    auto* modEng = (juce::String(prefix) == "a_") ? modA
                 : (juce::String(prefix) == "b_") ? modB
                 : nullptr;

    auto valueFor = [this, prefix, modEng](const char* leaf) -> float {
        const auto id = juce::String(prefix) + leaf;
        const float base = apvts.getRawParameterValue(id)->load();
        return modEng ? modEng->getModulatedValue(id, base) : base;
    };

    target.setCrossoverHz    (valueFor(ParamID::LEAF_PHANTOM_THRESHOLD));
    target.setPhantomStrength(valueFor(ParamID::LEAF_PHANTOM_STRENGTH) / 100.0f);
    target.setSaturation     (valueFor(ParamID::LEAF_HARMONIC_SATURATION) / 100.0f);
    target.setSynthStep      (valueFor(ParamID::LEAF_SYNTH_STEP) / 100.0f);
    target.setSynthDuty      (valueFor(ParamID::LEAF_SYNTH_DUTY) / 100.0f);
    target.setSynthSkip      ((int) valueFor(ParamID::LEAF_SYNTH_SKIP));
    target.setGhostAmount    (valueFor(ParamID::LEAF_GHOST) / 100.0f);
    target.setGhostMode      ((int) valueFor(ParamID::LEAF_GHOST_MODE));
    target.setOutputGainDb   (valueFor(ParamID::LEAF_OUTPUT_GAIN));
    target.setEnvelopeAttackMs (valueFor(ParamID::LEAF_ENV_ATTACK_MS));
    target.setEnvelopeReleaseMs(valueFor(ParamID::LEAF_ENV_RELEASE_MS));
    target.setEnvSource      ((int) valueFor(ParamID::LEAF_ENV_SOURCE));
    target.setBinauralMode   ((int) valueFor(ParamID::LEAF_BINAURAL_MODE));
    target.setBinauralWidth  (valueFor(ParamID::LEAF_BINAURAL_WIDTH) / 100.0f);
    target.setStereoWidth    (valueFor(ParamID::LEAF_STEREO_WIDTH) / 100.0f);
    target.setSynthLPF       (valueFor(ParamID::LEAF_SYNTH_LPF_HZ));
    target.setSynthHPF       (valueFor(ParamID::LEAF_SYNTH_HPF_HZ));
    {
        const int idx = (int) valueFor(ParamID::LEAF_SYNTH_FILTER_SLOPE);
        const int dBPerOct = (idx == 0) ? 6 : (idx == 2) ? 24 : 12;
        target.setSynthFilterSlope(dBPerOct);
    }

    static constexpr const char* hLeaves[7] = {
        ParamID::LEAF_RECIPE_H2, ParamID::LEAF_RECIPE_H3, ParamID::LEAF_RECIPE_H4,
        ParamID::LEAF_RECIPE_H5, ParamID::LEAF_RECIPE_H6, ParamID::LEAF_RECIPE_H7,
        ParamID::LEAF_RECIPE_H8
    };
    std::array<float, 7> amps;
    for (int i = 0; i < 7; ++i) amps[(size_t) i] = valueFor(hLeaves[i]) / 100.0f;
    target.setHarmonicAmplitudes(amps);

    target.setSynthMode      ((int) valueFor(ParamID::LEAF_MODE));
    target.setWaveletLength  (valueFor(ParamID::LEAF_SYNTH_WAVELET_LENGTH) / 100.0f);
    target.setGateThreshold  (valueFor(ParamID::LEAF_SYNTH_GATE_THRESHOLD) / 100.0f);
    target.setH1Amplitude    (valueFor(ParamID::LEAF_SYNTH_H1) / 100.0f);
    target.setSubAmplitude   (valueFor(ParamID::LEAF_SYNTH_SUB) / 100.0f);
    target.setMinPeriodSamples(valueFor(ParamID::LEAF_SYNTH_MIN_SAMPLES));
    target.setMaxPeriodSamples(valueFor(ParamID::LEAF_SYNTH_MAX_SAMPLES));
    target.setTrackingSpeed  (valueFor(ParamID::LEAF_TRACKING_SPEED) / 100.0f);
    target.setUsePunch       (valueFor(ParamID::LEAF_PUNCH_ENABLED) > 0.5f);
    target.setPunchAmount    (valueFor(ParamID::LEAF_PUNCH_AMOUNT) / 100.0f);
    target.setBoostThreshold (valueFor(ParamID::LEAF_SYNTH_BOOST_THRESHOLD) / 100.0f);
    target.setBoostAmount    (valueFor(ParamID::LEAF_SYNTH_BOOST_AMOUNT) / 100.0f);
    target.setMidiTriggerEnabled(valueFor(ParamID::LEAF_MIDI_TRIGGER_ENABLED) > 0.5f);
    target.setMidiGateRelease(valueFor(ParamID::LEAF_MIDI_GATE_RELEASE)    > 0.5f);
}

void DualEngineHost::process(juce::AudioBuffer<float>& buffer,
                             const juce::AudioBuffer<float>* sidechain)
{
    const int n   = buffer.getNumSamples();
    const int nCh = juce::jmin(buffer.getNumChannels(), 2);
    if (n == 0 || nCh == 0) return;

    // Read morph + curve + bypass from APVTS.
    const float morphAmt   = apvts.getRawParameterValue(ParamID::MORPH_AMOUNT)->load();
    const auto curveIdx    = (int) apvts.getRawParameterValue(ParamID::MORPH_CURVE)->load();
    const float aDb        = apvts.getRawParameterValue(ParamID::MORPH_A_LEVEL_DB)->load();
    const float bDb        = apvts.getRawParameterValue(ParamID::MORPH_B_LEVEL_DB)->load();
    const bool  bypassIdle = apvts.getRawParameterValue(ParamID::MORPH_BYPASS_IDLE_ENGINE)->load() > 0.5f;

    crossfader.setCurve(static_cast<MorphCrossfader::Curve>(curveIdx));
    crossfader.setLevels(aDb, bDb);
    crossfader.setMorph(morphAmt);

    // Idle bypass — block-boundary check.
    // morph >= 1-eps  → A is silent  (engine A bypassed)
    // morph <= eps    → B is silent  (engine B bypassed)
    const bool bypassA = bypassIdle && morphAmt >= 1.0f - MorphCrossfader::kBypassEpsilon;
    const bool bypassB = bypassIdle && morphAmt <= 0.0f + MorphCrossfader::kBypassEpsilon;

    // Sync params from APVTS into each engine.
    syncEngineFromPrefix(engineA, "a_");
    syncEngineFromPrefix(engineB, "b_");

    // Both engines process the same input. We snapshot the input into
    // aScratch (in-place processing target for engine A) AND bScratch
    // (target for engine B). The caller's `buffer` is then overwritten
    // with the crossfaded result.
    aScratch.setSize(nCh, n, false, false, true);
    bScratch.setSize(nCh, n, false, false, true);
    for (int c = 0; c < nCh; ++c)
    {
        std::memcpy(aScratch.getWritePointer(c), buffer.getReadPointer(c),
                    sizeof(float) * (size_t) n);
        std::memcpy(bScratch.getWritePointer(c), buffer.getReadPointer(c),
                    sizeof(float) * (size_t) n);
    }

    // Process each engine into its own scratch buffer.
    if (!bypassA) engineA.process(aScratch, sidechain);
    if (!bypassB) engineB.process(bScratch, sidechain);

    // Crossfade into `buffer` (in place).
    crossfader.mix(aScratch, bypassA, bScratch, bypassB, buffer);
}

} // namespace kaigen::phantom
