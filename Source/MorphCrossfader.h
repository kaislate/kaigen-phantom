// Source/MorphCrossfader.h
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace kaigen::phantom
{

/**
 * MorphCrossfader — block-rate audio crossfade between two engine outputs.
 *
 *   morph = 0   → A only
 *   morph = 1   → B only
 *   morph = 0.5 → mix per chosen curve
 *
 * Idle-engine bypass: when smoothed morph sits exactly at 0 (or 1), the
 * caller can skip processing the silent engine. mix() takes a `bypassA`
 * and `bypassB` flag from the caller; if a side is bypassed, its input
 * buffer is treated as zero (so caller need not actually fill it).
 */
class MorphCrossfader
{
public:
    enum class Curve { Linear, EqualPower, SCurve };

    void prepare(double sampleRate, int blockSize);

    void setCurve(Curve c) noexcept              { curve = c; }
    void setLevels(float aDb, float bDb) noexcept;
    void setMorph(float normalised) noexcept;       // [0,1]

    /** Compute (gainA, gainB) at the current morph for the active curve. */
    void getCurrentGains(float& gainA, float& gainB) const noexcept;

    /** Mix two engine output buffers into `out`. All buffers must have
     *  the same channel count and sample count. `bypassA` / `bypassB`
     *  let the caller skip multiplying-and-adding a known-zero side. */
    void mix(const juce::AudioBuffer<float>& inA, bool bypassA,
             const juce::AudioBuffer<float>& inB, bool bypassB,
             juce::AudioBuffer<float>& out) const;

    /** Threshold for "effectively at 0" / "effectively at 1" — lets caller
     *  decide bypass. At 0.005 the crossfade gain on the inactive side is
     *  ~-46 dB (linear-curve) — below the noise floor of all reasonable
     *  output paths. Bumping from 1e-6 cut idle CPU ~50% in the common case
     *  where the user parks morph at one engine. */
    static constexpr float kBypassEpsilon = 0.005f;

private:
    Curve curve { Curve::Linear };
    float morph { 0.0f };
    float gainALin { 1.0f };  // from level_db
    float gainBLin { 1.0f };
};

} // namespace kaigen::phantom
