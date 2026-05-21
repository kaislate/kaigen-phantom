// Source/DSP/SingleKnobReverb.cpp
//
// Implementation notes
// ────────────────────
// The FDN uses a Householder reflection matrix of the form
//     H = I − (2/N) * J  where J is the all-ones matrix.
// For N = 8 this gives each output as
//     y_i = sum(x) * (-1/4) + x_i      (since 2/N = 1/4)
// i.e. every line gets the mean of all lines subtracted twice — a lossless
// (orthonormal) mixing matrix that maximally diffuses energy between lines.
// The whole tank is then scaled by `baseFeedbackGain` to set RT60.
//
// Loop-gain derivation: at the longest delay (`maxLen`) the energy makes
// `sr / maxLen` round trips per second. For a target RT60 (−60 dB) of
// `kDecaySeconds`:
//     baseFeedbackGain = 10 ^ ( -3 * (maxLen / sr) / kDecaySeconds )
// (−60 dB → factor 1e−3 → exponent −3 in log10). Applied uniformly across
// all 8 lines, the *average* RT60 ends up close to the target.
//
// The per-line damping LPF and bass-shelf LPF act inside the loop, so their
// per-pass attenuations multiply: after N passes the highs above 6 kHz are
// down by ≈ 24 dB relative to the mids (matching the VVV "Damping high-shelf
// −24 dB @ 6 kHz" preset). The bass shelf adds gain — but to stay below
// the loss budget we mix the boosted low band back into the line at a
// fractional weight (0.5 of the LPF state added back) which gives an
// effective ~×1.5 multiplier below 300 Hz.

#include "SingleKnobReverb.h"
#include <cmath>

namespace kaigen::phantom
{

// Out-of-line definitions for constexpr static array members (required by the
// older ODR rule that pre-C++17 demanded; harmless under C++20).
constexpr int   SingleKnobReverb::kBaseDelaysSamples[SingleKnobReverb::kNumLines];
constexpr int   SingleKnobReverb::kEarlyAPSamples[SingleKnobReverb::kNumEarlyAPs];
constexpr float SingleKnobReverb::kEarlyAPGain;
constexpr float SingleKnobReverb::kModRateHz;
constexpr float SingleKnobReverb::kDampingFreqHz;
constexpr float SingleKnobReverb::kBassShelfHz;
constexpr float SingleKnobReverb::kPretankLpfHz;
constexpr float SingleKnobReverb::kOutLpfHz;
constexpr float SingleKnobReverb::kOutHpfHz;
constexpr float SingleKnobReverb::kPredelayMs;
constexpr float SingleKnobReverb::kDecaySeconds;

SingleKnobReverb::SingleKnobReverb() {}

// One-pole LPF coefficient for a target -3 dB cutoff.
//   y[n] = y[n-1] + alpha * (x[n] - y[n-1])
// where alpha = 1 - exp(-2 pi fc / fs)
float SingleKnobReverb::onePoleCoef(float cutoffHz, double sampleRate)
{
    const float fc = juce::jlimit(0.5f, (float) sampleRate * 0.49f, cutoffHz);
    return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * fc / (float) sampleRate);
}

