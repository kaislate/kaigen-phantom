#if KAIGEN_PRO_BUILD

#include "MorphEngine.h"
#include "Parameters.h"
#include "ABSlotManager.h"
#include "Engines/PhantomEngine.h"

namespace kaigen::phantom
{

MorphEngine::MorphEngine(juce::AudioProcessorValueTreeState& apvtsRef,
                         ABSlotManager& abSlotsRef,
                         PhantomEngine& primaryEngineRef)
    : apvts(apvtsRef), abSlots(abSlotsRef), primaryEngine(primaryEngineRef)
{
    apvts.addParameterListener(ParamID::MORPH_ENABLED,  this);
    apvts.addParameterListener(ParamID::MORPH_AMOUNT,   this);
    apvts.addParameterListener(ParamID::SCENE_ENABLED,  this);
    apvts.addParameterListener(ParamID::SCENE_POSITION, this);
}

MorphEngine::~MorphEngine()
{
    apvts.removeParameterListener(ParamID::MORPH_ENABLED,  this);
    apvts.removeParameterListener(ParamID::MORPH_AMOUNT,   this);
    apvts.removeParameterListener(ParamID::SCENE_ENABLED,  this);
    apvts.removeParameterListener(ParamID::SCENE_POSITION, this);
}

void MorphEngine::prepareToPlay(double sr, int spb)
{
    sampleRate = sr;
    samplesPerBlock = spb;

    // Single-pole IIR: per-sample alpha → per-block alpha
    // tau = 15 ms -> alpha_perSample = 1 - exp(-1 / (tau * sr))
    // per-block alpha = 1 - (1 - alpha_perSample)^spb
    constexpr float tauMs = 15.0f;
    const float alphaPerSample = 1.0f - std::exp(-1.0f / ((tauMs / 1000.0f) * (float) sr));
    smoothingAlpha = 1.0f - std::pow(1.0f - alphaPerSample, (float) spb);
}

void MorphEngine::preProcessBlock()
{
    // Smoothing step.
    smoothedMorph    += smoothingAlpha * (rawMorph    - smoothedMorph);
    smoothedScenePos += smoothingAlpha * (rawScenePos - smoothedScenePos);

    // Only apply arcs when morph is enabled and has at least one armed arc.
    if (!enabled || lane1Arcs.empty()) return;

    for (const auto& [paramID, entry] : lane1Arcs)
    {
        auto* p = apvts.getParameter(paramID);
        if (p == nullptr) continue;

        const auto& range = p->getNormalisableRange();
        const float paramRange = range.end - range.start;

        // value = base + depth * morph * paramRange   (then clamped by writeParamClamped)
        const float target = entry.capturedBase + entry.depth * smoothedMorph * paramRange;

        writeParamClamped(paramID, target);
    }
}

void MorphEngine::syncSecondaryEngineFromSlotB()
{
    // This is called inline from postProcessBlock; the actual work happens there
    // because we need the scope-held savedState. Kept as a named method for
    // future refactoring when the primary engine is decoupled from APVTS.
}

void MorphEngine::postProcessBlock(juce::AudioBuffer<float>& mainBuffer,
                                   const juce::AudioBuffer<float>* sidechain)
{
    if (!sceneEnabled || secondaryEngine == nullptr) return;

    const int n = mainBuffer.getNumSamples();
    const int nCh = juce::jmin(2, mainBuffer.getNumChannels());

    // Swap APVTS state to slot B, sync secondary engine, swap back.
    const auto savedState = apvts.copyState();
    const auto slotB = abSlots.getSlot(ABSlotManager::Slot::B);

    if (slotB.isValid())
    {
        // Replace state with slot B (secondary engine's intended config).
        {
            const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
            apvts.replaceState(slotB.createCopy());
        }

        juce::AudioBuffer<float> secondaryBuf(nCh, n);
        for (int ch = 0; ch < nCh; ++ch)
            secondaryBuf.copyFrom(ch, 0, mainBuffer, ch, 0, n);

        secondaryEngine->process(secondaryBuf, sidechain);

        // Restore primary state.
        {
            const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
            apvts.replaceState(savedState);
        }

        // Mix: primary output is already in mainBuffer; blend secondary in.
        const float pos = juce::jlimit(0.0f, 1.0f, smoothedScenePos);
        const float primaryGain = 1.0f - pos;
        const float secondaryGain = pos;

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* main = mainBuffer.getWritePointer(ch);
            const auto* sec = secondaryBuf.getReadPointer(ch);
            for (int i = 0; i < n; ++i)
                main[i] = main[i] * primaryGain + sec[i] * secondaryGain;
        }
    }
}

