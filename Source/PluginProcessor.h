#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "Engines/PhantomEngine.h"
#include "PresetManager.h"
#include "DualEngineHost.h"
#include "EngineFocus.h"
#include "SpectrumViewMode.h"
#include "Modulation/ModulationEngine.h"
#include "Modulation/Macro.h"

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
        // Both engines run in parallel and either may be audible (depending on
        // morph), so the host-reported tail must cover whichever release is
        // longer. Add a small safety margin to outlast envelope tail-down.
        const float aMs = apvts.getRawParameterValue(ParamID::A_ENV_RELEASE_MS)->load();
        const float bMs = apvts.getRawParameterValue(ParamID::B_ENV_RELEASE_MS)->load();
        return (double)(juce::jmax(aMs, bMs) / 1000.0f) + 0.1;
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

    /** Selector for computeEngineSpectrum: which engine ring buffer to read. */
    enum class SpectrumEngineId { A, B };

    /** UI-thread helper. Reads the most recent kFftSize samples from the
     *  per-engine FFT ring buffer (populated in processBlock — see Task 3),
     *  applies the same Hann window + FFT + log-binning pipeline as the
     *  input/output spectra, and writes binned magnitudes (range 0..1) to
     *  `dst`. Called from the WebView native binding on the message thread. */
    void computeEngineSpectrum(SpectrumEngineId which,
                               std::array<float, kSpectrumBins>& dst) const;

    // Oscilloscope ring buffers (written by audio thread, read by editor)
    static constexpr int kOscBufSize = PhantomEngine::kOscBufSize;
    std::array<float, kOscBufSize> oscInputBuf  {};
    std::array<float, kOscBufSize> oscOutputBuf {};
    std::atomic<int>               oscInputWrPos  { 0 };
    std::atomic<int>               oscOutputWrPos { 0 };

    // Dual-engine host: owns A + B engine instances and the morph crossfader.
    // PR1 always shows engine A in the editor; PR2 introduces tab switching.
    kaigen::phantom::DualEngineHost dualEngineHost;

    /** Editor accessor: which engine the UI is currently visualising.
     *  PR1 = always A. PR2 will dispatch on the active tab. */
    PhantomEngine& getActiveEngine() noexcept { return dualEngineHost.getActiveEngine(); }
    kaigen::phantom::DualEngineHost& getDualEngineHost() noexcept { return dualEngineHost; }

    // ─── Modulation engines (PR3a) ────────────────────────────────────────
    // Per-engine modulation containers. Engine A scopes Macro 1+2 to a_*
    // params; Engine B scopes Macro 3+4 to b_* params. Routings between a
    // macro and a param are owned by the corresponding engine here.
    kaigen::phantom::ModulationEngine& getModulationEngineA() noexcept { return modEngineA; }
    kaigen::phantom::ModulationEngine& getModulationEngineB() noexcept { return modEngineB; }

    // ─── Engine focus (editor-state, not preset-state) ────────────────────
    // Aliases let the WebView native bindings (Task 2) and editor code
    // refer to PhantomProcessor::ActiveTab / ::EngineFocus without
    // pulling in the kaigen::phantom namespace at every call site.
    using ActiveTab   = kaigen::phantom::ActiveTab;
    using EngineFocus = kaigen::phantom::EngineFocus;

    EngineFocus getEngineFocus() const noexcept { return engineFocus; }
    void setEngineFocus(EngineFocus newFocus) noexcept;

    // Spectrum view mode — editor-state, persisted in plugin state.
    using SpectrumViewMode = kaigen::phantom::SpectrumViewMode;

    SpectrumViewMode getSpectrumViewMode() const noexcept { return spectrumViewMode; }
    void setSpectrumViewMode(SpectrumViewMode m) noexcept { spectrumViewMode = m; }

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

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
    // Ring-buffer mask for the per-engine FFT capture rings. Single source of
    // truth; used at both the producer (processBlock) and consumer
    // (computeEngineSpectrum) call sites.
    static constexpr int kEngineRingMask = (kFftSize * 2) - 1;
    juce::dsp::FFT spectrumFFT { kFftOrder };
    std::array<float, kFftSize * 2> fftBuffer {};       // input (pre-engine)
    std::array<float, kFftSize * 2> fftOutputBuffer {}; // output (post-engine)
    int fftWritePos       = 0;
    int fftOutputWritePos = 0;

    // Per-engine output FFT capture (split-mode spectrum view).
    // Same size as the existing input fftBuffer; populated from
    // dualEngineHost.getEngineAOutput()/getEngineBOutput() in processBlock
    // after dualEngineHost.process(...) returns. Read by the WebView
    // native binding (see Task 4) on the message thread, hence atomic
    // write positions for the producer-side ring buffer.
    std::array<float, kFftSize * 2> fftBufferEngineA {};
    std::array<float, kFftSize * 2> fftBufferEngineB {};
    std::atomic<int> fftWritePosEngineA { 0 };
    std::atomic<int> fftWritePosEngineB { 0 };

    // Scratch buffer used by computeEngineSpectrum() (UI/message thread).
    // Mutable because the method is logically const (it does not change
    // observable state — it only reads the ring buffer and writes to the
    // caller-provided destination), but the FFT in-place transform needs
    // writable storage.
    mutable std::array<float, kFftSize * 2> spectrumEngineScratch {};

    // Editor focus: which tab the UI is on + whether LINK is active.
    // Editor preference, not preset state — stored alongside APVTS in the
    // <PluginState> wrapper but outside of any preset.
    EngineFocus engineFocus;

    SpectrumViewMode spectrumViewMode { SpectrumViewMode::Split };

    // ─── Modulation engines (PR3a) ────────────────────────────────────────
    // Declared after `apvts` (public, above) so the references they hold
    // outlive only the APVTS — and after `dualEngineHost` so destruction
    // order is engines-first, mod engines second (mod engines don't depend
    // on dualEngineHost yet but Task 5 will wire DualEngineHost to read
    // these for value-lookup intercept).
    kaigen::phantom::ModulationEngine modEngineA { apvts, "a_" };
    kaigen::phantom::ModulationEngine modEngineB { apvts, "b_" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomProcessor)
};
