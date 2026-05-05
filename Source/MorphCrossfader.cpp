#include "MorphCrossfader.h"
#include <cmath>

namespace kaigen::phantom
{

void MorphCrossfader::prepare(double, int) {
    // No allocations needed at prepare time; mix() reads buffers passed in.
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

    float gA = 0, gB = 0;
    getCurrentGains(gA, gB);

    for (int c = 0; c < nCh; ++c)
    {
        auto* dst = out.getWritePointer(c);
        const auto* aPtr = (!bypassA && c < inA.getNumChannels()) ? inA.getReadPointer(c) : nullptr;
        const auto* bPtr = (!bypassB && c < inB.getNumChannels()) ? inB.getReadPointer(c) : nullptr;
        for (int i = 0; i < nSm; ++i)
        {
            const float a = aPtr ? aPtr[i] * gA : 0.0f;
            const float b = bPtr ? bPtr[i] * gB : 0.0f;
            dst[i] = a + b;
        }
    }
}

} // namespace kaigen::phantom