void MorphEngine::parameterChanged(const juce::String& paramID, float newValue)
{
    if (suppressArcUpdates) return;

    if      (paramID == ParamID::MORPH_ENABLED)   enabled        = (newValue > 0.5f);
    else if (paramID == ParamID::MORPH_AMOUNT)    rawMorph       = juce::jlimit(0.0f, 1.0f, newValue);
    else if (paramID == ParamID::SCENE_ENABLED)   sceneEnabled   = (newValue > 0.5f);
    else if (paramID == ParamID::SCENE_POSITION)  rawScenePos    = juce::jlimit(0.0f, 1.0f, newValue);
}

void MorphEngine::setArcDepth(const juce::String& paramID, float normalizedDepth)
{
    // Clamp to bipolar range.
    normalizedDepth = juce::jlimit(-1.0f, 1.0f, normalizedDepth);

    if (std::abs(normalizedDepth) < 1e-6f)
    {
        lane1Arcs.erase(paramID);
        return;
    }

    // Capture current base value for this parameter.
    float base = 0.0f;
    if (auto* p = apvts.getParameter(paramID))
        base = p->convertFrom0to1(p->getValue());

    lane1Arcs[paramID] = { normalizedDepth, base };
}

float MorphEngine::getArcDepth(const juce::String& paramID) const
{
    auto it = lane1Arcs.find(paramID);
    return (it != lane1Arcs.end()) ? it->second.depth : 0.0f;
}

bool MorphEngine::hasNonZeroArc(const juce::String& paramID) const
{
    return lane1Arcs.find(paramID) != lane1Arcs.end();
}

int MorphEngine::armedKnobCount() const
{
    return (int) lane1Arcs.size();
}

std::vector<juce::String> MorphEngine::getArmedParamIDs() const
{
    std::vector<juce::String> result;
    result.reserve(lane1Arcs.size());
    for (const auto& [id, entry] : lane1Arcs)
        result.push_back(id);
    return result;
}

