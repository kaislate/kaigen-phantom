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

void MorphEngine::postProcessBlock(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) {}

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
        const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
        p->beginChangeGesture();
        p->setValueNotifyingHost(on ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
    // Note: Task 16 handles lazy secondaryEngine construction on first enable.
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
            const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
            p->setValueNotifyingHost(sceneEnabled ? 1.0f : 0.0f);
        }
        if (auto* p = apvts.getParameter(ParamID::SCENE_POSITION))
        {
            const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
            p->setValueNotifyingHost(rawScenePos);
        }
    }

    // Enable morph automatically if any arcs are loaded.
    if (!lane1Arcs.empty())
        setEnabled(true);
}

juce::ValueTree MorphEngine::toStateTree() const { return juce::ValueTree("MorphState"); }
void MorphEngine::fromStateTree(const juce::ValueTree&) {}

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

    const juce::ScopedValueSetter<bool> guard { suppressArcUpdates, true };
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(clamped));
    p->endChangeGesture();
}

} // namespace kaigen::phantom

#endif // KAIGEN_PRO_BUILD