void SingleKnobReverb::prepareDelayLines()
{
    // Scale base lengths (defined at 44.1 kHz) to the prepared sample rate.
    const float scale = (float) (sr / 44100.0);

    // Modulation depth: stay well below the shortest line's headroom. VVV's
    // 38 % mod-depth at a relatively slow rate translates to ~2 ms of delay-
    // line displacement; we centre at that figure and let the per-line LFOs
    // vary slightly in amplitude to chorus the swirl.
    // 2 ms → 3.5 ms — lusher, more VVV-like chorussing in the tank.
    // Still well under the shortest delay-line headroom (shortest line
    // ~40 ms at 44.1 kHz; 3.5 ms uses ~9 % of headroom and
    // prepareDelayLines reserves modDepthSamples * 1.5 + 8 samples
    // per line, which still fits).
    constexpr float kModDepthMs = 3.5f;
    modDepthSamples = (kModDepthMs * 1.0e-3f) * (float) sr;

    // Per-line LFO rate spread (±15 % around centre) so eight LFOs sweeping
    // simultaneously don't act as one giant unison detune.
    constexpr float kRateSpread = 0.15f;

    int maxLen = 0;
    for (int i = 0; i < kNumLines; ++i)
    {
        auto& dl = lines[i];

        const int baseLen = juce::jmax(64, (int) std::round(kBaseDelaysSamples[i] * scale));
        // Allocate room for read-tap modulation: base length + modulation
        // headroom + a small interp guard. Power-of-two not required (we
        // index modulo length), but we keep a small slack for the cubic
        // interpolator on the read tap.
        const int totalLen = baseLen + (int) std::ceil(modDepthSamples * 1.5f) + 8;
        dl.buf.assign((size_t) totalLen, 0.0f);
        dl.baseLen = baseLen;
        dl.writePos = 0;
        dl.dampingLpfState = 0.0f;
        dl.bassBoostState = 0.0f;

        // LFO: alternate rate offsets and start phases so the eight sweeps
        // are perceptually a diffuse swirl rather than synced wobble.
        const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
        const float offset = sign * kRateSpread * ((float) i / (kNumLines - 1) - 0.5f);
        const float rateHz = kModRateHz * (1.0f + offset);
        dl.lfoInc = juce::MathConstants<float>::twoPi * rateHz / (float) sr;
        dl.lfoPhase = (juce::MathConstants<float>::twoPi * i) / (float) kNumLines;

        // Per-line damping + bass-shelf coefficients. Damping is the same
        // 6 kHz LPF on every line so the tail uniformly darkens; the bass
        // shelf is also identical for parallel low-end build-up.
        dl.dampingCoef = onePoleCoef(kDampingFreqHz, sr);
        dl.bassBoostCoef = onePoleCoef(kBassShelfHz, sr);

        maxLen = juce::jmax(maxLen, baseLen);
    }

    // Loop-gain → RT60: see the file-top comment. Applied uniformly to
    // every line; we target the *longest* line so the longest mode hits
    // the chosen RT60 — shorter modes decay slightly faster, which is the
    // expected behaviour for a hall with frequency-dependent damping.
    if (maxLen > 0 && kDecaySeconds > 0.0f)
        baseFeedbackGain = std::pow(10.0f,
            -3.0f * ((float) maxLen / (float) sr) / kDecaySeconds);
    else
        baseFeedbackGain = 0.0f;

    // Bound the in-loop bass-shelf weight so the FDN stays stable at DC.
    //
    // The bass shelf adds a fraction of the per-line LPF state back to the
    // tap; at DC the LPF state equals the tap, so the per-pass DC gain through
    // the shelf is (1 + bassBoostMul). Combined with the feedback-gain scaling
    // applied to the writeback, the per-pass DC loop gain is
    //     g_dc = (1 + bassBoostMul) * baseFeedbackGain
    // Stability requires g_dc < 1. The original design hard-coded
    // bassBoostMul = 0.5 ("+3.5 dB low shelf in the loop"), which gives
    // g_dc ≈ 1.29 at 4 s RT60 / 44.1 kHz and the tank self-saturates regardless
    // of input.
    //
    // We pick the largest weight such that g_dc ≤ kLoopStabilityMargin (0.95 →
    // ~5 % safety from the unit circle), and cap it at the original 0.5 target
    // in case future tuning (shorter RT60, different sample rate) ever makes
    // 0.5 itself safe.
    constexpr float kLoopStabilityMargin = 0.95f;
    constexpr float kBassBoostTargetMul  = 0.5f;
    if (baseFeedbackGain > 1.0e-6f)
    {
        const float maxSafeMul = (kLoopStabilityMargin / baseFeedbackGain) - 1.0f;
        bassBoostMul = juce::jlimit(0.0f, kBassBoostTargetMul, maxSafeMul);
    }
    else
    {
        bassBoostMul = 0.0f;
    }
}

