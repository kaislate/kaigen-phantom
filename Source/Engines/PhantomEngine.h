#pragma once
#include <JuceHeader.h>
#include <array>
#include "BassExtractor.h"
#include "ZeroCrossingSynth.h"
#include "WaveletSynth.h"
#include "EnvelopeFollower.h"
#include "BinauralStage.h"
#include <juce_dsp/juce_dsp.h>

/**
 * PhantomEngine — top-level DSP container for the Kaigen Phantom plugin.
 *
 * Signal flow:
 *   input → BassExtractor.low → ZeroCrossingSynth → envelope-gated phantom
 *         → BassExtractor.high (pass-through)
 *         → Ghost Mix (Replace/Add) → BinauralStage → Stereo Width → Output
 *
 * ZeroCrossingSynth detects the dominant period from positive-slope zero
 * crossings and synthesises only H2-H8 — the fundamental is never present
 * in the output by construction, so no HPF is needed in Replace mode.
 */
class PhantomEngine
{
public:
    void prepare(double sampleRate, int blockSize, int numChannels);
    void reset();

    // ─── Parameter setters (called once per block from APVTS) ────────────
    void setCrossoverHz(float hz);
    void setHarmonicAmplitudes(const std::array<float, 7>& amps);
    void setSaturation(float sat);           // post-synthesis tanh saturation
    void setSynthStep(float step);           // 0=sine 1=square waveform morph
    void setSynthDuty(float duty);           // 0.05-0.95 pulse width
    void setSynthSkip(int n);               // 1-8 zero-crossing accumulator
    void setGhostAmount(float amt);          // [0..1] wet/dry
    void setGhostReplace(bool replace);      // true = replace, false = add
    void setPhantomStrength(float s);        // [0..1]
    void setOutputGainDb(float db);
    void setEnvelopeAttackMs(float ms);
    void setEnvelopeReleaseMs(float ms);
    void setBinauralMode(int mode);          // 0=off, 1=spread, 2=voice-split
    void setBinauralWidth(float width);      // [0..1]
    void setStereoWidth(float width);        // [0..2]
    void setSynthLPF(float hz);            // 200–20000 Hz, default 20000 (transparent)
    void setSynthHPF(float hz);            // 20–2000 Hz,   default 20   (transparent)
    void setSynthMode(int mode);           // 0 = Effect (ZCS), 1 = RESYN (WaveletSynth)

    // ─── Audio processing ────────────────────────────────────────────────
    void process(juce::AudioBuffer<float>& buffer);

    // ─── Oscilloscope capture (written by audio thread) ──────────────────
    static constexpr int kOscBufSize = 2048;
    std::array<float, kOscBufSize> oscSynthBuf {};   // phantom harmonics (ch 0, envelope-scaled)
    std::atomic<int>               oscSynthWrPos { 0 };

private:
    BassExtractor     bassExtractor;
    ZeroCrossingSynth synthL;
    ZeroCrossingSynth synthR;
    WaveletSynth      resynL;
    WaveletSynth      resynR;
    std::atomic<int>  synthMode { 0 };    // 0 = Effect, 1 = RESYN
    EnvelopeFollower  envelopeL;
    EnvelopeFollower  envelopeR;
    BinauralStage     binaural;

    juce::AudioBuffer<float> lowBuf;
    juce::AudioBuffer<float> highBuf;

    // Post-synthesis saturation (smoothed per-sample)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothSatL;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothSatR;

    // IIR filters on the synthesised harmonics signal (applied after saturation, before envelope scale)
    juce::IIRFilter lpfL, lpfR;
    juce::IIRFilter hpfL, hpfR;
    float lastLPFHz = 20000.0f;
    float lastHPFHz = 20.0f;

    // Block-rate params
    float ghostAmount     = 1.0f;
    bool  ghostReplace    = true;
    float phantomStrength = 0.8f;
    float outputGainLin   = 1.0f;
    float stereoWidth     = 1.0f;

    double sampleRate   = 44100.0;
    float  crossoverHz  = 120.0f;
    int    maxBlockSize = 0;
    int    numChannels  = 2;
};
