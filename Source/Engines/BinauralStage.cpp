#include "BinauralStage.h"
#include <cmath>
#include <algorithm>

void BinauralStage::prepare(double sr, int /*maxBlockSize*/)
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

void BinauralStage::process(juce::AudioBuffer<float>& buffer)
{
    if (mode == BinauralMode::Off) return;
    applySpread(buffer);
}

void BinauralStage::applySpread(juce::AudioBuffer<float>& buffer)
{
    if (width < 1e-5f) return;

    const int numSamples = buffer.getNumSamples();
    const float w = width;

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mid  = (L[i] + R[i]) * 0.5f;
        const float side = (L[i] - R[i]) * 0.5f;

        // For mono signals, create side signal by rotating mid 90 degrees.
        // Simple approach: alternate sign per sample to inject decorrelation.
        const float injected = mid * w * 0.5f;  // decorrelation injection
        const float newSide  = side * (1.0f + w) + ((i & 1) ? injected : -injected);

        L[i] = mid + newSide;
        R[i] = mid - newSide;
    }
}