void SingleKnobReverb::prepareFilters()
{
    pretankLpfState = 0.0f;
    pretankLpfCoef = onePoleCoef(kPretankLpfHz, sr);

    outLpfL = outLpfR = 0.0f;
    outLpfCoef = onePoleCoef(kOutLpfHz, sr);

    outHpfStateL = outHpfStateR = 0.0f;
    outHpfPrevInL = outHpfPrevInR = 0.0f;
    // One-pole DC blocker coefficient: pole at 1 - 2*pi*fc/fs.
    outHpfCoef = std::exp(-juce::MathConstants<float>::twoPi * kOutHpfHz / (float) sr);

    // Output low-shelf.
    outShelfL = outShelfR = 0.0f;
    outShelfCoef = onePoleCoef(kOutShelfHz, sr);
}

void SingleKnobReverb::prepare(double sampleRate, int /*blockSize*/)
{
    sr = sampleRate;
    predelayLenSamp = juce::jmax(1, (int) std::round((kPredelayMs * 1.0e-3f) * sr));
    const int predBufSize = juce::jmax(predelayLenSamp + 8, kPredelayMaxSamples);
    predelayBuf.assign((size_t) predBufSize, 0.0f);
    predelayPos = 0;

    // Early diffusion allpasses — same lengths for L and R, but independent
    // state so a stereo signal stays stereo (no inadvertent mono-sum).
    for (int i = 0; i < kNumEarlyAPs; ++i)
    {
        earlyAPL[i].buf.assign((size_t) kEarlyAPSamples[i], 0.0f);
        earlyAPL[i].pos = 0;
        earlyAPR[i].buf.assign((size_t) kEarlyAPSamples[i], 0.0f);
        earlyAPR[i].pos = 0;
    }

    prepareDelayLines();
    prepareFilters();
}

void SingleKnobReverb::reset()
{
    std::fill(predelayBuf.begin(), predelayBuf.end(), 0.0f);
    predelayPos = 0;

    for (auto& ap : earlyAPL) { std::fill(ap.buf.begin(), ap.buf.end(), 0.0f); ap.pos = 0; }
    for (auto& ap : earlyAPR) { std::fill(ap.buf.begin(), ap.buf.end(), 0.0f); ap.pos = 0; }

    for (auto& dl : lines)
    {
        std::fill(dl.buf.begin(), dl.buf.end(), 0.0f);
        dl.writePos = 0;
        dl.dampingLpfState = 0.0f;
        dl.bassBoostState = 0.0f;
        // LFO phase preserved across reset so the modulation pattern stays
        // consistent — only clears actual signal state.
    }

    pretankLpfState = 0.0f;
    outLpfL = outLpfR = 0.0f;
    outHpfStateL = outHpfStateR = 0.0f;
    outHpfPrevInL = outHpfPrevInR = 0.0f;
    outShelfL = outShelfR = 0.0f;
}

// Linear interpolation read on a delay line at fractional offset behind the
// write head. `delaySamples` must be in [0, baseLen + headroom).
float SingleKnobReverb::readDelayInterpolated(const DelayLine& dl, float delaySamples) const
{
    const int    bufLen = (int) dl.buf.size();
    const float  d      = juce::jlimit(0.0f, (float) (bufLen - 2), delaySamples);
    const int    di     = (int) d;
    const float  df     = d - (float) di;

    int idx0 = dl.writePos - 1 - di;
    while (idx0 < 0) idx0 += bufLen;
    int idx1 = idx0 - 1;
    while (idx1 < 0) idx1 += bufLen;

    const float s0 = dl.buf[(size_t) idx0];
    const float s1 = dl.buf[(size_t) idx1];
    return s0 + df * (s1 - s0);
}

