#include "BinauralStage.h"
#include <cmath>
#include <algorithm>

void BinauralStage::prepare(double sr, int /*maxBlockSize*/) noexcept
{
    sampleRate = sr;
    std::fill(voicePans, voicePans + 8, 0.0f);
}

void BinauralStage::reset() noexcept
{
    std::fill(voicePans, voicePans + 8, 0.0f);
}

void BinauralStage::setVoicePan(int voiceIndex, float pan) noexcept
{
    if (voiceIndex >= 0 && voiceIndex < 8)
        voicePans[voiceIndex] = juce::jlimit(-1.0f, 1.0f, pan);
}

void BinauralStage::process(juce::AudioBuffer<float>& buffer) noexcept
{
    switch (mode)
    {
        case BinauralMode::Spread:     applySpread(buffer); break;
        case BinauralMode::VoiceSplit: /* deferred to Task 10 wiring */ break;
        case BinauralMode::Off:
        default:                       break;
    }
}

void BinauralStage::applySpread(juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() < 2) return;
    if (width < 1e-5f) return;

    const int numSamples = buffer.getNumSamples();
    const float w = width;

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mid  = (L[i] + R[i]) * 0.5f;
        const float side = (L[i] - R[i]) * 0.5f;

        // M/S widening: boost side signal by width factor
        L[i] = mid + side * (1.0f + w);
        R[i] = mid - side * (1.0f + w);
    }
}
