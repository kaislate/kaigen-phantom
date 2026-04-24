#pragma once
#include <JuceHeader.h>
#include <optional>
#include "Parameters.h"
#include "Engines/PhantomEngine.h"
#include "PresetManager.h"
#include "ABSlotManager.h"
#ifdef KAIGEN_PRO_BUILD
#include "MorphEngine.h"
#endif

class PhantomProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    PhantomProcessor();
    ~PhantomProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return apvts.getParameter(ParamID::BYPASS);
    }

    const juce::String getName() const override { return "Kaigen Phantom"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override
    {
        const float releaseMs = apvts.getRawParameterValue(ParamID::ENV_RELEASE_MS)->load();
        return (double)(releaseMs / 1000.0f) + 0.1;
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    kaigen::phantom::PresetManager& getPresetManager() { return presetManager; }

    // Public state for the editor
    juce::AudioProcessorValueTreeState apvts;

    kaigen::phantom::PresetManager presetManager;

    // NOTE: MUST be declared AFTER `apvts`. Constructor subscribes to APVTS
    // parameter listeners in order (see Parameters.h getAllParameterIDs()).
    // If moved above `apvts`, the listener registration loop will find no
    // registered parameters and the modified-flag tracking will silently break.
    kaigen::phantom::ABSlotManager abSlots { apvts };

    kaigen::phantom::ABSlotManager& getABSlotManager() { return abSlots; }

    // Real-time data exposed to the UI
    std::atomic<float> currentPitch { -1.0f };
    std::atomic<float> peakInL  { 0.0f };
    std::atomic<float> peakInR  { 0.0f };
    std::atomic<float> peakOutL { 0.0f };
    std::atomic<float> peakOutR { 0.0f };

    static constexpr int kSpectrumBins = 80;
    std::array<float, kSpectrumBins> spectrumData {};       // input (pre-engine)
    std::array<float, kSpectrumBins> spectrumOutputData {}; // output (post-engine)
    std::atomic<bool> spectrumReady { false };

    // Oscilloscope ring buffers (written by audio thread, read by editor)
    static constexpr int kOscBufSize = PhantomEngine::kOscBufSize;
    std::array<float, kOscBufSize> oscInputBuf  {};
    std::array<float, kOscBufSize> oscOutputBuf {};
    std::atomic<int>               oscInputWrPos  { 0 };
    std::atomic<int>               oscOutputWrPos { 0 };

    // Engine — public so editor can read oscilloscope synth capture
    PhantomEngine engine;

  #ifdef KAIGEN_PRO_BUILD
    // NOTE: Constructed in PhantomProcessor ctor body (not initializer list)
    // because the sync lambda captures `this`, which requires the object to be
    // sufficiently constructed (apvts, abSlots, engine all exist first).
    // MUST be declared AFTER apvts, abSlots, and engine — same ordering rules
    // as the old direct-init, now enforced by ctor-body emplace order.
    std::optional<kaigen::phantom::MorphEngine> morphOpt;

    kaigen::phantom::MorphEngine& getMorphEngine() { return *morphOpt; }
  #endif

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

    void syncParamsToEngine(PhantomEngine& target);
    // Sync an engine from any source of denormalized param values (not just apvts).
    // Used by MorphEngine's Scene Crossfade to drive the secondary engine from
    // slot B's ValueTree without mutating the primary APVTS state.
    void syncEngineFromValueLookup(PhantomEngine& target,
                                   std::function<float(const char*)> valueFor);

    double sampleRate = 44100.0;

    // Pre-allocated sidechain buffer (avoid heap allocation on audio thread)
    juce::AudioBuffer<float> sidechainBuf;

    // Auto input gain state
    // Peak envelope follower: fast attack, slow release.
    // Gain smoothed separately to prevent zipper noise on level changes.
    float autoEnvelope   = 0.0f;  // running peak envelope
    float autoGain       = 1.0f;  // current applied gain (smoothed)
    float autoAttackCoef  = 0.0f; // per-block attack coefficient (computed in prepareToPlay)
    float autoReleaseCoef = 0.0f; // per-block release coefficient
    float autoSmoothCoef  = 0.0f; // per-block gain-smoothing coefficient

    // FFT for spectrum analysis — 8192-point for ~5Hz resolution
    static constexpr int kFftOrder = 13;
    static constexpr int kFftSize  = 1 << kFftOrder;
    juce::dsp::FFT spectrumFFT { kFftOrder };
    std::array<float, kFftSize * 2> fftBuffer {};       // input (pre-engine)
    std::array<float, kFftSize * 2> fftOutputBuffer {}; // output (post-engine)
    int fftWritePos       = 0;
    int fftOutputWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomProcessor)
};
