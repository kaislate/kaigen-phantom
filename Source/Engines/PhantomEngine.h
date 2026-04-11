#pragma once
#include <JuceHeader.h>
#include <array>
#include "BassExtractor.h"
#include "Waveshaper.h"
#include "EnvelopeFollower.h"
#include "BinauralStage.h"

/**
 * PhantomEngine — top-level DSP container for the Kaigen Phantom plugin.
 *
 * Signal flow:
 *   input → BassExtractor.low → Waveshaper → envelope-gated phantom signal
 *         → BassExtractor.high (pass-through)
 *         → Mix via Ghost (Replace/Add) → BinauralStage → Output Gain → output
 */
class PhantomEngine
{
public:
    void prepare(double sampleRate, int blockSize, int numChannels);
    void reset();

    // ─── Parameter setters (called once per block from APVTS) ────────────
    void setCrossoverHz(float hz);
    void setHarmonicAmplitudes(const std::array<float, 7>& amps);
    void setDrive(float drive);
    void setSaturation(float sat);
    void setGhostAmount(float amt);          // [0..1] wet/dry
    void setGhostReplace(bool replace);      // true = replace, false = add
    void setPhantomStrength(float s);        // [0..1]
    void setOutputGainDb(float db);
    void setEnvelopeAttackMs(float ms);
    void setEnvelopeReleaseMs(float ms);
    void setBinauralMode(int mode);          // 0 = off, 1 = spread, 2 = voice-split
    void setBinauralWidth(float width);      // [0..1]
    void setStereoWidth(float width);        // [0..2]

    // ─── Audio processing ────────────────────────────────────────────────
    void process(juce::AudioBuffer<float>& buffer);

private:
    BassExtractor    bassExtractor;
    Waveshaper       waveshaperL;
    Waveshaper       waveshaperR;
    EnvelopeFollower envelopeL;
    EnvelopeFollower envelopeR;
    BinauralStage    binaural;

    juce::AudioBuffer<float> lowBuf;     // bandpass-isolated bass
    juce::AudioBuffer<float> highBuf;    // pass-through upper band
    juce::AudioBuffer<float> phantomBuf; // shaped harmonics

    // Block-rate params
    float ghostAmount      = 1.0f;
    bool  ghostReplace     = true;
    float phantomStrength  = 0.8f;
    float outputGainLin    = 1.0f;
    float stereoWidth      = 1.0f;

    double sampleRate = 44100.0;
    int    maxBlockSize = 0;
    int    numChannels = 2;
};
