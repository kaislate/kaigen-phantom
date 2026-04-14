#include "PluginEditor.h"
#include "BinaryData.h"

#if JUCE_WINDOWS
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
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

#if JUCE_WINDOWS
namespace {

constexpr UINT_PTR kFocusRedirectId = 0x4B4750; // 'KGP'

LRESULT CALLBACK webViewFocusSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR id, DWORD_PTR)
{
    if (msg == WM_SETFOCUS)
    {
        // Redirect focus to the parent HWND so the DAW keeps keyboard MIDI
        if (HWND parent = GetParent(hwnd))
            SetFocus(parent);
        return 0;
    }
    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(hwnd, webViewFocusSubclass, id);
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

BOOL CALLBACK installOnChromeWindows(HWND hwnd, LPARAM)
{
    wchar_t cls[256] = {};
    GetClassNameW(hwnd, cls, 255);
    if (wcsncmp(cls, L"Chrome_WidgetWin", 16) == 0)
        SetWindowSubclass(hwnd, webViewFocusSubclass, kFocusRedirectId, 0);
    EnumChildWindows(hwnd, installOnChromeWindows, 0);
    return TRUE;
}

} // namespace

void PhantomEditor::parentHierarchyChanged()
{
    // WebView2 creates its Chrome_WidgetWin HWNDs asynchronously, so we retry
    // at increasing intervals until they exist.
    auto tryInstall = [this]()
    {
        if (auto* peer = getPeer())
            installOnChromeWindows((HWND) peer->getNativeHandle(), 0);
    };

    for (int delayMs : { 50, 200, 500, 1000, 2000 })
        juce::Timer::callAfterDelay(delayMs, tryInstall);
}
#endif

