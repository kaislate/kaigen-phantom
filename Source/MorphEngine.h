#pragma once
#if KAIGEN_PRO_BUILD

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
#include <unordered_map>
#include <memory>
#include <functional>

#include "Engines/PhantomEngine.h"

namespace kaigen::phantom
{

class ABSlotManager;

class MorphEngine : private juce::AudioProcessorValueTreeState::Listener
{
public:
    MorphEngine(juce::AudioProcessorValueTreeState& apvts,
                ABSlotManager& abSlots,
                PhantomEngine& primaryEngine,
                std::function<void(PhantomEngine&)> engineSyncFromAPVTS);
    ~MorphEngine() override;

    // Called from PhantomProcessor::prepareToPlay before audio starts.
    void prepareToPlay(double sampleRate, int samplesPerBlock);

    // Per-block hooks
    void preProcessBlock();
    void postProcessBlock(juce::AudioBuffer<float>& mainBuffer,
                          const juce::AudioBuffer<float>* sidechain);

    // Arc access (single lane in v1)
    void setArcDepth(const juce::String& paramID, float normalizedDepth);
    float getArcDepth(const juce::String& paramID) const;
    bool hasNonZeroArc(const juce::String& paramID) const;
    int armedKnobCount() const;
    std::vector<juce::String> getArmedParamIDs() const;

    // Morph amount — smoothed value, read-only from outside
    float getMorphAmount() const noexcept { return smoothedMorph; }

    // Morph enable toggle
    bool isEnabled() const noexcept { return enabled; }
    void setEnabled(bool on);

    // Capture mode
    void beginCapture();
    std::vector<juce::String> endCapture(bool commit);
    bool isInCapture() const noexcept { return inCapture; }

    // Scene Crossfade
    bool isSceneCrossfadeEnabled() const noexcept { return sceneEnabled; }
    void setSceneCrossfadeEnabled(bool on);
    float getScenePosition() const noexcept { return smoothedScenePos; }

    // Preset I/O
    juce::ValueTree toMorphConfigTree() const;
    void fromMorphConfigTree(const juce::ValueTree& morphConfigNode);

    // Plugin state I/O (for getStateInformation / setStateInformation)
    juce::ValueTree toStateTree() const;
    void fromStateTree(const juce::ValueTree& morphStateNode);

    // Returns the list of APVTS param IDs that can be arc-armed (all continuous).
    static std::vector<juce::String> getContinuousParamIDs(juce::AudioProcessorValueTreeState& apvts);

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void syncSecondaryEngineFromSlotB();
    void updateSmoothing();
    float smoothOne(float& target, float raw) const;
    void writeParamClamped(const juce::String& paramID, float denormalizedValue);

    struct ArcEntry
    {
        float depth = 0.0f;       // bipolar normalized [-1, +1]
        float capturedBase = 0.0f; // denormalized base value captured when arc was set
    };

    juce::AudioProcessorValueTreeState& apvts;
    ABSlotManager& abSlots;
    PhantomEngine& primaryEngine;
    std::function<void(PhantomEngine&)> engineSyncFromAPVTS;

    std::unordered_map<juce::String, ArcEntry> lane1Arcs;
    juce::String curveName = "linear";

    bool  enabled = false;
    float rawMorph = 0.0f;
    float smoothedMorph = 0.0f;

    // Capture mode
    bool  inCapture = false;
    std::unordered_map<juce::String, float> captureBaseline;

    // Scene Crossfade
    bool  sceneEnabled = false;
    float rawScenePos = 0.0f;
    float smoothedScenePos = 0.0f;
    std::unique_ptr<PhantomEngine> secondaryEngine;

    // Pre-allocated scratch buffers — resized once in prepareToPlay (C1 fix).
    juce::AudioBuffer<float> secondaryScratchBuf;
    juce::ValueTree savedPrimaryState;

    // Smoothing
    double sampleRate = 44100.0;
    int    samplesPerBlock = 512;
    float  smoothingAlpha = 0.0f;

    // Guard against our own listener firing during internal APVTS writes.
    bool suppressArcUpdates = false;
};

} // namespace kaigen::phantom

#endif // KAIGEN_PRO_BUILD
