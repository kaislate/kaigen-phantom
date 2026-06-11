#include "MorphCrossfader.h"
#include <cmath>

namespace kaigen::phantom
{

void MorphCrossfader::prepare(double, int) {
    // No allocations needed at prepare time; mix() reads buffers passed in.
    rampLatched = false;
    firstBlock  = true;
}

void MorphCrossfader::setLevels(float aDb, float bDb) noexcept
{
    gainALin = juce::Decibels::decibelsToGain(aDb);
    gainBLin = juce::Decibels::decibelsToGain(bDb);
}

void MorphCrossfader::setMorph(float n) noexcept
{
    morph = juce::jlimit(0.0f, 1.0f, n);
}

void MorphCrossfader::startBlock() noexcept
{
    float targetA = 0.0f, targetB = 0.0f;
    getCurrentGains(targetA, targetB);

    // First block after prepare: no previous gains to ramp from — snap so
    // a freshly-prepared instance doesn't fade in from stale state.
    rampFromA = firstBlock ? targetA : rampToA;
    rampFromB = firstBlock ? targetB : rampToB;
    rampToA   = targetA;
    rampToB   = targetB;

    firstBlock  = false;
    rampLatched = true;
}

bool MorphCrossfader::aIsSilentThisBlock() const noexcept
{
    return rampLatched
        && rampFromA <= kSilentGain && rampToA <= kSilentGain;
}

bool MorphCrossfader::bIsSilentThisBlock() const noexcept
{
    return rampLatched
        && rampFromB <= kSilentGain && rampToB <= kSilentGain;
}

void MorphCrossfader::getCurrentGains(float& gA, float& gB) const noexcept
{
    switch (curve)
    {
        case Curve::Linear:
            gA = (1.0f - morph) * gainALin;
            gB = morph           * gainBLin;
            break;
        case Curve::EqualPower:
        {
            const float theta = morph * juce::MathConstants<float>::halfPi;
            gA = std::cos(theta) * gainALin;
            gB = std::sin(theta) * gainBLin;
            break;
        }
        case Curve::SCurve:
        {
            // Smoothstep on morph for both sides
            const float t  = morph * morph * (3.0f - 2.0f * morph);
            gA = (1.0f - t) * gainALin;
            gB = t           * gainBLin;
            break;
        }
    }
}

void MorphCrossfader::mix(const juce::AudioBuffer<float>& inA, bool bypassA,
                          const juce::AudioBuffer<float>& inB, bool bypassB,
                          juce::AudioBuffer<float>& out) const
{
    const int nCh = out.getNumChannels();
    const int nSm = out.getNumSamples();
    if (nSm == 0) return;

    // Ramp from the previous block's gains to the current targets so morph
    // / level automation doesn't step at block boundaries. Without a
    // startBlock() latch, fall back to flat target gains.
    float gStartA = 0, gStartB = 0, gEndA = 0, gEndB = 0;
    if (rampLatched)
    {
        gStartA = rampFromA; gEndA = rampToA;
        gStartB = rampFromB; gEndB = rampToB;
    }
    else
    {
        getCurrentGains(gEndA, gEndB);
        gStartA = gEndA;
        gStartB = gEndB;
    }
    const float stepA = (gEndA - gStartA) / (float) nSm;
    const float stepB = (gEndB - gStartB) / (float) nSm;

    for (int c = 0; c < nCh; ++c)
    {
        auto* dst = out.getWritePointer(c);
        const auto* aPtr = (!bypassA && c < inA.getNumChannels()) ? inA.getReadPointer(c) : nullptr;
        const auto* bPtr = (!bypassB && c < inB.getNumChannels()) ? inB.getReadPointer(c) : nullptr;
        float gA = gStartA + stepA;   // sample i carries gain at (i+1)/n so
        float gB = gStartB + stepB;   // the final sample lands on the target
        for (int i = 0; i < nSm; ++i)
        {
            const float a = aPtr ? aPtr[i] * gA : 0.0f;
            const float b = bPtr ? bPtr[i] * gB : 0.0f;
            dst[i] = a + b;
            gA += stepA;
            gB += stepB;
        }
    }
}

} // namespace kaigen::phantom
