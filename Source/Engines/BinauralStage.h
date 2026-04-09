#pragma once
#include <JuceHeader.h>

enum class BinauralMode { Off, Spread, VoiceSplit };

class BinauralStage
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setMode(BinauralMode m) noexcept  { mode  = m; }
    void setWidth(float w) noexcept        { width = juce::jlimit(0.0f, 1.0f, w); }

    void setVoicePan(int voiceIndex, float pan) noexcept;

    void process(juce::AudioBuffer<float>& buffer);

    bool isUsingBinaural() const noexcept { return mode != BinauralMode::Off; }

private:
    BinauralMode mode  = BinauralMode::Off;
    float        width = 0.5f;
    double       sampleRate = 44100.0;

    float voicePans[8] = {};

    void applySpread(juce::AudioBuffer<float>& buffer);
};
