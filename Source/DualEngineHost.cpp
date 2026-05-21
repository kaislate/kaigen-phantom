// Source/DualEngineHost.cpp
#include "DualEngineHost.h"
#include "Parameters.h"

namespace kaigen::phantom
{

DualEngineHost::DualEngineHost(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    // Pre-resolve every per-engine parameter ID + atomic pointer ONCE.
    // The audio-thread sync loop below uses these cached entries, doing
    // zero string allocations and zero APVTS hash lookups per block.
    buildParamCache(cacheA, "a_");
    buildParamCache(cacheB, "b_");
}

void DualEngineHost::buildParamCache(std::vector<CachedLeaf>& cache, const char* prefix)
{
    cache.clear();
    cache.reserve(40);

    auto add = [&](const char* leaf) {
        CachedLeaf c;
        c.leafKey = leaf;
        c.fullId  = juce::String(prefix) + leaf;
        c.baseAtom = apvts.getRawParameterValue(c.fullId);
        c.param    = apvts.getParameter(c.fullId);
        jassert(c.baseAtom != nullptr);
        cache.push_back(std::move(c));
    };

    // All per-engine leaf params syncEngineFromPrefix touches.
    add(ParamID::LEAF_PHANTOM_THRESHOLD);
    add(ParamID::LEAF_PHANTOM_STRENGTH);
    add(ParamID::LEAF_HARMONIC_SATURATION);
    add(ParamID::LEAF_SYNTH_STEP);
    add(ParamID::LEAF_SYNTH_DUTY);
    add(ParamID::LEAF_SYNTH_SKIP);
    add(ParamID::LEAF_GHOST);
    add(ParamID::LEAF_GHOST_MODE);
    add(ParamID::LEAF_OUTPUT_GAIN);
    add(ParamID::LEAF_ENV_ATTACK_MS);
    add(ParamID::LEAF_ENV_RELEASE_MS);
    add(ParamID::LEAF_ENV_SOURCE);
    add(ParamID::LEAF_BINAURAL_MODE);
    add(ParamID::LEAF_BINAURAL_WIDTH);
    add(ParamID::LEAF_STEREO_WIDTH);
    add(ParamID::LEAF_SYNTH_LPF_HZ);
    add(ParamID::LEAF_SYNTH_HPF_HZ);
    add(ParamID::LEAF_SYNTH_FILTER_SLOPE);
    add(ParamID::LEAF_RECIPE_H2);
    add(ParamID::LEAF_RECIPE_H3);
    add(ParamID::LEAF_RECIPE_H4);
    add(ParamID::LEAF_RECIPE_H5);
    add(ParamID::LEAF_RECIPE_H6);
    add(ParamID::LEAF_RECIPE_H7);
    add(ParamID::LEAF_RECIPE_H8);
    add(ParamID::LEAF_MODE);
    add(ParamID::LEAF_SYNTH_WAVELET_LENGTH);
    add(ParamID::LEAF_SYNTH_GATE_THRESHOLD);
    add(ParamID::LEAF_SYNTH_H1);
    add(ParamID::LEAF_SYNTH_SUB);
    add(ParamID::LEAF_SYNTH_TRIM);
    add(ParamID::LEAF_SYNTH_MIN_SAMPLES);
    add(ParamID::LEAF_SYNTH_MAX_SAMPLES);
    add(ParamID::LEAF_TRACKING_SPEED);
    add(ParamID::LEAF_PUNCH_ENABLED);
    add(ParamID::LEAF_PUNCH_AMOUNT);
    add(ParamID::LEAF_SYNTH_BOOST_THRESHOLD);
    add(ParamID::LEAF_SYNTH_BOOST_AMOUNT);
    add(ParamID::LEAF_MIDI_TRIGGER_ENABLED);
    add(ParamID::LEAF_MIDI_GATE_RELEASE);
}

const DualEngineHost::CachedLeaf& DualEngineHost::findCached(
    const std::vector<CachedLeaf>& cache, const char* leaf) const noexcept
{
    // Linear scan — ~38 entries, all-pointer-compare, ~40 ns total.
    // (Linker-deduped string literals: ParamID::LEAF_FOO == ParamID::LEAF_FOO
    // by pointer, so this is a strict-pointer-equality lookup.)
    for (const auto& c : cache)
        if (c.leafKey == leaf) return c;
    static const CachedLeaf empty;
    jassertfalse;   // unknown leaf — buildParamCache missing an entry
    return empty;
}

void DualEngineHost::setModulationEngines(kaigen::phantom::ModulationEngine* a,
                                          kaigen::phantom::ModulationEngine* b) noexcept
{
    modA.store(a, std::memory_order_release);
    modB.store(b, std::memory_order_release);
}

bool DualEngineHost::validateParamCachesCoverAPVTS(juce::StringArray& missing) const
{
    missing.clear();

    // Per-engine APVTS params that the audio thread does NOT read via the
    // cache — these are intentionally absent from buildParamCache. If you add
    // a new UI-only per-engine param, list its leaf here so the validator
    // doesn't flag it. Anything ELSE missing is a real bug.
    static const std::array<const char*, 1> kAudioThreadExclusions = {
        ParamID::LEAF_RECIPE_PRESET,    // editor applies preset by writing H2..H8
    };

    auto isExcluded = [](const juce::String& id) {
        for (auto* leaf : kAudioThreadExclusions)
            if (id == juce::String("a_") + leaf || id == juce::String("b_") + leaf)
                return true;
        return false;
    };

    auto inCache = [](const std::vector<CachedLeaf>& cache, const juce::String& fullId) {
        for (const auto& c : cache)
            if (c.fullId == fullId && c.baseAtom != nullptr) return true;
        return false;
    };

    for (const auto& id : getAllParameterIDs())
    {
        if (isExcluded(id)) continue;

        if (id.startsWith("a_"))
        {
            if (! inCache(cacheA, id)) missing.add(id);
        }
        else if (id.startsWith("b_"))
        {
            if (! inCache(cacheB, id)) missing.add(id);
        }
    }

    return missing.isEmpty();
}

void DualEngineHost::prepareToPlay(double sampleRate, int blockSize, int numChannels)
{
    engineA.prepare(sampleRate, blockSize, numChannels);
    engineB.prepare(sampleRate, blockSize, numChannels);
    crossfader.prepare(sampleRate, blockSize);
    bScratch.setSize(numChannels, blockSize, false, true, true);
    aScratch.setSize(numChannels, blockSize, false, true, true);
    phantomOnlyMix.setSize(numChannels, blockSize, false, true, true);
    phantomOnlyMix.clear();
}

void DualEngineHost::reset()
{
    engineA.reset();
    engineB.reset();
    phantomOnlyMix.clear();
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

void DualEngineHost::syncEngineFromPrefix(PhantomEngine& target,
                                            const std::vector<CachedLeaf>& cache,
                                            kaigen::phantom::ModulationEngine* modEng)
{
    // Hot path. Zero string allocations, zero APVTS hash lookups per call.
    // Each leaf lookup is a pointer-equality scan over ~38 entries (~40ns)
    // plus a pre-resolved atomic<float> load + an early-out modulation check.
    auto valueFor = [&cache, modEng, this](const char* leaf) -> float {
        const auto& c = findCached(cache, leaf);
        const float base = c.baseAtom->load(std::memory_order_relaxed);
        return modEng ? modEng->getModulatedValue(c.fullId, c.param, base) : base;
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
    target.setSynthTrim      (valueFor(ParamID::LEAF_SYNTH_TRIM) / 100.0f);
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

    // Sync params from APVTS into each engine. Cache + modEng are passed
    // explicitly so the hot path does no string compare to pick which side.
    syncEngineFromPrefix(engineA, cacheA, modA.load(std::memory_order_acquire));
    syncEngineFromPrefix(engineB, cacheB, modB.load(std::memory_order_acquire));

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

    // Same crossfade applied to the per-engine phantom-only side
    // buffers. Re-uses the crossfader state set just above, so the
    // weights match the main mix exactly — the reverb-source selector
    // hears the synth at the same relative gain as the main output.
    phantomOnlyMix.setSize(nCh, n, false, false, true);
    crossfader.mix(engineA.getPhantomOnlyOutput(), bypassA,
                   engineB.getPhantomOnlyOutput(), bypassB,
                   phantomOnlyMix);
}

} // namespace kaigen::phantom
