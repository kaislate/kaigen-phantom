#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Parameters.h"

struct SinglePageBrowser : juce::WebBrowserComponent
{
    using WebBrowserComponent::WebBrowserComponent;
    bool pageAboutToLoad(const juce::String& newURL) override
    {
        return newURL == getResourceProviderRoot();
    }
};

class PhantomEditor : public juce::AudioProcessorEditor
{
public:
    explicit PhantomEditor(PhantomProcessor&);
    ~PhantomEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool keyPressed(const juce::KeyPress&) override { return false; }
    bool keyStateChanged(bool) override { return false; }

#if JUCE_WINDOWS
    void parentHierarchyChanged() override;

    // Re-scans the WebView HWND tree on a timer so newly-created Chromium
    // child windows get the focus subclass installed.
    struct FocusRescanTimer : juce::Timer
    {
        PhantomEditor* owner = nullptr;
        void timerCallback() override;
    };
    FocusRescanTimer focusRescanTimer;
#endif

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);
    static juce::WebBrowserComponent::Options buildWebViewOptions(PhantomEditor&);

    PhantomProcessor& processor;

    // ── Slider relays ─────────────────────────────────────────────────
    // Per-engine relays are paired (a_*/b_*) so the JS dispatch layer can
    // bind whichever side matches the active engine tab. Globals (input_gain,
    // morph_amount) stay as single relays.
    juce::WebSliderRelay inputGainRelay              { "input_gain" };

    juce::WebSliderRelay ghostRelayA                 { "a_ghost" };
    juce::WebSliderRelay ghostRelayB                 { "b_ghost" };
    juce::WebSliderRelay phantomThresholdRelayA      { "a_phantom_threshold" };
    juce::WebSliderRelay phantomThresholdRelayB      { "b_phantom_threshold" };
    juce::WebSliderRelay phantomStrengthRelayA       { "a_phantom_strength" };
    juce::WebSliderRelay phantomStrengthRelayB       { "b_phantom_strength" };
    juce::WebSliderRelay outputGainRelayA            { "a_output_gain" };
    juce::WebSliderRelay outputGainRelayB            { "b_output_gain" };
    juce::WebSliderRelay recipeH2RelayA              { "a_recipe_h2" };
    juce::WebSliderRelay recipeH2RelayB              { "b_recipe_h2" };
    juce::WebSliderRelay recipeH3RelayA              { "a_recipe_h3" };
    juce::WebSliderRelay recipeH3RelayB              { "b_recipe_h3" };
    juce::WebSliderRelay recipeH4RelayA              { "a_recipe_h4" };
    juce::WebSliderRelay recipeH4RelayB              { "b_recipe_h4" };
    juce::WebSliderRelay recipeH5RelayA              { "a_recipe_h5" };
    juce::WebSliderRelay recipeH5RelayB              { "b_recipe_h5" };
    juce::WebSliderRelay recipeH6RelayA              { "a_recipe_h6" };
    juce::WebSliderRelay recipeH6RelayB              { "b_recipe_h6" };
    juce::WebSliderRelay recipeH7RelayA              { "a_recipe_h7" };
    juce::WebSliderRelay recipeH7RelayB              { "b_recipe_h7" };
    juce::WebSliderRelay recipeH8RelayA              { "a_recipe_h8" };
    juce::WebSliderRelay recipeH8RelayB              { "b_recipe_h8" };
    juce::WebSliderRelay harmonicSaturationRelayA    { "a_harmonic_saturation" };
    juce::WebSliderRelay harmonicSaturationRelayB    { "b_harmonic_saturation" };
    juce::WebSliderRelay synthStepRelayA             { "a_synth_step" };
    juce::WebSliderRelay synthStepRelayB             { "b_synth_step" };
    juce::WebSliderRelay synthDutyRelayA             { "a_synth_duty" };
    juce::WebSliderRelay synthDutyRelayB             { "b_synth_duty" };
    juce::WebSliderRelay synthSkipRelayA             { "a_synth_skip" };
    juce::WebSliderRelay synthSkipRelayB             { "b_synth_skip" };
    juce::WebSliderRelay envAttackRelayA             { "a_env_attack_ms" };
    juce::WebSliderRelay envAttackRelayB             { "b_env_attack_ms" };
    juce::WebSliderRelay envReleaseRelayA            { "a_env_release_ms" };
    juce::WebSliderRelay envReleaseRelayB            { "b_env_release_ms" };
    juce::WebSliderRelay binauralWidthRelayA         { "a_binaural_width" };
    juce::WebSliderRelay binauralWidthRelayB         { "b_binaural_width" };
    juce::WebSliderRelay stereoWidthRelayA           { "a_stereo_width" };
    juce::WebSliderRelay stereoWidthRelayB           { "b_stereo_width" };
    juce::WebSliderRelay synthLPFRelayA              { "a_synth_lpf_hz" };
    juce::WebSliderRelay synthLPFRelayB              { "b_synth_lpf_hz" };
    juce::WebSliderRelay synthHPFRelayA              { "a_synth_hpf_hz" };
    juce::WebSliderRelay synthHPFRelayB              { "b_synth_hpf_hz" };
    juce::WebSliderRelay synthWaveletLengthRelayA    { "a_synth_wavelet_length" };
    juce::WebSliderRelay synthWaveletLengthRelayB    { "b_synth_wavelet_length" };
    juce::WebSliderRelay synthGateThresholdRelayA    { "a_synth_gate_threshold" };
    juce::WebSliderRelay synthGateThresholdRelayB    { "b_synth_gate_threshold" };
    juce::WebSliderRelay synthH1RelayA               { "a_synth_h1" };
    juce::WebSliderRelay synthH1RelayB               { "b_synth_h1" };
    juce::WebSliderRelay synthSubRelayA              { "a_synth_sub" };
    juce::WebSliderRelay synthSubRelayB              { "b_synth_sub" };
    juce::WebSliderRelay synthMinSamplesRelayA       { "a_synth_min_samples" };
    juce::WebSliderRelay synthMinSamplesRelayB       { "b_synth_min_samples" };
    juce::WebSliderRelay synthMaxSamplesRelayA       { "a_synth_max_samples" };
    juce::WebSliderRelay synthMaxSamplesRelayB       { "b_synth_max_samples" };
    juce::WebSliderRelay trackingSpeedRelayA         { "a_tracking_speed" };
    juce::WebSliderRelay trackingSpeedRelayB         { "b_tracking_speed" };
    juce::WebSliderRelay punchAmountRelayA           { "a_punch_amount" };
    juce::WebSliderRelay punchAmountRelayB           { "b_punch_amount" };
    juce::WebSliderRelay synthBoostThresholdRelayA   { "a_synth_boost_threshold" };
    juce::WebSliderRelay synthBoostThresholdRelayB   { "b_synth_boost_threshold" };
    juce::WebSliderRelay synthBoostAmountRelayA      { "a_synth_boost_amount" };
    juce::WebSliderRelay synthBoostAmountRelayB      { "b_synth_boost_amount" };

    // Morph slider — global. Drives the audio crossfader between engines A and B.
    juce::WebSliderRelay morphAmountRelay            { "morph_amount" };

    // Macro modulators — global APVTS params (PR3a). Each macro is owned by
    // a ModulationEngine (A: macro1/2, B: macro3/4) but the param itself is
    // global so host automation works regardless of the active engine tab.
    juce::WebSliderRelay macro1Relay                 { "macro1" };
    juce::WebSliderRelay macro2Relay                 { "macro2" };
    juce::WebSliderRelay macro3Relay                 { "macro3" };
    juce::WebSliderRelay macro4Relay                 { "macro4" };

    // ── Combo-box relays ──────────────────────────────────────────────
    juce::WebComboBoxRelay modeRelayA                { "a_mode" };
    juce::WebComboBoxRelay modeRelayB                { "b_mode" };
    juce::WebComboBoxRelay ghostModeRelayA           { "a_ghost_mode" };
    juce::WebComboBoxRelay ghostModeRelayB           { "b_ghost_mode" };
    juce::WebComboBoxRelay recipePresetRelayA        { "a_recipe_preset" };
    juce::WebComboBoxRelay recipePresetRelayB        { "b_recipe_preset" };
    juce::WebComboBoxRelay binauralModeRelayA        { "a_binaural_mode" };
    juce::WebComboBoxRelay binauralModeRelayB        { "b_binaural_mode" };
    juce::WebComboBoxRelay filterSlopeRelayA         { "a_synth_filter_slope" };
    juce::WebComboBoxRelay filterSlopeRelayB         { "b_synth_filter_slope" };

    // ── Toggle relays ─────────────────────────────────────────────────
    // bypass + input_gain_auto are globals (single relay). Per-engine toggles
    // are paired so JS can bind whichever matches the active tab.
    juce::WebToggleButtonRelay bypassRelay              { "bypass" };
    juce::WebToggleButtonRelay inputGainAutoRelay       { "input_gain_auto" };

    juce::WebToggleButtonRelay punchEnabledRelayA       { "a_punch_enabled" };
    juce::WebToggleButtonRelay punchEnabledRelayB       { "b_punch_enabled" };
    juce::WebToggleButtonRelay midiTriggerRelayA        { "a_midi_trigger_enabled" };
    juce::WebToggleButtonRelay midiTriggerRelayB        { "b_midi_trigger_enabled" };
    juce::WebToggleButtonRelay midiGateReleaseRelayA    { "a_midi_gate_release" };
    juce::WebToggleButtonRelay midiGateReleaseRelayB    { "b_midi_gate_release" };

    // WebView — constructed in .cpp via buildWebViewOptions()
    SinglePageBrowser webView;

    // Attachments — constructed in .cpp after webView. Per-engine attachments
    // come in pairs (one bound to A_*, one to B_*); globals stay single.
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> comboAttachments;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment>          bypassAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment>          inputGainAutoAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment>          punchEnabledAttachmentA;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment>          punchEnabledAttachmentB;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment>          midiTriggerAttachmentA;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment>          midiTriggerAttachmentB;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment>          midiGateReleaseAttachmentA;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment>          midiGateReleaseAttachmentB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomEditor)
};
