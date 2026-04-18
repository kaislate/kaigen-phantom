#include "EnvelopeFollower.h"
#include <cmath>

void EnvelopeFollower::prepare(double sr) noexcept
{
    sampleRate = sr;
    env = 0.0f;
    recomputeCoefs();
}

void EnvelopeFollower::reset() noexcept
{
    env = 0.0f;
}

void EnvelopeFollower::setAttackMs(float ms) noexcept
{
    attackMs = juce::jmax(0.01f, ms);
    recomputeCoefs();
}

void EnvelopeFollower::setReleaseMs(float ms) noexcept
{
    releaseMs = juce::jmax(0.01f, ms);
    recomputeCoefs();
}

void EnvelopeFollower::recomputeCoefs() noexcept
{
    // One-pole: coef = 1 - exp(-ln(1000) / (t * sr))
    // Defines t as the time to decay from peak to −60 dBFS (factor 0.001),
    // matching the standard audio engineering definition used by compressors.
    // Using -1 instead of -ln(1000) would give the RC time constant (1/e in t),
    // making the audible release ~7× longer than the labelled value.
    const double srd    = juce::jmax(1.0, sampleRate);
    const double ln1000 = 6.907755278982137; // ln(1000) = 3 * ln(10)
    attackCoef  = 1.0f - static_cast<float>(std::exp(-ln1000 / (attackMs  * 0.001 * srd)));
    releaseCoef = 1.0f - static_cast<float>(std::exp(-ln1000 / (releaseMs * 0.001 * srd)));
}

float EnvelopeFollower::process(float input) noexcept
{
    const float target = std::fabs(input);
    const float coef   = (target > env) ? attackCoef : releaseCoef;
    env += (target - env) * coef;
    return env;
}
