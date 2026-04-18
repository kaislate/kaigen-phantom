#pragma once
#include <JuceHeader.h>

/**
 * EnvelopeFollower — peak envelope detector with asymmetric attack/release.
 *
 * Rectified input feeds a short peak-hold stage (~30 ms) that bridges the
 * per-cycle troughs of pitched audio, then a one-pole smoother with separate
 * attack and release coefficients. Without the peak-hold stage, a long attack
 * combined with a short release would cause env to settle below peak on
 * oscillating input; with it, env can actually reach the input's peak level
 * over the user-set attack time.
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
    double sampleRate  = 44100.0;
    float  env         = 0.0f;
    float  peakHold    = 0.0f;
    float  attackMs    = 1.0f;
    float  releaseMs   = 50.0f;
    float  attackCoef  = 0.0f;
    float  releaseCoef = 0.0f;
    float  peakHoldCoef = 0.0f;  // per-sample decay multiplier for peak-hold stage

    void recomputeCoefs() noexcept;
};
