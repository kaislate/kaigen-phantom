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

    /** Reset env and peak-hold to 0 so the Attack curve starts fresh. Called on
     *  MIDI note-on when MIDI triggering is active. Also clears any in-progress
     *  forced release. */
    void retrigger() noexcept { env = 0.0f; peakHold = 0.0f; forceReleaseActive = false; }

    /** Force the envelope into release (target → 0) regardless of audio input.
     *  Called on MIDI note-off when MIDI gate-release is active. Cleared
     *  automatically on next retrigger() or when env has decayed to near-zero. */
    void forceRelease() noexcept { forceReleaseActive = true; }

    /** True while a MIDI-gated release is in progress. Used by the engine to
     *  enable free-run on the wavelet synth so the tail rings past the
     *  amplitude floor. */
    bool isForceReleasing() const noexcept { return forceReleaseActive; }

private:
    double sampleRate  = 44100.0;
    float  env         = 0.0f;
    float  peakHold    = 0.0f;
    float  attackMs    = 1.0f;
    float  releaseMs   = 50.0f;
    float  attackCoef  = 0.0f;
    float  releaseCoef = 0.0f;
    int    peakHoldMaxSamples    = 0;   // hold duration at peak before decay begins
    int    peakHoldSamplesLeft   = 0;
    float  peakHoldPostDecayCoef = 0.0f; // fast decay after hold timer expires
    bool   forceReleaseActive = false;

    void recomputeCoefs() noexcept;
};
