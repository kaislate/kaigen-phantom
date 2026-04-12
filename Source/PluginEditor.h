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

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);
    static juce::WebBrowserComponent::Options buildWebViewOptions(PhantomEditor&);

    PhantomProcessor& processor;

    // ── Slider relays (one per APVTS slider parameter) ────────────────
    juce::WebSliderRelay ghostRelay              { "ghost" };
    juce::WebSliderRelay phantomThresholdRelay   { "phantom_threshold" };
    juce::WebSliderRelay phantomStrengthRelay    { "phantom_strength" };
    juce::WebSliderRelay outputGainRelay         { "output_gain" };
    juce::WebSliderRelay recipeH2Relay           { "recipe_h2" };
    juce::WebSliderRelay recipeH3Relay           { "recipe_h3" };
    juce::WebSliderRelay recipeH4Relay           { "recipe_h4" };
    juce::WebSliderRelay recipeH5Relay           { "recipe_h5" };
    juce::WebSliderRelay recipeH6Relay           { "recipe_h6" };
    juce::WebSliderRelay recipeH7Relay           { "recipe_h7" };
    juce::WebSliderRelay recipeH8Relay           { "recipe_h8" };
    juce::WebSliderRelay harmonicSaturationRelay { "harmonic_saturation" };
    juce::WebSliderRelay synthStepRelay          { "synth_step" };
    juce::WebSliderRelay synthDutyRelay          { "synth_duty" };
    juce::WebSliderRelay synthSkipRelay          { "synth_skip" };
    juce::WebSliderRelay envAttackRelay          { "env_attack_ms" };
    juce::WebSliderRelay envReleaseRelay         { "env_release_ms" };
    juce::WebSliderRelay binauralWidthRelay      { "binaural_width" };
    juce::WebSliderRelay stereoWidthRelay        { "stereo_width" };
    juce::WebSliderRelay synthLPFRelay           { "synth_lpf_hz" };
    juce::WebSliderRelay synthHPFRelay           { "synth_hpf_hz" };

    // ── Combo-box relays ──────────────────────────────────────────────
    juce::WebComboBoxRelay modeRelay             { "mode" };
    juce::WebComboBoxRelay ghostModeRelay        { "ghost_mode" };
    juce::WebComboBoxRelay recipePresetRelay     { "recipe_preset" };
    juce::WebComboBoxRelay binauralModeRelay     { "binaural_mode" };

    // ── Toggle relay for bypass ───────────────────────────────────────
    juce::WebToggleButtonRelay bypassRelay       { "bypass" };

    // WebView — constructed in .cpp via buildWebViewOptions()
    SinglePageBrowser webView;

    // Attachments — constructed in .cpp after webView
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> comboAttachments;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment>          bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomEditor)
};