void MorphEngine::setEnabled(bool on)
{
    enabled = on;
    // Also sync the APVTS parameter so the UI slider and DAW reflect state.
    if (auto* p = apvts.getParameter(ParamID::MORPH_ENABLED))
    {
        auto abGuard = abSlots.scopedSuppressModified();
        const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
        p->beginChangeGesture();
        p->setValueNotifyingHost(on ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

void MorphEngine::beginCapture()
{
    inCapture = true;
    captureBaseline.clear();

    // Snapshot baseline values for every continuous parameter.
    for (const auto& id : getContinuousParamIDs(apvts))
    {
        if (auto* p = apvts.getParameter(id))
            captureBaseline[id] = p->convertFrom0to1(p->getValue());
    }
}

std::vector<juce::String> MorphEngine::endCapture(bool commit)
{
    std::vector<juce::String> modified;
    if (!inCapture) return modified;

    if (commit)
    {
        // For each parameter: delta = current - baseline → arc = delta / paramRange.
        for (const auto& [id, baseline] : captureBaseline)
        {
            auto* p = apvts.getParameter(id);
            if (p == nullptr) continue;

            const float current = p->convertFrom0to1(p->getValue());
            const auto& range = p->getNormalisableRange();
            const float paramRange = range.end - range.start;

            if (paramRange > 0.0f)
            {
                const float delta = current - baseline;
                const float depth = juce::jlimit(-1.0f, 1.0f, delta / paramRange);

                if (std::abs(depth) >= 1e-4f)
                {
                    // Overwrite the arc with new depth + baseline (NOT current).
                    lane1Arcs[id] = { depth, baseline };
                    modified.push_back(id);
                }
            }
        }
    }
    else
    {
        // Restore baselines (cancel).
        auto abGuard = abSlots.scopedSuppressModified();
        const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
        for (const auto& [id, baseline] : captureBaseline)
        {
            if (auto* p = apvts.getParameter(id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->convertTo0to1(baseline));
                p->endChangeGesture();
            }
        }
    }

    inCapture = false;
    captureBaseline.clear();
    return modified;
}

void MorphEngine::setSceneCrossfadeEnabled(bool on)
{
    sceneEnabled = on;
    if (auto* p = apvts.getParameter(ParamID::SCENE_ENABLED))
    {
        auto abGuard = abSlots.scopedSuppressModified();
        const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
        p->beginChangeGesture();
        p->setValueNotifyingHost(on ? 1.0f : 0.0f);
        p->endChangeGesture();
    }

    if (on && secondaryEngine == nullptr)
    {
        secondaryEngine = std::make_unique<PhantomEngine>();
        secondaryEngine->prepare(sampleRate, samplesPerBlock, 2);
    }
    // Keep secondaryEngine around once created; no destruction on disable
    // (avoids churn; memory cost is acceptable).
}

juce::ValueTree MorphEngine::toMorphConfigTree() const
{
    juce::ValueTree root { "MorphConfig" };
    root.setProperty("defaultPosition", rawMorph, nullptr);
    root.setProperty("curve", curveName, nullptr);

    juce::ValueTree lane { "ArcLane" };
    lane.setProperty("id", 1, nullptr);
    for (const auto& [id, entry] : lane1Arcs)
    {
        juce::ValueTree arc { "Arc" };
        arc.setProperty("paramID", id, nullptr);
        arc.setProperty("depth", entry.depth, nullptr);
        lane.appendChild(arc, nullptr);
    }
    root.appendChild(lane, nullptr);

    if (sceneEnabled || rawScenePos != 0.0f)
    {
        juce::ValueTree scene { "SceneCrossfade" };
        scene.setProperty("enabled", sceneEnabled ? 1 : 0, nullptr);
        scene.setProperty("position", rawScenePos, nullptr);
        root.appendChild(scene, nullptr);
    }

    return root;
}

void MorphEngine::fromMorphConfigTree(const juce::ValueTree& morphConfig)
{
    if (!morphConfig.isValid() || morphConfig.getType().toString() != "MorphConfig")
        return;

    // Reset current state before applying new.
    lane1Arcs.clear();

    // Apply top-level morph defaults.
    rawMorph = juce::jlimit(0.0f, 1.0f,
        (float) morphConfig.getProperty("defaultPosition", juce::var(0.0f)));
    smoothedMorph = rawMorph;
    curveName = morphConfig.getProperty("curve", juce::var("linear")).toString();

    // Read arcs.
    const auto lane = morphConfig.getChildWithName("ArcLane");
    if (lane.isValid())
    {
        for (int i = 0; i < lane.getNumChildren(); ++i)
        {
            const auto arc = lane.getChild(i);
            if (arc.getType().toString() != "Arc") continue;

            const auto id = arc.getProperty("paramID", juce::var("")).toString();
            const float depth = (float) arc.getProperty("depth", juce::var(0.0f));

            if (id.isNotEmpty() && std::abs(depth) >= 1e-6f)
            {
                // Capture current live value as the base (the base is recomputed
                // on load; the saved arc depth is relative delta).
                float base = 0.0f;
                if (auto* p = apvts.getParameter(id))
                    base = p->convertFrom0to1(p->getValue());
                lane1Arcs[id] = { depth, base };
            }
        }
    }

    // Read scene crossfade state.
    const auto scene = morphConfig.getChildWithName("SceneCrossfade");
    if (scene.isValid())
    {
        sceneEnabled = ((int) scene.getProperty("enabled", 0)) != 0;
        rawScenePos = juce::jlimit(0.0f, 1.0f,
            (float) scene.getProperty("position", juce::var(0.0f)));
        smoothedScenePos = rawScenePos;

        // Sync APVTS parameters.
        if (auto* p = apvts.getParameter(ParamID::SCENE_ENABLED))
        {
            auto abGuard = abSlots.scopedSuppressModified();
            const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
            p->setValueNotifyingHost(sceneEnabled ? 1.0f : 0.0f);
        }
        if (auto* p = apvts.getParameter(ParamID::SCENE_POSITION))
        {
            auto abGuard = abSlots.scopedSuppressModified();
            const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
            p->setValueNotifyingHost(rawScenePos);
        }
    }

    // Enable morph automatically if any arcs are loaded.
    if (!lane1Arcs.empty())
        setEnabled(true);
}

juce::ValueTree MorphEngine::toStateTree() const
{
    // <MorphState> wraps <MorphConfig> plus live runtime state.
    juce::ValueTree root { "MorphState" };
    root.setProperty("enabled",       enabled ? 1 : 0, nullptr);
    root.setProperty("morphAmount",   smoothedMorph, nullptr);
    root.setProperty("sceneEnabled",  sceneEnabled ? 1 : 0, nullptr);
    root.setProperty("scenePosition", smoothedScenePos, nullptr);

    // Arc lane inline (same structure as preset's MorphConfig).
    juce::ValueTree lane { "ArcLane" };
    lane.setProperty("id", 1, nullptr);
    for (const auto& [id, entry] : lane1Arcs)
    {
        juce::ValueTree arc { "Arc" };
        arc.setProperty("paramID", id, nullptr);
        arc.setProperty("depth", entry.depth, nullptr);
        arc.setProperty("capturedBase", entry.capturedBase, nullptr);
        lane.appendChild(arc, nullptr);
    }
    root.appendChild(lane, nullptr);
    return root;
}

void MorphEngine::fromStateTree(const juce::ValueTree& morphStateNode)
{
    if (!morphStateNode.isValid() || morphStateNode.getType().toString() != "MorphState")
    {
        // Reset to defaults.
        enabled = false;
        rawMorph = smoothedMorph = 0.0f;
        sceneEnabled = false;
        rawScenePos = smoothedScenePos = 0.0f;
        lane1Arcs.clear();
        return;
    }

    enabled = ((int) morphStateNode.getProperty("enabled", 0)) != 0;
    rawMorph = smoothedMorph = juce::jlimit(0.0f, 1.0f,
        (float) morphStateNode.getProperty("morphAmount", juce::var(0.0f)));
    sceneEnabled = ((int) morphStateNode.getProperty("sceneEnabled", 0)) != 0;
    rawScenePos = smoothedScenePos = juce::jlimit(0.0f, 1.0f,
        (float) morphStateNode.getProperty("scenePosition", juce::var(0.0f)));

    lane1Arcs.clear();
    const auto lane = morphStateNode.getChildWithName("ArcLane");
    if (lane.isValid())
    {
        for (int i = 0; i < lane.getNumChildren(); ++i)
        {
            const auto arc = lane.getChild(i);
            if (arc.getType().toString() != "Arc") continue;

            const auto id = arc.getProperty("paramID", juce::var("")).toString();
            const float depth = (float) arc.getProperty("depth", juce::var(0.0f));
            const float base  = (float) arc.getProperty("capturedBase", juce::var(0.0f));

            if (id.isNotEmpty() && std::abs(depth) >= 1e-6f)
                lane1Arcs[id] = { depth, base };
        }
    }

    // Sync APVTS to reflect the restored state.
    auto abGuard = abSlots.scopedSuppressModified();
    const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
    if (auto* p = apvts.getParameter(ParamID::MORPH_ENABLED))  p->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
    if (auto* p = apvts.getParameter(ParamID::MORPH_AMOUNT))   p->setValueNotifyingHost(rawMorph);
    if (auto* p = apvts.getParameter(ParamID::SCENE_ENABLED))  p->setValueNotifyingHost(sceneEnabled ? 1.0f : 0.0f);
    if (auto* p = apvts.getParameter(ParamID::SCENE_POSITION)) p->setValueNotifyingHost(rawScenePos);
}

std::vector<juce::String> MorphEngine::getContinuousParamIDs(juce::AudioProcessorValueTreeState& apvts)
{
    std::vector<juce::String> result;
    for (const auto& id : getAllParameterIDs())
    {
        if (auto* p = apvts.getParameter(id))
        {
            // Continuous = range with non-integer interval, or a non-bool/non-choice type.
            if (dynamic_cast<juce::AudioParameterBool*>(p)   != nullptr) continue;
            if (dynamic_cast<juce::AudioParameterChoice*>(p) != nullptr) continue;
            // Skip Pro morph params themselves.
            if (id == ParamID::MORPH_ENABLED || id == ParamID::MORPH_AMOUNT
             || id == ParamID::SCENE_ENABLED || id == ParamID::SCENE_POSITION)
                continue;
            result.push_back(id);
        }
    }
    return result;
}

void MorphEngine::updateSmoothing() {}
float MorphEngine::smoothOne(float&, float) const { return 0.0f; }

void MorphEngine::writeParamClamped(const juce::String& paramID, float denormalizedValue)
{
    auto* p = apvts.getParameter(paramID);
    if (p == nullptr) return;

    const auto& range = p->getNormalisableRange();
    const float clamped = juce::jlimit(range.start, range.end, denormalizedValue);

    // Double guard: MorphEngine's own listener AND ABSlotManager's modified-flag
    // listener must both be suppressed during this internal write, otherwise
    // slot A gets spuriously marked modified every block.
    auto abGuard = abSlots.scopedSuppressModified();
    const juce::ScopedValueSetter<bool> morphGuard { suppressArcUpdates, true };

    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(clamped));
    p->endChangeGesture();
}

} // namespace kaigen::phantom

#endif // KAIGEN_PRO_BUILD
