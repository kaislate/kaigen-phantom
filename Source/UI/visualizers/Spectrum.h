// Source/UI/visualizers/Spectrum.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class PhantomProcessor;

namespace kaigen::phantom
{

/** Native spectrum analyzer — full canvas port of spectrum.js.
 *
 *  Renders a dual-layer Bezier area-fill spectrum with:
 *    - Input signal: dim gray gradient fill + stroke
 *    - Output signal: bright white gradient fill + stroke (+ glow)
 *    - Peak-hold: thin white stroke line above output curve
 *    - Frequency grid (log scale) + dB grid
 *    - Frequency labels (30, 100, 300, 1k, 3k, 10k)
 *    - dB labels (0, -12, -24, -36, -48)
 *    - Crossover frequency line (vertical dashed blue)
 *    - Combined vs Split view modes
 *
 *  30 Hz Timer-driven repaint. Data comes from PhantomProcessor::spectrumData
 *  (input) and spectrumOutputData (output), plus engineASpectrum /
 *  engineBSpectrum for Split mode.
 */
class Spectrum : public juce::Component, private juce::Timer
{
public:
    Spectrum(PhantomProcessor& processor,
             juce::AudioProcessorValueTreeState& apvts);
    ~Spectrum() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    // ── Frequency / bin helpers ──────────────────────────────────────────
    static constexpr int   kBinCount         = 80;
    static constexpr float kBinFreqLow       = 30.0f;
    static constexpr float kBinFreqHigh      = 16000.0f;
    static constexpr float kDisplayFreqLow   = 20.0f;
    static constexpr float kDisplayFreqHigh  = 20000.0f;

    /** Bin index → centre frequency (log-spaced, matching spectrum.js binToFreq). */
    static float binToFreq(int bin) noexcept;

    /** Frequency → x pixel position (log10 scale, matching spectrum.js freqToX). */
    static float freqToX(float hz, float w) noexcept;

    // ── Bin smoothing ────────────────────────────────────────────────────
    static constexpr float kSmoothUp   = 0.5f;   // fast attack  (matches JS SMOOTH_UP)
    static constexpr float kSmoothDown = 0.08f;  // slow release (matches JS SMOOTH_DN)
    static constexpr float kPeakDecay  = 0.003f; // peak-hold decay per tick (matches JS)

    /** Apply asymmetric smoothing from an atomic-float source array
     *  (read with memory_order_relaxed) into smoothed[]. */
    void smoothBinsAtomic(
        const std::array<std::atomic<float>, kBinCount>& raw,
        std::array<float, kBinCount>& smoothed) noexcept;

    // ── Drawing helpers ──────────────────────────────────────────────────
    struct Pt { float x, y; };

    /** Build display-filtered list of (x,y) points from a bin array. */
    std::vector<Pt> buildCurvePoints(const std::array<float, kBinCount>& bins,
                                     float w, float h) const;

    /** Quadratic-Bezier stroke path through pts (matches spectrum.js strokeCurve). */
    static juce::Path makeStrokePath(const std::vector<Pt>& pts);

    /** Straight-line closed fill path through pts (matches spectrum.js fillCurve). */
    static juce::Path makeFillPath(const std::vector<Pt>& pts, float h);

    /** Draw frequency + dB grid + labels into g at (0,0,w,h). */
    static void drawGrid(juce::Graphics& g, float w, float h);

    /** Draw a single spectrum pane (Combined or one half of Split).
     *  xOffset is the canvas-space x translation already applied by caller.
     *  synthBins is the engine's phantom-only (post-filter, pre-mix) curve,
     *  drawn as a blue stroke between OUTPUT and PEAK so the user can see
     *  filter attenuation on the synth signal directly even when the dry
     *  pass-through in Combine/Replace modes leaves OUTPUT looking
     *  unaffected. nullptr to skip. */
    void drawPane(juce::Graphics& g,
                  float paneW, float paneH,
                  const std::array<float, kBinCount>& inBins,
                  const std::array<float, kBinCount>& outBins,
                  const std::array<float, kBinCount>* peakBins,  // nullptr → no peak line
                  const std::array<float, kBinCount>* synthBins, // nullptr → no synth line
                  float xoverHz) const;

    // ── State ────────────────────────────────────────────────────────────
    PhantomProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    std::array<float, kBinCount> smoothedIn       {};
    std::array<float, kBinCount> smoothedOut      {};
    std::array<float, kBinCount> peakOut          {};
    std::array<float, kBinCount> smoothedEngA     {};
    std::array<float, kBinCount> smoothedEngB     {};
    std::array<float, kBinCount> smoothedSynth    {};   // combined-mode synth-only
    std::array<float, kBinCount> smoothedSynthA   {};   // split-mode engine A synth
    std::array<float, kBinCount> smoothedSynthB   {};   // split-mode engine B synth

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Spectrum)
};

} // namespace kaigen::phantom
