// Source/DSP/SingleKnobReverb.h
//
// Single-knob algorithmic reverb voiced to a specific Valhalla Vintage Verb
// preset (Concert Hall, 1970s color, 4 s decay, dark damping, slow attack,
// prominent ~2.5 Hz modulation). The user-facing parameter is wet amount only;
// every internal coefficient is baked.
//
// Topology: 8-tap FDN with Householder feedback matrix. Pre-tank chain: 8 kHz
// downsampling LPF (1970s color) → 20 ms predelay → 6-stage Schroeder allpass
// diffuser (early reflections / build-up shape). Each FDN delay line has:
//   • a per-line LFO at staggered rates near 2.53 Hz (delay-line modulation,
//     interpolated read tap)
//   • a one-pole damping LPF (≈ -24 dB shelf @ 6 kHz inside the loop)
//   • a one-pole bass-boost shelf that lifts content below ~300 Hz by ~1.5 ×
//     so the low end rings longer than the highs
// Output chain: 10 Hz HPF + 8 kHz LPF (the VVV-equivalent EQ Low/HighCut).
//
// Audio-thread safe: no heap allocations or locks in process(). Delay-line
// vectors are sized once in prepare() and reused.

#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

namespace kaigen::phantom
{

class SingleKnobReverb
{
public:
    SingleKnobReverb();

    /** Allocate delay-line storage and pre-compute coefficients. Must be
     *  called before process() at every sample-rate change. */
    void prepare(double sampleRate, int blockSize);

    /** Zero all internal state (delay lines, filter z-1 values, LFO phases).
     *  Call on a transport stop or preset reload to silence the tank. */
    void reset();

