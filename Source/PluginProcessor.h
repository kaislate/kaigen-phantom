#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "Engines/PhantomEngine.h"
#include "PresetManager.h"
#include "DualEngineHost.h"
#include "EngineFocus.h"
#include "SpectrumViewMode.h"
#include "MatrixViewState.h"
#include "EditorViewState.h"
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

    // Per-engine spectra, computed on the audio thread (mirrors the input/output
    // pipeline). The native binding reads these as atomic snapshots — no FFT,
    // no allocation on the message thread. memory_order_relaxed is sufficient:
    // spectrum visualization tolerates eventual consistency, and the publish
    // timestamp doesn't gate any other observable state.
    std::array<std::atomic<float>, kSpectrumBins> engineASpectrum {};
    std::array<std::atomic<float>, kSpectrumBins> engineBSpectrum {};

    // Oscilloscope ring buffers. Audio thread does relaxed atomic stores;
    // editor binding does relaxed atomic loads. Plain float[] would tear
    // only theoretically on x86/ARM, but std::atomic<float> with relaxed
    // ordering makes the data race well-defined at zero cost in codegen
    // (still a single mov on these platforms) — same pattern the spectrum
    // arrays already use above.
    static constexpr int kOscBufSize = PhantomEngine::kOscBufSize;
    std::array<std::atomic<float>, kOscBufSize> oscInputBuf  {};
    std::array<std::atomic<float>, kOscBufSize> oscOutputBuf {};
    std::atomic<int>                            oscInputWrPos  { 0 };
    std::atomic<int>                            oscOutputWrPos { 0 };

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

    /** Broadcaster fired (on the message thread) whenever the engine focus
     *  changes — editor widgets that need to retarget their per-engine
     *  APVTS attachments (PhantomKnob.setEnginePrefix etc.) subscribe here. */
    juce::ChangeBroadcaster& getEngineFocusBroadcaster() noexcept
        { return engineFocusBroadcaster; }

    // Spectrum view mode — editor-state, persisted in plugin state.
    using SpectrumViewMode = kaigen::phantom::SpectrumViewMode;

    SpectrumViewMode getSpectrumViewMode() const noexcept { return spectrumViewMode; }
    void setSpectrumViewMode(SpectrumViewMode m) noexcept { spectrumViewMode = m; }

    // Matrix view state — editor-state, persisted in plugin state.
    using MatrixViewState = kaigen::phantom::MatrixViewState;

    MatrixViewState getMatrixView() const                      { return matrixView; }
    void            setMatrixView(const MatrixViewState& s)    { matrixView = s; }

    // Editor view state — which editor (native vs WebView) the user prefers.
    // Editor-state, persisted in plugin state.
    using EditorViewState = kaigen::phantom::EditorViewState;

    EditorViewState getEditorView() const                      { return editorView; }
    void            setEditorView(const EditorViewState& s)    { editorView = s; }

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
    // truth; used at both ring-write (producer) and FFT-read (consumer)
    // sites — both running on the audio thread in processBlock.
    static constexpr int kEngineRingMask = (kFftSize * 2) - 1;
    juce::dsp::FFT spectrumFFT { kFftOrder };
    std::array<float, kFftSize * 2> fftBuffer {};       // input (pre-engine)
    std::array<float, kFftSize * 2> fftOutputBuffer {}; // output (post-engine)
    int fftWritePos       = 0;
    int fftOutputWritePos = 0;

    // Per-engine output FFT capture (split-mode spectrum view).
    // Same size as the existing input fftBuffer; populated from
    // dualEngineHost.getEngineAOutput()/getEngineBOutput() in processBlock
    // after dualEngineHost.process(...) returns, then transformed in-place
    // on the audio thread on the same kFftSize cadence as input/output.
    // Atomic write positions are kept for symmetry with the existing
    // pattern; only the audio thread mutates them.
    std::array<float, kFftSize * 2> fftBufferEngineA {};
    std::array<float, kFftSize * 2> fftBufferEngineB {};
    std::atomic<int> fftWritePosEngineA { 0 };
    std::atomic<int> fftWritePosEngineB { 0 };

    // Per-engine FFT scratch buffers (audio thread). Populated by copying the
    // most recent kFftSize samples from the per-engine ring buffer, then
    // Hann-windowed and FFT'd in place. Pre-allocated here so the audio
    // thread never heap-allocates.
    std::array<float, kFftSize * 2> fftScratchEngineA {};
    std::array<float, kFftSize * 2> fftScratchEngineB {};

    // Sub-rate cadence: count samples since last per-engine FFT and run when
    // we've accumulated kFftSize new samples. Matches the input/output
    // cadence (one FFT per kFftSize samples ≈ 5.86 Hz at 48k).
    int samplesSinceEngineFftA = 0;
    int samplesSinceEngineFftB = 0;

    // Editor focus: which tab the UI is on + whether LINK is active.
    // Editor preference, not preset state — stored alongside APVTS in the
    // <PluginState> wrapper but outside of any preset.
    EngineFocus engineFocus;
    juce::ChangeBroadcaster engineFocusBroadcaster;

    SpectrumViewMode spectrumViewMode { SpectrumViewMode::Split };

    // Matrix view UI state (mode + per-engine expanded categories).
    // Layout/UI concern only — routing data lives in <ModulationConfig>.
    kaigen::phantom::MatrixViewState matrixView;

    // Editor view state (native vs WebView preference).
    // Editor preference, not preset state — persisted alongside other view
    // states in the <PluginState> wrapper.
    kaigen::phantom::EditorViewState editorView;

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
