#include "PluginEditor.h"
#include "BinaryData.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

static const char* getMimeForExtension(const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { "html", "text/html" },  { "htm",  "text/html" },
        { "css",  "text/css" },   { "js",   "text/javascript" },
        { "json", "application/json" },
        { "png",  "image/png" },  { "jpg",  "image/jpeg" },
        { "svg",  "image/svg+xml" }, { "woff2","font/woff2" },
    };
    if (const auto it = mimeMap.find(extension.toLowerCase()); it != mimeMap.end())
        return it->second;
    return "application/octet-stream";
}

// Build the Options chain for the WebView — called once during construction
juce::WebBrowserComponent::Options PhantomEditor::buildWebViewOptions(PhantomEditor& self)
{
   #if JUCE_WINDOWS
    // Enable WebView2 remote debugging on port 9222 — must be set before
    // the first WebView2 environment is created in this process
    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
                             L"--remote-debugging-port=9222 --remote-allow-origins=*");
   #endif

    auto options = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)))
        .withNativeIntegrationEnabled();

    // Register all slider relays
    juce::WebSliderRelay* sliderRelays[] = {
        &self.ghostRelay, &self.phantomThresholdRelay, &self.phantomStrengthRelay,
        &self.outputGainRelay,
        &self.recipeH2Relay, &self.recipeH3Relay, &self.recipeH4Relay,
        &self.recipeH5Relay, &self.recipeH6Relay, &self.recipeH7Relay, &self.recipeH8Relay,
        &self.recipePhaseH2Relay, &self.recipePhaseH3Relay, &self.recipePhaseH4Relay,
        &self.recipePhaseH5Relay, &self.recipePhaseH6Relay, &self.recipePhaseH7Relay,
        &self.recipePhaseH8Relay,
        &self.recipeRotationRelay, &self.harmonicSaturationRelay,
        &self.binauralWidthRelay,
        &self.trackingSensitivityRelay, &self.trackingGlideRelay,
        &self.maxVoicesRelay, &self.staggerDelayRelay,
        &self.sidechainDuckAmountRelay, &self.sidechainDuckAttackRelay,
        &self.sidechainDuckReleaseRelay, &self.stereoWidthRelay
    };
    for (auto* r : sliderRelays)
        options = options.withOptionsFrom(*r);

    // Register combo relays
    juce::WebComboBoxRelay* comboRelays[] = {
        &self.modeRelay, &self.ghostModeRelay,
        &self.recipePresetRelay, &self.deconflictionModeRelay,
        &self.binauralModeRelay
    };
    for (auto* r : comboRelays)
        options = options.withOptionsFrom(*r);

    // Register bypass toggle relay
    options = options.withOptionsFrom(self.bypassRelay);

    // Native functions for real-time data
    options = options
        .withNativeFunction("getSpectrumData",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::Array<juce::var> bins;
                // Always return latest spectrum data (don't consume ready flag)
                for (int i = 0; i < PhantomProcessor::kSpectrumBins; ++i)
                    bins.add(self.processor.spectrumData[(size_t)i]);
                complete(bins);
            })
        .withNativeFunction("getDiagnostics",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty("processBlocks",    self.processor.processBlockCount.load(std::memory_order_relaxed));
                obj->setProperty("fftRuns",          self.processor.fftRunCount.load(std::memory_order_relaxed));
                obj->setProperty("fftMaxMagnitude",  (double)self.processor.fftMaxMagnitude.load(std::memory_order_relaxed));
                obj->setProperty("currentPitch",     (double)self.processor.currentPitch.load(std::memory_order_relaxed));
                // First 5 spectrumData values
                juce::Array<juce::var> first5;
                for (int i = 0; i < 5; ++i)
                    first5.add((double)self.processor.spectrumData[(size_t)i]);
                obj->setProperty("spectrum0_4", first5);
                complete(juce::var(obj));
            })
        .withNativeFunction("getPeakLevels",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty("inL",  (double)self.processor.peakInL.load(std::memory_order_relaxed));
                obj->setProperty("inR",  (double)self.processor.peakInR.load(std::memory_order_relaxed));
                obj->setProperty("outL", (double)self.processor.peakOutL.load(std::memory_order_relaxed));
                obj->setProperty("outR", (double)self.processor.peakOutR.load(std::memory_order_relaxed));
                complete(juce::var(obj));
            })
        .withNativeFunction("getPitchInfo",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto* obj = new juce::DynamicObject();
                float hz = self.processor.currentPitch.load(std::memory_order_relaxed);
                obj->setProperty("hz", (double)hz);
                if (hz > 0.0f) {
                    static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                    int midi = juce::roundToInt(12.0f * std::log2(hz / 440.0f) + 69.0f);
                    int note = ((midi % 12) + 12) % 12;
                    int octave = (midi / 12) - 1;
                    obj->setProperty("note", juce::String(noteNames[note]) + juce::String(octave));
                } else {
                    obj->setProperty("note", "---");
                }
                int presetIdx = (int)self.processor.apvts.getRawParameterValue(ParamID::RECIPE_PRESET)->load();
                static const char* presetNames[] = {"Warm","Aggressive","Hollow","Dense","Custom"};
                obj->setProperty("preset", juce::String(presetNames[juce::jlimit(0, 4, presetIdx)]));
                complete(juce::var(obj));
            })
        .withResourceProvider([&self](const auto& url) { return self.getResource(url); });

    return options;
}

PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      webView(buildWebViewOptions(*this))
{
    setSize(920, 620);
    addAndMakeVisible(webView);

    // Defer URL load until after construction completes — fixes VST3 white screen
    juce::MessageManager::callAsync([this]()
    {
        webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    });

    // Create parameter attachments
    struct SliderBinding { const char* paramId; juce::WebSliderRelay& relay; };
    SliderBinding sliderBindings[] = {
        { ParamID::GHOST,                  ghostRelay },
        { ParamID::PHANTOM_THRESHOLD,      phantomThresholdRelay },
        { ParamID::PHANTOM_STRENGTH,       phantomStrengthRelay },
        { ParamID::OUTPUT_GAIN,            outputGainRelay },
        { ParamID::RECIPE_H2,              recipeH2Relay },
        { ParamID::RECIPE_H3,              recipeH3Relay },
        { ParamID::RECIPE_H4,              recipeH4Relay },
        { ParamID::RECIPE_H5,              recipeH5Relay },
        { ParamID::RECIPE_H6,              recipeH6Relay },
        { ParamID::RECIPE_H7,              recipeH7Relay },
        { ParamID::RECIPE_H8,              recipeH8Relay },
        { ParamID::RECIPE_PHASE_H2,        recipePhaseH2Relay },
        { ParamID::RECIPE_PHASE_H3,        recipePhaseH3Relay },
        { ParamID::RECIPE_PHASE_H4,        recipePhaseH4Relay },
        { ParamID::RECIPE_PHASE_H5,        recipePhaseH5Relay },
        { ParamID::RECIPE_PHASE_H6,        recipePhaseH6Relay },
        { ParamID::RECIPE_PHASE_H7,        recipePhaseH7Relay },
        { ParamID::RECIPE_PHASE_H8,        recipePhaseH8Relay },
        { ParamID::RECIPE_ROTATION,        recipeRotationRelay },
        { ParamID::HARMONIC_SATURATION,    harmonicSaturationRelay },
        { ParamID::BINAURAL_WIDTH,         binauralWidthRelay },
        { ParamID::TRACKING_SENSITIVITY,   trackingSensitivityRelay },
        { ParamID::TRACKING_GLIDE,         trackingGlideRelay },
        { ParamID::MAX_VOICES,             maxVoicesRelay },
        { ParamID::STAGGER_DELAY,          staggerDelayRelay },
        { ParamID::SIDECHAIN_DUCK_AMOUNT,  sidechainDuckAmountRelay },
        { ParamID::SIDECHAIN_DUCK_ATTACK,  sidechainDuckAttackRelay },
        { ParamID::SIDECHAIN_DUCK_RELEASE, sidechainDuckReleaseRelay },
        { ParamID::STEREO_WIDTH,           stereoWidthRelay },
    };
    for (auto& b : sliderBindings)
        sliderAttachments.push_back(std::make_unique<juce::WebSliderParameterAttachment>(
            *processor.apvts.getParameter(b.paramId), b.relay, nullptr));

    struct ComboBinding { const char* paramId; juce::WebComboBoxRelay& relay; };
    ComboBinding comboBindings[] = {
        { ParamID::MODE,               modeRelay },
        { ParamID::GHOST_MODE,         ghostModeRelay },
        { ParamID::RECIPE_PRESET,      recipePresetRelay },
        { ParamID::DECONFLICTION_MODE, deconflictionModeRelay },
        { ParamID::BINAURAL_MODE,      binauralModeRelay },
    };
    for (auto& b : comboBindings)
        comboAttachments.push_back(std::make_unique<juce::WebComboBoxParameterAttachment>(
            *processor.apvts.getParameter(b.paramId), b.relay, nullptr));

    // Bypass toggle attachment
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::BYPASS), bypassRelay, nullptr);
}

PhantomEditor::~PhantomEditor() = default;

void PhantomEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff06060c));
}

void PhantomEditor::resized()
{
    webView.setBounds(getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource> PhantomEditor::getResource(const juce::String& url)
{
    const auto urlToRetrieve = url == "/" ? juce::String{ "index.html" }
                                          : url.fromFirstOccurrenceOf("/", false, false);

    // JUCE BinaryData naming: dots -> underscores, dashes STRIPPED (not replaced)
    auto resourceName = urlToRetrieve.replace(".", "_").replace("-", "").replace("/", "_");

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (juce::String(BinaryData::namedResourceList[i]) == resourceName)
        {
            int size = 0;
            const auto* data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], size);
            if (data != nullptr && size > 0)
            {
                auto extension = urlToRetrieve.fromLastOccurrenceOf(".", false, false);
                std::vector<std::byte> bytes(reinterpret_cast<const std::byte*>(data),
                                              reinterpret_cast<const std::byte*>(data) + size);
                return juce::WebBrowserComponent::Resource{ std::move(bytes),
                                                             juce::String(getMimeForExtension(extension)) };
            }
        }
    }
    return std::nullopt;
}
