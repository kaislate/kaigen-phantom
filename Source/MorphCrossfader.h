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

    /** Latch the per-block gain ramp: subsequent mix() calls interpolate
     *  from the previous block's gains to the current targets. Call once
     *  per audio block, after the setters, before any mix() calls. */
    void startBlock() noexcept;

    /** True when a side contributes nothing to this block — its gain ramp
     *  both starts and ends below the silence threshold. Callers use this
     *  (after startBlock()) to skip processing the idle engine without
     *  hard-cutting a morph ramp-out mid-transition. */
    bool aIsSilentThisBlock() const noexcept;
    bool bIsSilentThisBlock() const noexcept;

    /** Compute (gainA, gainB) at the current morph for the active curve. */
    void getCurrentGains(float& gainA, float& gainB) const noexcept;

    /** Mix two engine output buffers into `out`. All buffers must have
     *  the same channel count and sample count. `bypassA` / `bypassB`
     *  let the caller skip multiplying-and-adding a known-zero side. */
    void mix(const juce::AudioBuffer<float>& inA, bool bypassA,
             const juce::AudioBuffer<float>& inB, bool bypassB,
             juce::AudioBuffer<float>& out) const;

    /** Silence threshold (linear gain, ~-46 dB) for the per-block silence
     *  queries — below the noise floor of all reasonable output paths.
     *  Operating on the actual ramp gains makes the idle-bypass decision
     *  curve- and level-trim-aware. A loose threshold matters: idle bypass
     *  cuts ~50% CPU in the common case where morph parks at one engine. */
    static constexpr float kSilentGain = 0.005f;

private:
    Curve curve { Curve::Linear };
    float morph { 0.0f };
    float gainALin { 1.0f };  // from level_db
    float gainBLin { 1.0f };

    // Per-block gain ramp latched by startBlock(). When latched, mix()
    // interpolates rampFrom → rampTo across the block (dezippers morph /
    // level automation). When never latched (legacy callers), mix() applies
    // the flat target gains.
    float rampFromA { 1.0f }, rampFromB { 0.0f };
    float rampToA   { 1.0f }, rampToB   { 0.0f };
    bool  rampLatched { false };
    bool  firstBlock  { true };
};

} // namespace kaigen::phantom
