#pragma once
#include <JuceHeader.h>

enum class GhostMode { Replace, Add };

class CrossoverBlend
{
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;

    void setThresholdHz(float hz) noexcept;
    void setGhost(float g) noexcept         { ghost = juce::jlimit(0.0f, 1.0f, g); }
    void setGhostMode(GhostMode m) noexcept { ghostMode = m; }
    void setSidechainDuckAmount(float a) noexcept { duckAmount = juce::jlimit(0.0f, 1.0f, a); }
    void setDuckAttackMs(float ms) noexcept;
    void setDuckReleaseMs(float ms) noexcept;
    void setStereoWidth(float w) noexcept   { stereoWidth = juce::jlimit(0.0f, 2.0f, w); }
    void setOutputGain(float g) noexcept    { outputGain = g; }

    // dry: main input, modified in-place to become final output
    // phantom: post-binaural post-optimizer harmonic output
    // sidechain: optional (nullptr = no sidechain)
    void process(juce::AudioBuffer<float>& dry,
                 const juce::AudioBuffer<float>& phantom,
                 const juce::AudioBuffer<float>* sidechain) noexcept;

private:
    double sampleRate    = 44100.0;
    float  ghost         = 1.0f;
    GhostMode ghostMode  = GhostMode::Replace;
    float  duckAmount    = 0.0f;
    float  duckAttackMs  = 5.0f;
    float  duckReleaseMs = 80.0f;
    float  stereoWidth   = 1.0f;
    float  outputGain    = 1.0f;

    float currentDuck       = 1.0f;
    float duckAttackCoeff   = 0.0f;
    float duckReleaseCoeff  = 0.0f;

    // 1st-order IIR LP and HP at threshold (one per channel)
    float lpState[2] = {};  // LP filter state per channel
    float lpCoeff    = 0.0f;
    float lastThresholdHz = -1.0f;

    void rebuildCrossover(float thresholdHz) noexcept;
    void updateDuckCoeffs() noexcept;
    void applyStereoWidth(juce::AudioBuffer<float>& buffer, int numSamples) noexcept;
};
