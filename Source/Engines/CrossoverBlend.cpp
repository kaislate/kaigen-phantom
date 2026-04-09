#include "CrossoverBlend.h"
#include <cmath>

void CrossoverBlend::prepare(double sr, int /*maxBlockSize*/) noexcept
{
    sampleRate = sr;
    rebuildCrossover(80.0f);
    updateDuckCoeffs();
    currentDuck = 1.0f;
    lpState[0] = lpState[1] = 0.0f;
}

void CrossoverBlend::reset() noexcept
{
    lpState[0] = lpState[1] = 0.0f;
    currentDuck = 1.0f;
}

void CrossoverBlend::setThresholdHz(float hz) noexcept
{
    if (std::abs(hz - lastThresholdHz) > 0.5f)
        rebuildCrossover(hz);
}

void CrossoverBlend::setDuckAttackMs(float ms) noexcept
{
    duckAttackMs = ms;
    updateDuckCoeffs();
}

void CrossoverBlend::setDuckReleaseMs(float ms) noexcept
{
    duckReleaseMs = ms;
    updateDuckCoeffs();
}

void CrossoverBlend::rebuildCrossover(float thresholdHz) noexcept
{
    lastThresholdHz = thresholdHz;
    // 1st-order LP coefficient: c = exp(-2π*fc/sr)
    lpCoeff = std::exp(-2.0f * 3.14159265f * thresholdHz / (float)sampleRate);
}

void CrossoverBlend::updateDuckCoeffs() noexcept
{
    duckAttackCoeff = (duckAttackMs > 0.0f)
        ? std::exp(-1.0f / (float)(sampleRate * duckAttackMs / 1000.0))
        : 0.0f;
    duckReleaseCoeff = (duckReleaseMs > 0.0f)
        ? std::exp(-1.0f / (float)(sampleRate * duckReleaseMs / 1000.0))
        : 0.0f;
}

void CrossoverBlend::process(juce::AudioBuffer<float>& dry,
                              const juce::AudioBuffer<float>& phantom,
                              const juce::AudioBuffer<float>* sidechain) noexcept
{
    if (ghost < 1e-5f) return;  // Ghost 0% = dry unchanged

    const int numSamples = dry.getNumSamples();
    const int numCh      = juce::jmin(dry.getNumChannels(), 2);

    // 1. Compute sidechain duck envelope (single gain value for entire block)
    if (sidechain != nullptr && duckAmount > 1e-5f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float sc = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                sc = std::max(sc, std::abs(sidechain->getSample(ch, i)));

            const float target = 1.0f - duckAmount * juce::jlimit(0.0f, 1.0f, sc);
            const float coeff  = (sc > (1.0f - currentDuck)) ? duckAttackCoeff : duckReleaseCoeff;
            currentDuck        = currentDuck * coeff + target * (1.0f - coeff);
        }
    }

    // 2. Mix dry with phantom per channel
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* dryData           = dry.getWritePointer(ch);
        const float* phantomData = phantom.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = dryData[i];

            // 1st-order LP: sub band
            lpState[ch] = lpCoeff * lpState[ch] + (1.0f - lpCoeff) * x;
            const float subBand  = lpState[ch];
            const float highBand = x - subBand;

            const float scaledPhantom = phantomData[i] * currentDuck * ghost;

            if (ghostMode == GhostMode::Replace)
            {
                // Remove sub content (fade from full-dry to high-only), add phantom
                const float subRemoved = highBand + subBand * (1.0f - ghost);
                dryData[i] = (subRemoved + scaledPhantom) * outputGain;
            }
            else  // Add
            {
                dryData[i] = (x + scaledPhantom) * outputGain;
            }
        }
    }

    // 3. Apply stereo width to final output
    if (dry.getNumChannels() >= 2 && std::abs(stereoWidth - 1.0f) > 0.01f)
        applyStereoWidth(dry, numSamples);
}

void CrossoverBlend::applyStereoWidth(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mid  = (L[i] + R[i]) * 0.5f;
        const float side = (L[i] - R[i]) * 0.5f * stereoWidth;
        L[i] = mid + side;
        R[i] = mid - side;
    }
}
