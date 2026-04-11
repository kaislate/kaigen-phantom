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
    ~PhantomEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    PhantomProcessor& processor;

    // ── Slider relays (must be declared before webView) ──────────────────
    juce::WebSliderRelay ghostRelay                  { "ghost" };
    juce::WebSliderRelay phantomThresholdRelay       { "phantom_threshold" };
    juce::WebSliderRelay phantomStrengthRelay        { "phantom_strength" };
    juce::WebSliderRelay outputGainRelay             { "output_gain" };
    juce::WebSliderRelay recipeH2Relay               { "recipe_h2" };
    juce::WebSliderRelay recipeH3Relay               { "recipe_h3" };
    juce::WebSliderRelay recipeH4Relay               { "recipe_h4" };
    juce::WebSliderRelay recipeH5Relay               { "recipe_h5" };
    juce::WebSliderRelay recipeH6Relay               { "recipe_h6" };
    juce::WebSliderRelay recipeH7Relay               { "recipe_h7" };
    juce::WebSliderRelay recipeH8Relay               { "recipe_h8" };
    juce::WebSliderRelay recipePhaseH2Relay          { "recipe_phase_h2" };
    juce::WebSliderRelay recipePhaseH3Relay          { "recipe_phase_h3" };
    juce::WebSliderRelay recipePhaseH4Relay          { "recipe_phase_h4" };
    juce::WebSliderRelay recipePhaseH5Relay          { "recipe_phase_h5" };
    juce::WebSliderRelay recipePhaseH6Relay          { "recipe_phase_h6" };
    juce::WebSliderRelay recipePhaseH7Relay          { "recipe_phase_h7" };
    juce::WebSliderRelay recipePhaseH8Relay          { "recipe_phase_h8" };
    juce::WebSliderRelay recipeRotationRelay         { "recipe_rotation" };
    juce::WebSliderRelay harmonicSaturationRelay     { "harmonic_saturation" };
    juce::WebSliderRelay binauralWidthRelay          { "binaural_width" };
    juce::WebSliderRelay trackingSensitivityRelay    { "tracking_sensitivity" };
    juce::WebSliderRelay trackingGlideRelay          { "tracking_glide" };
    juce::WebSliderRelay maxVoicesRelay              { "max_voices" };
    juce::WebSliderRelay staggerDelayRelay           { "stagger_delay" };
    juce::WebSliderRelay sidechainDuckAmountRelay    { "sidechain_duck_amount" };
    juce::WebSliderRelay sidechainDuckAttackRelay    { "sidechain_duck_attack" };
    juce::WebSliderRelay sidechainDuckReleaseRelay   { "sidechain_duck_release" };
    juce::WebSliderRelay stereoWidthRelay            { "stereo_width" };

    // ── Combo-box relays (must be declared before webView) ───────────────
    juce::WebComboBoxRelay modeRelay                 { "mode" };
    juce::WebComboBoxRelay ghostModeRelay            { "ghost_mode" };
    juce::WebComboBoxRelay recipePresetRelay         { "recipe_preset" };
    juce::WebComboBoxRelay deconflictionModeRelay    { "deconfliction_mode" };

    // ── WebView (relays must be registered before construction) ──────────
    SinglePageBrowser webView {
        juce::WebBrowserComponent::Options{}
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options(
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder(
                        juce::File::getSpecialLocation(juce::File::tempDirectory)))
            .withNativeIntegrationEnabled()
            // Slider relays
            .withOptionsFrom(ghostRelay)
            .withOptionsFrom(phantomThresholdRelay)
            .withOptionsFrom(phantomStrengthRelay)
            .withOptionsFrom(outputGainRelay)
            .withOptionsFrom(recipeH2Relay)
            .withOptionsFrom(recipeH3Relay)
            .withOptionsFrom(recipeH4Relay)
            .withOptionsFrom(recipeH5Relay)
            .withOptionsFrom(recipeH6Relay)
            .withOptionsFrom(recipeH7Relay)
            .withOptionsFrom(recipeH8Relay)
            .withOptionsFrom(recipePhaseH2Relay)
            .withOptionsFrom(recipePhaseH3Relay)
            .withOptionsFrom(recipePhaseH4Relay)
            .withOptionsFrom(recipePhaseH5Relay)
            .withOptionsFrom(recipePhaseH6Relay)
            .withOptionsFrom(recipePhaseH7Relay)
            .withOptionsFrom(recipePhaseH8Relay)
            .withOptionsFrom(recipeRotationRelay)
            .withOptionsFrom(harmonicSaturationRelay)
            .withOptionsFrom(binauralWidthRelay)
            .withOptionsFrom(trackingSensitivityRelay)
            .withOptionsFrom(trackingGlideRelay)
            .withOptionsFrom(maxVoicesRelay)
            .withOptionsFrom(staggerDelayRelay)
            .withOptionsFrom(sidechainDuckAmountRelay)
            .withOptionsFrom(sidechainDuckAttackRelay)
            .withOptionsFrom(sidechainDuckReleaseRelay)
            .withOptionsFrom(stereoWidthRelay)
            // Combo-box relays
            .withOptionsFrom(modeRelay)
            .withOptionsFrom(ghostModeRelay)
            .withOptionsFrom(recipePresetRelay)
            .withOptionsFrom(deconflictionModeRelay)
            // Native functions for real-time data
            .withNativeFunction("getSpectrumData",
                [this](const juce::Array<juce::var>&,
                       juce::WebBrowserComponent::NativeFunctionCompletion complete)
                {
                    juce::Array<juce::var> bins;
                    if (processor.spectrumReady.exchange(false))
                        for (int i = 0; i < PhantomProcessor::kSpectrumBins; ++i)
                            bins.add(processor.spectrumData[(size_t)i]);
                    else
                        for (int i = 0; i < PhantomProcessor::kSpectrumBins; ++i)
                            bins.add(0.0f);
                    complete(bins);
                })
            .withNativeFunction("getPeakLevels",
                [this](const juce::Array<juce::var>&,
                       juce::WebBrowserComponent::NativeFunctionCompletion complete)
                {
                    auto* obj = new juce::DynamicObject();
                    obj->setProperty("inL",  (double)processor.peakInL.load(std::memory_order_relaxed));
                    obj->setProperty("inR",  (double)processor.peakInR.load(std::memory_order_relaxed));
                    obj->setProperty("outL", (double)processor.peakOutL.load(std::memory_order_relaxed));
                    obj->setProperty("outR", (double)processor.peakOutR.load(std::memory_order_relaxed));
                    complete(juce::var(obj));
                })
            .withNativeFunction("getPitchInfo",
                [this](const juce::Array<juce::var>&,
                       juce::WebBrowserComponent::NativeFunctionCompletion complete)
                {
                    auto* obj = new juce::DynamicObject();
                    float hz = processor.currentPitch.load(std::memory_order_relaxed);
                    obj->setProperty("hz", (double)hz);
                    if (hz > 0.0f)
                    {
                        static const char* noteNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
                        int midi   = juce::roundToInt(12.0f * std::log2(hz / 440.0f) + 69.0f);
                        int note   = ((midi % 12) + 12) % 12;
                        int octave = (midi / 12) - 1;
                        obj->setProperty("note", juce::String(noteNames[note]) + juce::String(octave));
                    }
                    else
                    {
                        obj->setProperty("note", "---");
                    }
                    int presetIdx = (int)processor.apvts.getRawParameterValue(ParamID::RECIPE_PRESET)->load();
                    static const char* presetNames[] = { "Warm","Aggressive","Hollow","Dense","Custom" };
                    obj->setProperty("preset", juce::String(presetNames[juce::jlimit(0, 4, presetIdx)]));
                    complete(juce::var(obj));
                })
            .withResourceProvider([this](const auto& url) { return getResource(url); })
    };

    // ── Parameter attachments (must be declared after webView) ───────────
    juce::WebSliderParameterAttachment ghostAttach
        { *processor.apvts.getParameter(ParamID::GHOST),               ghostRelay,               nullptr };
    juce::WebSliderParameterAttachment phantomThresholdAttach
        { *processor.apvts.getParameter(ParamID::PHANTOM_THRESHOLD),   phantomThresholdRelay,    nullptr };
    juce::WebSliderParameterAttachment phantomStrengthAttach
        { *processor.apvts.getParameter(ParamID::PHANTOM_STRENGTH),    phantomStrengthRelay,     nullptr };
    juce::WebSliderParameterAttachment outputGainAttach
        { *processor.apvts.getParameter(ParamID::OUTPUT_GAIN),         outputGainRelay,          nullptr };
    juce::WebSliderParameterAttachment recipeH2Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_H2),           recipeH2Relay,            nullptr };
    juce::WebSliderParameterAttachment recipeH3Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_H3),           recipeH3Relay,            nullptr };
    juce::WebSliderParameterAttachment recipeH4Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_H4),           recipeH4Relay,            nullptr };
    juce::WebSliderParameterAttachment recipeH5Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_H5),           recipeH5Relay,            nullptr };
    juce::WebSliderParameterAttachment recipeH6Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_H6),           recipeH6Relay,            nullptr };
    juce::WebSliderParameterAttachment recipeH7Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_H7),           recipeH7Relay,            nullptr };
    juce::WebSliderParameterAttachment recipeH8Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_H8),           recipeH8Relay,            nullptr };
    juce::WebSliderParameterAttachment recipePhaseH2Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_PHASE_H2),     recipePhaseH2Relay,       nullptr };
    juce::WebSliderParameterAttachment recipePhaseH3Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_PHASE_H3),     recipePhaseH3Relay,       nullptr };
    juce::WebSliderParameterAttachment recipePhaseH4Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_PHASE_H4),     recipePhaseH4Relay,       nullptr };
    juce::WebSliderParameterAttachment recipePhaseH5Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_PHASE_H5),     recipePhaseH5Relay,       nullptr };
    juce::WebSliderParameterAttachment recipePhaseH6Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_PHASE_H6),     recipePhaseH6Relay,       nullptr };
    juce::WebSliderParameterAttachment recipePhaseH7Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_PHASE_H7),     recipePhaseH7Relay,       nullptr };
    juce::WebSliderParameterAttachment recipePhaseH8Attach
        { *processor.apvts.getParameter(ParamID::RECIPE_PHASE_H8),     recipePhaseH8Relay,       nullptr };
    juce::WebSliderParameterAttachment recipeRotationAttach
        { *processor.apvts.getParameter(ParamID::RECIPE_ROTATION),     recipeRotationRelay,      nullptr };
    juce::WebSliderParameterAttachment harmonicSaturationAttach
        { *processor.apvts.getParameter(ParamID::HARMONIC_SATURATION), harmonicSaturationRelay,  nullptr };
    juce::WebSliderParameterAttachment binauralWidthAttach
        { *processor.apvts.getParameter(ParamID::BINAURAL_WIDTH),      binauralWidthRelay,       nullptr };
    juce::WebSliderParameterAttachment trackingSensitivityAttach
        { *processor.apvts.getParameter(ParamID::TRACKING_SENSITIVITY),trackingSensitivityRelay, nullptr };
    juce::WebSliderParameterAttachment trackingGlideAttach
        { *processor.apvts.getParameter(ParamID::TRACKING_GLIDE),      trackingGlideRelay,       nullptr };
    juce::WebSliderParameterAttachment maxVoicesAttach
        { *processor.apvts.getParameter(ParamID::MAX_VOICES),          maxVoicesRelay,           nullptr };
    juce::WebSliderParameterAttachment staggerDelayAttach
        { *processor.apvts.getParameter(ParamID::STAGGER_DELAY),       staggerDelayRelay,        nullptr };
    juce::WebSliderParameterAttachment sidechainDuckAmountAttach
        { *processor.apvts.getParameter(ParamID::SIDECHAIN_DUCK_AMOUNT),  sidechainDuckAmountRelay,   nullptr };
    juce::WebSliderParameterAttachment sidechainDuckAttackAttach
        { *processor.apvts.getParameter(ParamID::SIDECHAIN_DUCK_ATTACK),  sidechainDuckAttackRelay,   nullptr };
    juce::WebSliderParameterAttachment sidechainDuckReleaseAttach
        { *processor.apvts.getParameter(ParamID::SIDECHAIN_DUCK_RELEASE), sidechainDuckReleaseRelay,  nullptr };
    juce::WebSliderParameterAttachment stereoWidthAttach
        { *processor.apvts.getParameter(ParamID::STEREO_WIDTH),        stereoWidthRelay,         nullptr };

    juce::WebComboBoxParameterAttachment modeAttach
        { *processor.apvts.getParameter(ParamID::MODE),               modeRelay,               nullptr };
    juce::WebComboBoxParameterAttachment ghostModeAttach
        { *processor.apvts.getParameter(ParamID::GHOST_MODE),         ghostModeRelay,          nullptr };
    juce::WebComboBoxParameterAttachment recipePresetAttach
        { *processor.apvts.getParameter(ParamID::RECIPE_PRESET),      recipePresetRelay,       nullptr };
    juce::WebComboBoxParameterAttachment deconflictionModeAttach
        { *processor.apvts.getParameter(ParamID::DECONFLICTION_MODE), deconflictionModeRelay,  nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomEditor)
};