juce::WebBrowserComponent::Options PhantomEditor::buildWebViewOptions(PhantomEditor& self)
{
   #if JUCE_WINDOWS
    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
                             L"--remote-debugging-port=9222 --remote-allow-origins=*");
   #endif

    auto options = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)))
        .withNativeIntegrationEnabled();

    // ── Slider relays ─────────────────────────────────────────────────
    juce::WebSliderRelay* sliderRelays[] = {
        &self.inputGainRelay,
        &self.ghostRelay, &self.phantomThresholdRelay, &self.phantomStrengthRelay,
        &self.outputGainRelay,
        &self.recipeH2Relay, &self.recipeH3Relay, &self.recipeH4Relay,
        &self.recipeH5Relay, &self.recipeH6Relay, &self.recipeH7Relay, &self.recipeH8Relay,
        &self.harmonicSaturationRelay,
        &self.synthStepRelay, &self.synthDutyRelay, &self.synthSkipRelay,
        &self.envAttackRelay, &self.envReleaseRelay,
        &self.binauralWidthRelay, &self.stereoWidthRelay,
        &self.synthLPFRelay, &self.synthHPFRelay,
        &self.synthWaveletLengthRelay, &self.synthGateThresholdRelay,
        &self.synthH1Relay, &self.synthSubRelay, &self.synthMinSamplesRelay, &self.synthMaxSamplesRelay, &self.trackingSpeedRelay,
        &self.punchAmountRelay,
        &self.synthBoostThresholdRelay, &self.synthBoostAmountRelay
    };
    for (auto* r : sliderRelays)
        options = options.withOptionsFrom(*r);

    // ── Combo-box relays ──────────────────────────────────────────────
    juce::WebComboBoxRelay* comboRelays[] = {
        &self.modeRelay, &self.ghostModeRelay,
        &self.recipePresetRelay, &self.binauralModeRelay
    };
    for (auto* r : comboRelays)
        options = options.withOptionsFrom(*r);

    // ── Toggle relays ────────────────────────────────────────────────
    options = options.withOptionsFrom(self.bypassRelay);
    options = options.withOptionsFrom(self.punchEnabledRelay);

    // ── Native functions for real-time data ──────────────────────────
    options = options
        .withNativeFunction("getSpectrumData",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::Array<juce::var> inputBins, outputBins;
                for (int i = 0; i < PhantomProcessor::kSpectrumBins; ++i)
                {
                    inputBins .add(self.processor.spectrumData      [(size_t) i]);
                    outputBins.add(self.processor.spectrumOutputData[(size_t) i]);
                }
                auto* obj = new juce::DynamicObject();
                obj->setProperty("input",  inputBins);
                obj->setProperty("output", outputBins);
                complete(juce::var(obj));
            })
        .withNativeFunction("getPeakLevels",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty("inL",  (double) self.processor.peakInL .load(std::memory_order_relaxed));
                obj->setProperty("inR",  (double) self.processor.peakInR .load(std::memory_order_relaxed));
                obj->setProperty("outL", (double) self.processor.peakOutL.load(std::memory_order_relaxed));
                obj->setProperty("outR", (double) self.processor.peakOutR.load(std::memory_order_relaxed));
                complete(juce::var(obj));
            })
        .withNativeFunction("getPitchInfo",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto* obj = new juce::DynamicObject();
                const float hz = self.processor.currentPitch.load(std::memory_order_relaxed);
                obj->setProperty("hz", (double) hz);
                if (hz > 0.0f)
                {
                    static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                    const int midi = juce::roundToInt(12.0f * std::log2(hz / 440.0f) + 69.0f);
                    const int note = ((midi % 12) + 12) % 12;
                    const int octave = (midi / 12) - 1;
                    obj->setProperty("note", juce::String(noteNames[note]) + juce::String(octave));
                }
                else
                {
                    obj->setProperty("note", "---");
                }
                const int presetIdx = (int) self.processor.apvts.getRawParameterValue(ParamID::RECIPE_PRESET)->load();
                static const char* presetNames[] = { "Warm","Aggressive","Hollow","Dense","Stable","Weird","Custom" };
                obj->setProperty("preset", juce::String(presetNames[juce::jlimit(0, 6, presetIdx)]));
                complete(juce::var(obj));
            })
        .withNativeFunction("getOscilloscopeData",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::Array<juce::var> inArr, synthArr, outArr;
                for (int i = 0; i < PhantomEngine::kOscBufSize; ++i)
                {
                    inArr  .add((double) self.processor.oscInputBuf [(size_t) i]);
                    synthArr.add((double) self.processor.engine.oscSynthBuf[(size_t) i]);
                    outArr .add((double) self.processor.oscOutputBuf[(size_t) i]);
                }
                auto* obj = new juce::DynamicObject();
                obj->setProperty("input",       inArr);
                obj->setProperty("synth",       synthArr);
                obj->setProperty("output",      outArr);
                obj->setProperty("inputWrPos",  (int) self.processor.oscInputWrPos .load(std::memory_order_relaxed));
                obj->setProperty("synthWrPos",  (int) self.processor.engine.oscSynthWrPos.load(std::memory_order_relaxed));
                obj->setProperty("outputWrPos", (int) self.processor.oscOutputWrPos.load(std::memory_order_relaxed));
                obj->setProperty("sampleRate",   (double) self.processor.getSampleRate());
                obj->setProperty("synthPeak",    (double) self.processor.engine.getSynthInputPeak());
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
    setWantsKeyboardFocus(false);
    setSize(1600, 820);
    addAndMakeVisible(webView);

    juce::MessageManager::callAsync([this]()
    {
        webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    });

    // ── Slider attachments ────────────────────────────────────────────
    struct SliderBinding { const char* paramId; juce::WebSliderRelay& relay; };
    SliderBinding sliderBindings[] = {
        { ParamID::INPUT_GAIN,          inputGainRelay },
        { ParamID::GHOST,               ghostRelay },
        { ParamID::PHANTOM_THRESHOLD,   phantomThresholdRelay },
        { ParamID::PHANTOM_STRENGTH,    phantomStrengthRelay },
        { ParamID::OUTPUT_GAIN,         outputGainRelay },
        { ParamID::RECIPE_H2,           recipeH2Relay },
        { ParamID::RECIPE_H3,           recipeH3Relay },
        { ParamID::RECIPE_H4,           recipeH4Relay },
        { ParamID::RECIPE_H5,           recipeH5Relay },
        { ParamID::RECIPE_H6,           recipeH6Relay },
        { ParamID::RECIPE_H7,           recipeH7Relay },
        { ParamID::RECIPE_H8,           recipeH8Relay },
        { ParamID::HARMONIC_SATURATION, harmonicSaturationRelay },
        { ParamID::SYNTH_STEP,          synthStepRelay },
        { ParamID::SYNTH_DUTY,          synthDutyRelay },
        { ParamID::SYNTH_SKIP,          synthSkipRelay },
        { ParamID::ENV_ATTACK_MS,       envAttackRelay },
        { ParamID::ENV_RELEASE_MS,      envReleaseRelay },
        { ParamID::BINAURAL_WIDTH,      binauralWidthRelay },
        { ParamID::STEREO_WIDTH,        stereoWidthRelay },
        { ParamID::SYNTH_LPF_HZ,            synthLPFRelay },
        { ParamID::SYNTH_HPF_HZ,            synthHPFRelay },
        { ParamID::SYNTH_WAVELET_LENGTH,    synthWaveletLengthRelay },
        { ParamID::SYNTH_GATE_THRESHOLD,    synthGateThresholdRelay },
        { ParamID::SYNTH_H1,                synthH1Relay },
        { ParamID::SYNTH_SUB,               synthSubRelay },
        { ParamID::SYNTH_MIN_SAMPLES,       synthMinSamplesRelay },
        { ParamID::SYNTH_MAX_SAMPLES,       synthMaxSamplesRelay },
        { ParamID::TRACKING_SPEED,          trackingSpeedRelay },
        { ParamID::PUNCH_AMOUNT,            punchAmountRelay },
        { ParamID::SYNTH_BOOST_THRESHOLD,   synthBoostThresholdRelay },
        { ParamID::SYNTH_BOOST_AMOUNT,      synthBoostAmountRelay },
    };
    for (auto& b : sliderBindings)
        sliderAttachments.push_back(std::make_unique<juce::WebSliderParameterAttachment>(
            *processor.apvts.getParameter(b.paramId), b.relay, nullptr));

    // ── Combo attachments ─────────────────────────────────────────────
    struct ComboBinding { const char* paramId; juce::WebComboBoxRelay& relay; };
    ComboBinding comboBindings[] = {
        { ParamID::MODE,          modeRelay },
        { ParamID::GHOST_MODE,    ghostModeRelay },
        { ParamID::RECIPE_PRESET, recipePresetRelay },
        { ParamID::BINAURAL_MODE, binauralModeRelay },
    };
    for (auto& b : comboBindings)
        comboAttachments.push_back(std::make_unique<juce::WebComboBoxParameterAttachment>(
            *processor.apvts.getParameter(b.paramId), b.relay, nullptr));

    // ── Toggle attachments ────────────────────────────────────────────
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::BYPASS), bypassRelay, nullptr);
    punchEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::PUNCH_ENABLED), punchEnabledRelay, nullptr);
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

    // JUCE BinaryData naming: dots → underscores, dashes STRIPPED
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
                std::vector<std::byte> bytes(
                    reinterpret_cast<const std::byte*>(data),
                    reinterpret_cast<const std::byte*>(data) + size);
                return juce::WebBrowserComponent::Resource{
                    std::move(bytes),
                    juce::String(getMimeForExtension(extension))
                };
            }
        }
    }
    return std::nullopt;
}
