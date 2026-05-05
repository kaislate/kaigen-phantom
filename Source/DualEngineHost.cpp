// Source/DualEngineHost.cpp
#include "DualEngineHost.h"
#include "Parameters.h"

namespace kaigen::phantom
{

DualEngineHost::DualEngineHost(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
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
    auto valueFor = [this, prefix](const char* leaf) -> float {
        const auto id = juce::String(prefix) + leaf;
        return apvts.getRawParameterValue(id)->load();
    };

    target.setCrossoverHz    (valueFor("phantom_threshold"));
    target.setPhantomStrength(valueFor("phantom_strength") / 100.0f);
    target.setSaturation     (valueFor("harmonic_saturation") / 100.0f);
    target.setSynthStep      (valueFor("synth_step") / 100.0f);
    target.setSynthDuty      (valueFor("synth_duty") / 100.0f);
    target.setSynthSkip      ((int) valueFor("synth_skip"));
    target.setGhostAmount    (valueFor("ghost") / 100.0f);
    target.setGhostMode      ((int) valueFor("ghost_mode"));
    target.setOutputGainDb   (valueFor("output_gain"));
    target.setEnvelopeAttackMs (valueFor("env_attack_ms"));
    target.setEnvelopeReleaseMs(valueFor("env_release_ms"));
    target.setEnvSource      ((int) valueFor("env_source"));
    target.setBinauralMode   ((int) valueFor("binaural_mode"));
    target.setBinauralWidth  (valueFor("binaural_width") / 100.0f);
    target.setStereoWidth    (valueFor("stereo_width") / 100.0f);
    target.setSynthLPF       (valueFor("synth_lpf_hz"));
    target.setSynthHPF       (valueFor("synth_hpf_hz"));
    {
        const int idx = (int) valueFor("synth_filter_slope");
        const int dBPerOct = (idx == 0) ? 6 : (idx == 2) ? 24 : 12;
        target.setSynthFilterSlope(dBPerOct);
    }

    static const char* hLeaves[7] = {
        "recipe_h2","recipe_h3","recipe_h4","recipe_h5","recipe_h6","recipe_h7","recipe_h8"
    };
    std::array<float, 7> amps;
    for (int i = 0; i < 7; ++i) amps[(size_t) i] = valueFor(hLeaves[i]) / 100.0f;
    target.setHarmonicAmplitudes(amps);

    target.setSynthMode      ((int) valueFor("mode"));
    target.setWaveletLength  (valueFor("synth_wavelet_length") / 100.0f);
    target.setGateThreshold  (valueFor("synth_gate_threshold") / 100.0f);
    target.setH1Amplitude    (valueFor("synth_h1") / 100.0f);
    target.setSubAmplitude   (valueFor("synth_sub") / 100.0f);
    target.setMinPeriodSamples(valueFor("synth_min_samples"));
    target.setMaxPeriodSamples(valueFor("synth_max_samples"));
    target.setTrackingSpeed  (valueFor("tracking_speed") / 100.0f);
    target.setUsePunch       (valueFor("punch_enabled") > 0.5f);
    target.setPunchAmount    (valueFor("punch_amount") / 100.0f);
    target.setBoostThreshold (valueFor("synth_boost_threshold") / 100.0f);
    target.setBoostAmount    (valueFor("synth_boost_amount") / 100.0f);
    target.setMidiTriggerEnabled(valueFor("midi_trigger_enabled") > 0.5f);
    target.setMidiGateRelease(valueFor("midi_gate_release")    > 0.5f);
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
