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

    // Relays — declared before webView (construction order)
    juce::WebSliderRelay  ghostRelay { "ghost" },
                          phantomThresholdRelay { "phantom_threshold" },
                          phantomStrengthRelay { "phantom_strength" },
                          outputGainRelay { "output_gain" },
                          recipeH2Relay { "recipe_h2" },  recipeH3Relay { "recipe_h3" },
                          recipeH4Relay { "recipe_h4" },  recipeH5Relay { "recipe_h5" },
                          recipeH6Relay { "recipe_h6" },  recipeH7Relay { "recipe_h7" },
                          recipeH8Relay { "recipe_h8" },
                          recipePhaseH2Relay { "recipe_phase_h2" }, recipePhaseH3Relay { "recipe_phase_h3" },
                          recipePhaseH4Relay { "recipe_phase_h4" }, recipePhaseH5Relay { "recipe_phase_h5" },
                          recipePhaseH6Relay { "recipe_phase_h6" }, recipePhaseH7Relay { "recipe_phase_h7" },
                          recipePhaseH8Relay { "recipe_phase_h8" },
                          recipeRotationRelay { "recipe_rotation" },
                          harmonicSaturationRelay { "harmonic_saturation" },
                          binauralWidthRelay { "binaural_width" },
                          trackingSensitivityRelay { "tracking_sensitivity" },
                          trackingGlideRelay { "tracking_glide" },
                          maxVoicesRelay { "max_voices" },
                          staggerDelayRelay { "stagger_delay" },
                          sidechainDuckAmountRelay { "sidechain_duck_amount" },
                          sidechainDuckAttackRelay { "sidechain_duck_attack" },
                          sidechainDuckReleaseRelay { "sidechain_duck_release" },
                          stereoWidthRelay { "stereo_width" };

    juce::WebComboBoxRelay modeRelay { "mode" },
                           ghostModeRelay { "ghost_mode" },
                           recipePresetRelay { "recipe_preset" },
                           deconflictionModeRelay { "deconfliction_mode" };

    // WebView — constructed in .cpp via buildWebViewOptions()
    SinglePageBrowser webView;

    // Attachments — constructed in .cpp after webView
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> comboAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhantomEditor)
};
