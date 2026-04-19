#include "EnvelopeFollower.h"
#include <cmath>

void EnvelopeFollower::prepare(double sr) noexcept
{
    sampleRate = sr;
    env = 0.0f;
    peakHold = 0.0f;
    peakHoldSamplesLeft = 0;
    recomputeCoefs();
}

void EnvelopeFollower::reset() noexcept
{
    env = 0.0f;
    peakHold = 0.0f;
    peakHoldSamplesLeft = 0;
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

    // Peak-hold: hold flat for kPeakHoldMs (bridges troughs of pitched audio down
    // to ~25 Hz fundamentals with margin), then decay fast at kPostHoldMs rate.
    // Each new rectified peak restarts the hold timer, so during a sustained note
    // the one-pole sees a truly stable target and env can reach peak at the
    // user-set attack time regardless of release.
    static constexpr double kPeakHoldMs = 20.0;
    static constexpr double kPostHoldMs = 5.0;
    peakHoldMaxSamples    = static_cast<int>(kPeakHoldMs * 0.001 * srd);
    peakHoldPostDecayCoef = static_cast<float>(std::exp(-kReleaseLn / (kPostHoldMs * 0.001 * srd)));
}

float EnvelopeFollower::process(float input) noexcept
{
    const float rectified = std::fabs(input);

    // Peak-hold with true hold duration: each new peak restarts the hold timer,
    // so sustained pitched audio keeps the hold flat at peak. On silence the
    // hold drops to zero immediately so release engages at the full user rate.
    // Near-peak tolerance (kNearPeakRatio) keeps the timer alive when successive
    // crests of a steady sine fall slightly below the first due to sample
    // quantization — without this, env oscillates and creates AM sidebands that
    // leak the fundamental into the phantom output.
    constexpr float kSilenceFloor  = 1.0e-6f;
    constexpr float kNearPeakRatio = 0.98f;
    if (rectified < kSilenceFloor)
    {
        peakHold = 0.0f;
        peakHoldSamplesLeft = 0;
    }
    else if (rectified > peakHold)
    {
        peakHold = rectified;
        peakHoldSamplesLeft = peakHoldMaxSamples;
    }
    else if (rectified > peakHold * kNearPeakRatio)
    {
        peakHoldSamplesLeft = peakHoldMaxSamples;
    }
    else if (peakHoldSamplesLeft > 0)
    {
        --peakHoldSamplesLeft;
    }
    else
    {
        peakHold *= peakHoldPostDecayCoef;
    }

    const float target = forceReleaseActive ? 0.0f : peakHold;
    const float coef   = (target > env) ? attackCoef : releaseCoef;
    env += (target - env) * coef;

    if (forceReleaseActive && env < 1.0e-4f)
        forceReleaseActive = false;

    return env;
}
