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
    const double srd = juce::jmax(1.0, sampleRate);

    // Attack: ln(1000) → time to charge from 0 to 99.9% of target (−60 dB gap).
    // Fast and snappy — the envelope fully engages within the labelled attack time.
    static constexpr double kAttackLn = 6.907755278982137;  // ln(1000)
    attackCoef = 1.0f - static_cast<float>(std::exp(-kAttackLn / (attackMs * 0.001 * srd)));

    // Release: ln(10) → time to decay to 10% of peak (−20 dB).
    // Matches perceived release: the sound is still faintly audible at the labelled
    // time, then tails off naturally.  With ln(1000) the displayed time mapped to
    // −60 dB, making the last 2/3 of the release inaudibly quiet — a 5s release
    // sounded like ~1.7s.  ln(10) fixes this perceptual mismatch.
    static constexpr double kReleaseLn = 2.302585092994046;  // ln(10)
    releaseCoef = 1.0f - static_cast<float>(std::exp(-kReleaseLn / (releaseMs * 0.001 * srd)));
}

float EnvelopeFollower::process(float input) noexcept
{
    const float target = std::fabs(input);
    const float coef   = (target > env) ? attackCoef : releaseCoef;
    env += (target - env) * coef;
    return env;
}
