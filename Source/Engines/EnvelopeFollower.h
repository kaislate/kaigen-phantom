#pragma once
#include <JuceHeader.h>

/**
 * EnvelopeFollower — peak envelope detector with asymmetric attack/release.
 *
 * Feeds a rectified input into a one-pole smoothing filter with separate
 * attack and release coefficients. Output is sample-accurate and never
 * overshoots or hangs — when the input stops, the envelope decays to zero
 * within the release time.
 */
class EnvelopeFollower
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    /** Attack time in milliseconds. Default 1ms. */
    void setAttackMs(float ms) noexcept;

    /** Release time in milliseconds. Default 50ms. */
    void setReleaseMs(float ms) noexcept;

    /** Processes one sample. Returns the smoothed envelope value. */
    float process(float input) noexcept;

    /** Returns the current envelope value without advancing. */
    float getValue() const noexcept { return env; }

private:
    double sampleRate = 44100.0;
    float  env        = 0.0f;
    float  attackMs   = 1.0f;
    float  releaseMs  = 50.0f;
    float  attackCoef = 0.0f;
    float  releaseCoef = 0.0f;

    void recomputeCoefs() noexcept;
};