    /** In-place wet rendering. Caller passes a copy of the dry signal; on
     *  return the buffer holds 100 % wet (no internal dry-mix). The caller
     *  is responsible for the final `out = dry + wet * mix` combine — this
     *  keeps the reverb a strict parallel-send component. Stereo only;
     *  channels beyond 1 are ignored. */
    void process(juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kNumLines = 8;

    // Delay-line lengths (samples at 44.1 kHz). Mutually-prime sized so the
    // FDN modes never align — gives the dense, hall-like late field. These
    // are the *base* lengths; the actual buffer size adds head-room for
    // modulation and gets rescaled to the prepared sample rate.
    static constexpr int kBaseDelaysSamples[kNumLines] = {
        1789, 2017, 2371, 2647, 2939, 3217, 3539, 3823
    };

    // Predelay: 20 ms baked in. At 192 kHz that's < 4096 samples.
    static constexpr int kPredelayMaxSamples = 4096;

    // Early-diffusion allpass stages (6-stage Schroeder cascade). Mutually-
    // prime so the diffuser fingerprint stays smooth. Coefficient (g) chosen
    // for ~78 % diffusion — enough to smear transients without ringing.
    static constexpr int kNumEarlyAPs = 6;
    static constexpr int kEarlyAPSamples[kNumEarlyAPs] = { 113, 197, 313, 421, 571, 691 };
    static constexpr float kEarlyAPGain = 0.7f;

    // ── State ─────────────────────────────────────────────────────────────
    double sr { 44100.0 };
    int    predelayLenSamp { 0 };
    float  modDepthSamples { 0.0f };

    // Predelay line (mono — both channels share predelay; stereo diverges
    // inside the FDN where lines 0..3 feed L and 4..7 feed R).
    std::vector<float> predelayBuf;
    int                predelayPos { 0 };

    // Pre-tank LPF (8 kHz, one-pole). 1970s downsample colour.
    float pretankLpfState { 0.0f };
    float pretankLpfCoef  { 0.0f };

    // Early-diffusion allpass cascade. Per-channel state so a stereo input
    // doesn't collapse to mono before reaching the tank.
    struct AllpassLine
    {
        std::vector<float> buf;
        int pos { 0 };
    };
    std::array<AllpassLine, kNumEarlyAPs> earlyAPL;
    std::array<AllpassLine, kNumEarlyAPs> earlyAPR;

    // FDN delay lines (mono per line). Each line has its own modulation LFO
    // and per-line filters (damping LPF + bass boost shelf).
    struct DelayLine
    {
        std::vector<float> buf;
        int  writePos { 0 };
        int  baseLen  { 0 };       // unmodulated length (samples)

        float lfoPhase { 0.0f };
        float lfoInc   { 0.0f };   // per-sample phase advance (radians)

        float dampingLpfState { 0.0f };
        float dampingCoef     { 0.0f }; // one-pole LPF (≈ -24 dB above 6 kHz)

        float bassBoostState  { 0.0f };
        float bassBoostCoef   { 0.0f }; // one-pole LPF for the bass shelf
    };
    std::array<DelayLine, kNumLines> lines;

    // Output filters (per channel).
    float outLpfL { 0.0f }, outLpfR { 0.0f };
    float outLpfCoef { 0.0f };
    float outHpfStateL { 0.0f }, outHpfStateR { 0.0f };
    float outHpfPrevInL { 0.0f },  outHpfPrevInR { 0.0f };
    float outHpfCoef    { 0.0f };

    // Static output low-shelf — VVV "bass mult" style coloration applied
    // post-tank, post-band-limit. One-pole LPF state + a fractional
    // add-back gives ~+4 dB below ~250 Hz, flat above. Always on;
    // stability is irrelevant because it sits outside the feedback loop.
    float outShelfL { 0.0f }, outShelfR { 0.0f };
    float outShelfCoef { 0.0f };
    static constexpr float kOutShelfHz  = 250.0f;
    static constexpr float kOutShelfMul = 0.585f;  // ~+4 dB at DC (1 + 0.585 = 1.585)

    // Per-line LFO base rate (Hz). Each line picks a slightly different rate
    // around this centre so the modulation chorusses rather than rings as one.
    static constexpr float kModRateHz = 2.53f;

    // Damping target: high frequencies above 6 kHz lose ≈ 24 dB inside the
    // loop. We approximate the shelf with a one-pole LPF set to the −3 dB
    // point that yields that decay per-pass at the chosen decay time.
    static constexpr float kDampingFreqHz = 6000.0f;

    // Bass-boost shelf corner. Content below this frequency gets the loop-
    // gain bump (so the low end rings longer than the mids/highs).
    static constexpr float kBassShelfHz = 300.0f;

    // Loop gain → 4 s RT60 at the longest delay line. Computed in prepare().
    float baseFeedbackGain { 0.0f };

    // In-loop bass-shelf weight (used in process() as `tap += bassBoostMul * state`).
    // Computed in prepare() from `baseFeedbackGain` so the total per-pass loop gain
    // at DC — `(1 + bassBoostMul) * baseFeedbackGain` — stays under a stability
    // margin. The original design hard-coded 0.5 here, which combined with the 4 s
    // RT60 feedback gain (~0.86) gave a DC loop gain of ~1.29 and the tank ran
    // away into self-oscillation. See prepareDelayLines() for the bound.
    float bassBoostMul { 0.0f };

    // 1970s downsample LPF target.
    static constexpr float kPretankLpfHz = 8000.0f;

    // Output band-limit (matches VVV EQ HighCut 8 kHz / LowCut 10 Hz).
    static constexpr float kOutLpfHz = 8000.0f;
    static constexpr float kOutHpfHz = 10.0f;

    // Predelay (20 ms) and decay (4 s) — baked.
    static constexpr float kPredelayMs = 20.0f;
    static constexpr float kDecaySeconds = 4.0f;

    // ── Helpers ───────────────────────────────────────────────────────────
    void prepareDelayLines();
    void prepareFilters();
    static float onePoleCoef(float cutoffHz, double sampleRate);
    float readDelayInterpolated(const DelayLine& dl, float delaySamples) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SingleKnobReverb)
};

} // namespace kaigen::phantom