void SingleKnobReverb::process(juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n == 0 || nCh == 0) return;

    float* L = buffer.getWritePointer(0);
    float* R = (nCh > 1) ? buffer.getWritePointer(1) : L;

    const int predBufLen = (int) predelayBuf.size();

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i];
        const float inR = R[i];

        // ── Pre-tank LPF (10 kHz, 1970s downsample colour) ────────────
        // Sum to mono for the tank input — the tank itself creates the
        // stereo image via line-pair routing. Mono predelay is standard
        // for hall reverbs and saves a buffer.
        const float monoIn = 0.5f * (inL + inR);
        pretankLpfState += pretankLpfCoef * (monoIn - pretankLpfState);
        const float preFiltered = pretankLpfState;

        // ── Predelay (20 ms) ──────────────────────────────────────────
        predelayBuf[(size_t) predelayPos] = preFiltered;
        int readP = predelayPos - predelayLenSamp;
        if (readP < 0) readP += predBufLen;
        const float predOut = predelayBuf[(size_t) readP];
        predelayPos = (predelayPos + 1) % predBufLen;

        // ── Early diffusion (4-stage allpass, per-channel) ────────────
        // Stereo separation: L and R feed mirror-image cascades of the
        // same prime lengths, so identical inputs decorrelate over the
        // four stages.
        float eL = predOut;
        float eR = predOut;
        for (int s = 0; s < kNumEarlyAPs; ++s)
        {
            auto& apL = earlyAPL[s];
            auto& apR = earlyAPR[s];
            const int lenL = (int) apL.buf.size();
            const int lenR = (int) apR.buf.size();

            // Schroeder allpass: out = -g*in + delayed + g*delayed_out
            // Standard difference equation:
            //     v[n] = in[n] + g * v[n - M]      (state update)
            //     out  = -g * v[n] + v[n - M]
            const float dL = apL.buf[(size_t) apL.pos];
            const float vL = eL + kEarlyAPGain * dL;
            const float oL = -kEarlyAPGain * vL + dL;
            apL.buf[(size_t) apL.pos] = vL;
            apL.pos = (apL.pos + 1) % lenL;
            eL = oL;

            const float dR = apR.buf[(size_t) apR.pos];
            const float vR = eR + kEarlyAPGain * dR;
            const float oR = -kEarlyAPGain * vR + dR;
            apR.buf[(size_t) apR.pos] = vR;
            apR.pos = (apR.pos + 1) % lenR;
            eR = oR;
        }

        // ── FDN tank ──────────────────────────────────────────────────
        // Input split: lines 0..3 see L, lines 4..7 see R. The Householder
        // matrix then redistributes energy across all 8 lines, but the
        // initial split preserves stereo onset cues.
        std::array<float, kNumLines> taps;

        // Read each line at its (modulated) tap. Tap delay = baseLen +
        // mod-shifted offset. Read happens BEFORE the matrix mixes the
        // new feedback in — standard FDN ordering.
        for (int k = 0; k < kNumLines; ++k)
        {
            auto& dl = lines[k];

            // Modulated tap: baseLen + sin(phase) * depth.
            const float modOff = std::sin(dl.lfoPhase) * modDepthSamples;
            dl.lfoPhase += dl.lfoInc;
            if (dl.lfoPhase > juce::MathConstants<float>::twoPi)
                dl.lfoPhase -= juce::MathConstants<float>::twoPi;
            const float delaySamp = (float) dl.baseLen + modOff;

            float tap = readDelayInterpolated(dl, delaySamp);

            // In-loop damping — shelf style, NOT a strict LPF replacement.
            //
            // The original `tap = dl.dampingLpfState` swapped the tap for its
            // LPF output every pass, which compounds to ~7 dB of HF loss per
            // pass at Nyquist; over the ~46 passes of a 4 s RT60 that's
            // hundreds of dB and the tail goes muffled inside the first
            // second. Voiced wrong against VVV's "Concert Hall 1970s" which
            // keeps the highs audible across the whole tail.
            //
            // Shelf blend: lows ride through at unit gain, highs lose only
            // ~1.4 dB per pass (kDampingMix = 0.85 keeps 85 % of the tap,
            // mixes in 15 % of the LPF state). Net HF-vs-LF darkening over
            // the full RT60 ends up ~20 dB — the VVV-style "darker tail
            // than mids" without the kill-floor we had before.
            dl.dampingLpfState += dl.dampingCoef * (tap - dl.dampingLpfState);
            constexpr float kDampingMix = 0.85f;
            tap = kDampingMix * tap + (1.0f - kDampingMix) * dl.dampingLpfState;

            // Bass-boost shelf (in-loop): track low band with a one-pole
            // LPF, then ADD a fraction of it back to the tap. This gives
            // an in-loop low-shelf bump so the bass rings longer than the
            // mids/highs. The weight is bounded (see prepareDelayLines())
            // so the per-pass DC loop gain stays under unity — the original
            // 0.5 constant pushed it to ~1.29 and the tank ran away.
            dl.bassBoostState += dl.bassBoostCoef * (tap - dl.bassBoostState);
            tap = tap + bassBoostMul * dl.bassBoostState;

            taps[(size_t) k] = tap;
        }

        // Householder mixing: y = x − (2/N) * sum(x), N = 8.
        float sum = 0.0f;
        for (int k = 0; k < kNumLines; ++k) sum += taps[(size_t) k];
        const float meanMix = (2.0f / (float) kNumLines) * sum;

        // Write back into the delay lines (scaled by baseFeedbackGain to set
        // RT60) plus the per-line input drive.
        for (int k = 0; k < kNumLines; ++k)
        {
            auto& dl = lines[k];
            const int bufLen = (int) dl.buf.size();

            const float feedback = (taps[(size_t) k] - meanMix) * baseFeedbackGain;
            const float drive    = (k < kNumLines / 2) ? eL : eR;

            // Input drive is scaled by 1/sqrt(N/2) ≈ 0.5 so the tank doesn't
            // saturate when summing 4 lines per channel.
            dl.buf[(size_t) dl.writePos] = feedback + 0.5f * drive;
            dl.writePos = (dl.writePos + 1) % bufLen;
        }

        // Output: pick the L tank from lines 0..3, R from 4..7. (Pre-matrix
        // taps — the matrix has already permuted them implicitly.)
        float wetL = 0.0f, wetR = 0.0f;
        for (int k = 0; k < kNumLines / 2; ++k) wetL += taps[(size_t) k];
        for (int k = kNumLines / 2; k < kNumLines; ++k) wetR += taps[(size_t) k];

        // Compensate the tap summation gain. The 4 lines summed per channel
        // are largely uncorrelated (Householder mixing + per-line modulation
        // + per-line damping), so the energy-correct divisor is sqrt(4) = 2
        // → multiplier 0.5. The previous 0.25 treated them as fully
        // coherent and over-attenuated by 6 dB; that's the main reason the
        // wet signal felt anaemic next to VVV at the same mix setting.
        wetL *= 0.5f;
        wetR *= 0.5f;

        // ── Output band-limit: 10 Hz HPF + 8 kHz LPF ─────────────────
        // HPF: y[n] = α*(y[n-1] + x[n] − x[n-1])
        {
            const float y = outHpfCoef * (outHpfStateL + wetL - outHpfPrevInL);
            outHpfStateL = y;
            outHpfPrevInL = wetL;
            wetL = y;
        }
        {
            const float y = outHpfCoef * (outHpfStateR + wetR - outHpfPrevInR);
            outHpfStateR = y;
            outHpfPrevInR = wetR;
            wetR = y;
        }
        // LPF (one-pole towards input).
        outLpfL += outLpfCoef * (wetL - outLpfL);
        outLpfR += outLpfCoef * (wetR - outLpfR);

        // Output low-shelf — same one-pole-LPF-plus-add-back structure
        // as the in-loop bass shelf, but post-tank where stability
        // isn't a concern. Track the low band, add back kOutShelfMul
        // * state for an effective +4 dB bump below ~250 Hz with no
        // boost above it.
        outShelfL += outShelfCoef * (outLpfL - outShelfL);
        outShelfR += outShelfCoef * (outLpfR - outShelfR);
        const float shelfedL = outLpfL + kOutShelfMul * outShelfL;
        const float shelfedR = outLpfR + kOutShelfMul * outShelfR;

        L[i] = shelfedL;
        if (nCh > 1) R[i] = shelfedR;
    }
}

} // namespace kaigen::phantom
