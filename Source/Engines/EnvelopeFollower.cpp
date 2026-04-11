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
    // One-pole: coef = 1 - exp(-1 / (t * sr))
    // Larger coef = faster response. At t→0, coef→1 (instant). At t→∞, coef→0.
    const double srd = juce::jmax(1.0, sampleRate);
    attackCoef  = 1.0f - static_cast<float>(std::exp(-1.0 / (attackMs  * 0.001 * srd)));
    releaseCoef = 1.0f - static_cast<float>(std::exp(-1.0 / (releaseMs * 0.001 * srd)));
}

float EnvelopeFollower::process(float input) noexcept
{
    const float target = std::fabs(input);
    const float coef   = (target > env) ? attackCoef : releaseCoef;
    env += (target - env) * coef;
    return env;
}
