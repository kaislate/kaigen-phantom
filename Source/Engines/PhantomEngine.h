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
 *   input → BassExtractor.low → ZeroCrossingSynth (Effect) or WaveletSynth (RESYN)
 *         → envelope-gated phantom
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
    void setGhostMode(int mode);             // 0 = Replace, 1 = Combine, 2 = Phantom Only
    void setPhantomStrength(float s);        // [0..1]
    void setOutputGainDb(float db);
    void setEnvelopeAttackMs(float ms);
    void setEnvelopeReleaseMs(float ms);
    void setBinauralMode(int mode);          // 0=off, 1=spread, 2=voice-split
    void setBinauralWidth(float width);      // [0..1]
    void setStereoWidth(float width);        // [0..2]
    void setSynthLPF(float hz);            // 200–20000 Hz, default 20000 (transparent)
    void setSynthHPF(float hz);            // 20–2000 Hz,   default 20   (transparent)
    void setSynthFilterSlope(int dBPerOct); // 6, 12, or 24 dB/oct (applies to both LPF and HPF)
    void setSynthMode(int mode);           // 0 = Effect (ZCS), 1 = RESYN (WaveletSynth)
    void setWaveletLength(float len);      // RESYN only: 0.05–1.0 fraction of period
    void setGateThreshold(float thr);      // RESYN only: 0.0–1.0 min negative-peak threshold
    void setEnvSource(int s);
    void setTrackingSpeed(float speed);    // EMA alpha [0.01–0.8]: low=stable/glide, high=fast
    void setMinPeriodSamples(float samples);   // min waveset length [2–500 samples]
    void setMaxPeriodSamples(float samples);   // max waveset length [100–8000 samples]
    void setH1Amplitude(float amp);        // RESYN only: H1 level [0–2]
    void setSubAmplitude(float amp);       // RESYN only: sub-harmonic level [0–2]
    void setUsePunch(bool on);             // enable per-wavelet peak amplitude modulation
    void setPunchAmount(float amount);     // [0–1]: 0 = pure envelope, 1 = pure wavelet peak
    void setBoostThreshold(float thr);     // RESYN only: upward expansion threshold [0–1]
    void setBoostAmount(float amt);        // RESYN only: upward expansion gain [0–2]
    void setInputDetectionGain(float lin); // scales what the synth's period/gate/boost detection sees; audio path stays at unity
    void setMidiTriggerEnabled(bool on);   // when true, MIDI note-on retriggers the envelope; audio transient retrigger is suppressed
    void setMidiGateRelease(bool on);      // when true (and MIDI trigger enabled), MIDI note-off forces envelope release + free-run

    // ─── MIDI event handlers (called from PluginProcessor per MIDI event) ───
    void handleMidiNoteOn() noexcept;
    void handleMidiNoteOff() noexcept;

    /** Current tracked pitch in Hz (from the active engine's zero-crossing tracker).
     *  Returns 0 when the input is too quiet to give reliable pitch info. */
    float getEstimatedHz() const noexcept;
    /** Running peak of the bass-band signal seen by the active synth engine.
     *  Used by the UI to position gate threshold lines accurately — the gate
     *  operates on the bass band, not the full input signal shown in the scope. */
    float getSynthInputPeak() const noexcept;

    // ─── Audio processing ────────────────────────────────────────────────
    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain = nullptr);

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

    // IIR filters on the synthesised harmonics signal (applied after saturation, before envelope scale).
    // Three slope options multiplex through different filter topologies:
    //   -6 dB/oct  → FirstOrderTPTFilter  (1-pole)
    //   -12 dB/oct → IIRFilter stage A only (Butterworth biquad, Q=1/sqrt(2))
    //   -24 dB/oct → IIRFilter stage A + stage B cascaded (4th-order Butterworth)
    juce::dsp::FirstOrderTPTFilter<float> lpf1pole;
    juce::dsp::FirstOrderTPTFilter<float> hpf1pole;
    juce::IIRFilter lpfL_sa, lpfR_sa, lpfL_sb, lpfR_sb;
    juce::IIRFilter hpfL_sa, hpfR_sa, hpfL_sb, hpfR_sb;
    float lastLPFHz = 20000.0f;
    float lastHPFHz = 20.0f;
    int   filterSlope = 12;  // 6, 12, or 24 dB/oct

    void recomputeLPFCoefs();
    void recomputeHPFCoefs();

    // Block-rate params
    float ghostAmount     = 1.0f;
    int   ghostMode       = 0;  // 0=Replace, 1=Combine, 2=Phantom Only
    float phantomStrength = 0.8f;
    float outputGainLin   = 1.0f;
    float stereoWidth     = 1.0f;
    int envSource = 0;  // 0 = main input bass band, 1 = sidechain

    double sampleRate   = 44100.0;
    float  crossoverHz  = 120.0f;
    int    maxBlockSize = 0;
    int    numChannels  = 2;

    bool  usePunch    = false;
    float punchAmount = 1.0f;

    float inputDetectionGain = 1.0f;
    bool  midiTriggerEnabled = false;
    bool  midiGateRelease    = false;
};
