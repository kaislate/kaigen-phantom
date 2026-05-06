#include "PluginEditor.h"
#include "BinaryData.h"
#include "PresetMigration.h"

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

// Set to true by JS whenever a text input / textarea / contenteditable has
// focus inside the WebView. While true, the keyboard subclass lets the
// WebView handle keys itself so the user can type. While false, keys are
// forwarded up to the top-level window (the DAW host) so spacebar plays,
// A–L triggers MIDI keyboard mode, etc.
std::atomic<bool> sWebViewInputFocused { false };

bool shouldForwardKey(UINT msg)
{
    return msg == WM_KEYDOWN  || msg == WM_KEYUP
        || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP
        || msg == WM_CHAR       || msg == WM_SYSCHAR;
}

LRESULT CALLBACK webViewFocusSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR id, DWORD_PTR)
{
    const bool inputFocused = sWebViewInputFocused.load(std::memory_order_relaxed);

    // When nothing text-y is focused, don't let a mouse click shift keyboard
    // focus onto the WebView. The click itself still goes through to JS so
    // knob drags work — we just refuse to become the active window.
    if (msg == WM_MOUSEACTIVATE && !inputFocused)
        return MA_NOACTIVATE;

    if (msg == WM_SETFOCUS)
    {
        // Redirect focus to the parent HWND so the DAW keeps keyboard MIDI
        if (HWND parent = GetParent(hwnd))
            SetFocus(parent);
        return 0;
    }

    // Forward keyboard input to the DAW's top-level window unless a text
    // field inside the WebView currently has focus.
    if (shouldForwardKey(msg) && !inputFocused)
    {
        if (HWND top = GetAncestor(hwnd, GA_ROOT))
        {
            if (top != hwnd)
            {
                PostMessage(top, msg, wParam, lParam);
                return 0;
            }
        }
    }

    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(hwnd, webViewFocusSubclass, id);
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

BOOL CALLBACK installOnChromeWindows(HWND hwnd, LPARAM)
{
    wchar_t cls[256] = {};
    GetClassNameW(hwnd, cls, 255);
    // Match any Chromium-owned child window that can hold keyboard focus.
    // "Chrome_WidgetWin"        — outer WebView HWND
    // "Chrome_RenderWidgetHostHWND" — inner renderer (where keys actually land)
    // "Intermediate D3D Window"      — compositing layer (seen in some versions)
    const bool isChromeWindow =
            wcsncmp(cls, L"Chrome_",            7)  == 0 ||
            wcsncmp(cls, L"Intermediate D3D",  16) == 0;

    if (isChromeWindow)
        SetWindowSubclass(hwnd, webViewFocusSubclass, kFocusRedirectId, 0);
    EnumChildWindows(hwnd, installOnChromeWindows, 0);
    return TRUE;
}

} // namespace

void PhantomEditor::FocusRescanTimer::timerCallback()
{
    if (owner == nullptr) return;
    if (auto* peer = owner->getPeer())
        installOnChromeWindows((HWND) peer->getNativeHandle(), 0);
}

void PhantomEditor::parentHierarchyChanged()
{
    // WebView2 creates its HWNDs asynchronously, and some Chromium helper
    // windows appear only after the user interacts with the UI (e.g., after
    // first mouse-down on a canvas). We rescan on a long-running timer so
    // those windows also get the focus subclass installed.
    focusRescanTimer.owner = this;
    if (!focusRescanTimer.isTimerRunning())
        focusRescanTimer.startTimer(1000);

    // Also do a few quick passes during startup to catch windows before
    // the first real scan tick.
    auto tryInstall = [this]()
    {
        if (auto* peer = getPeer())
            installOnChromeWindows((HWND) peer->getNativeHandle(), 0);
    };
    for (int delayMs : { 50, 200, 500 })
        juce::Timer::callAfterDelay(delayMs, tryInstall);
}
#endif

juce::WebBrowserComponent::Options PhantomEditor::buildWebViewOptions(PhantomEditor& self)
{
   #if JUCE_WINDOWS
    // Force WebView2 to render at devicePixelRatio = 1 regardless of monitor
    // DPR. On HiDPI displays this quarters the GPU texture memory per surface
    // (a 2× DPR → 1× drop cuts texture area by 4), dramatically reducing DWM
    // compositor work when multiple plugin UIs are visible at once. Visual
    // tradeoff: canvas content renders slightly softer on HiDPI, but the
    // perf ceiling with 2+ visible instances rises substantially.
    std::wstring webViewArgs = L"--force-device-scale-factor=1";

    // Debug port is opt-in (see prior note on TCP-bind stalls). Set
    // KAIGEN_PHANTOM_DEVTOOLS=1 before launching the host to re-enable
    // DevTools on port 9222.
    if (juce::SystemStats::getEnvironmentVariable("KAIGEN_PHANTOM_DEVTOOLS", {}).isNotEmpty())
        webViewArgs += L" --remote-debugging-port=9222 --remote-allow-origins=*";

    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", webViewArgs.c_str());
   #endif

    // Shared WebView2 User Data Folder across all Phantom instances in the same
    // process. WebView2 reuses a single browser process for WebViews that share
    // a UDF, which is cheaper at open-time than spawning a fresh Chromium per
    // instance. The UDF lives under our app's roaming-data dir rather than the
    // OS temp dir so it doesn't collide with other apps' temp files.
    auto udf = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Kaigen")
                   .getChildFile("KaigenPhantom")
                   .getChildFile("WebView2UserData");
    udf.createDirectory();

    auto options = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(udf))
        .withNativeIntegrationEnabled();

    // ── Slider relays ─────────────────────────────────────────────────
    // Per-engine relays are registered as A/B pairs so the JS dispatch layer
    // can pick which side to bind based on the active engine tab.
    juce::WebSliderRelay* sliderRelays[] = {
        &self.inputGainRelay,
        &self.ghostRelayA,                &self.ghostRelayB,
        &self.phantomThresholdRelayA,     &self.phantomThresholdRelayB,
        &self.phantomStrengthRelayA,      &self.phantomStrengthRelayB,
        &self.outputGainRelayA,           &self.outputGainRelayB,
        &self.recipeH2RelayA,             &self.recipeH2RelayB,
        &self.recipeH3RelayA,             &self.recipeH3RelayB,
        &self.recipeH4RelayA,             &self.recipeH4RelayB,
        &self.recipeH5RelayA,             &self.recipeH5RelayB,
        &self.recipeH6RelayA,             &self.recipeH6RelayB,
        &self.recipeH7RelayA,             &self.recipeH7RelayB,
        &self.recipeH8RelayA,             &self.recipeH8RelayB,
        &self.harmonicSaturationRelayA,   &self.harmonicSaturationRelayB,
        &self.synthStepRelayA,            &self.synthStepRelayB,
        &self.synthDutyRelayA,            &self.synthDutyRelayB,
        &self.synthSkipRelayA,            &self.synthSkipRelayB,
        &self.envAttackRelayA,            &self.envAttackRelayB,
        &self.envReleaseRelayA,           &self.envReleaseRelayB,
        &self.binauralWidthRelayA,        &self.binauralWidthRelayB,
        &self.stereoWidthRelayA,          &self.stereoWidthRelayB,
        &self.synthLPFRelayA,             &self.synthLPFRelayB,
        &self.synthHPFRelayA,             &self.synthHPFRelayB,
        &self.synthWaveletLengthRelayA,   &self.synthWaveletLengthRelayB,
        &self.synthGateThresholdRelayA,   &self.synthGateThresholdRelayB,
        &self.synthH1RelayA,              &self.synthH1RelayB,
        &self.synthSubRelayA,             &self.synthSubRelayB,
        &self.synthMinSamplesRelayA,      &self.synthMinSamplesRelayB,
        &self.synthMaxSamplesRelayA,      &self.synthMaxSamplesRelayB,
        &self.trackingSpeedRelayA,        &self.trackingSpeedRelayB,
        &self.punchAmountRelayA,          &self.punchAmountRelayB,
        &self.synthBoostThresholdRelayA,  &self.synthBoostThresholdRelayB,
        &self.synthBoostAmountRelayA,     &self.synthBoostAmountRelayB,
        &self.morphAmountRelay,
        &self.macro1Relay,
        &self.macro2Relay,
        &self.macro3Relay,
        &self.macro4Relay,
    };
    for (auto* r : sliderRelays)
        options = options.withOptionsFrom(*r);

    // ── Combo-box relays ──────────────────────────────────────────────
    juce::WebComboBoxRelay* comboRelays[] = {
        &self.modeRelayA,           &self.modeRelayB,
        &self.ghostModeRelayA,      &self.ghostModeRelayB,
        &self.recipePresetRelayA,   &self.recipePresetRelayB,
        &self.binauralModeRelayA,   &self.binauralModeRelayB,
        &self.filterSlopeRelayA,    &self.filterSlopeRelayB,
    };
    for (auto* r : comboRelays)
        options = options.withOptionsFrom(*r);

    // ── Toggle relays ────────────────────────────────────────────────
    // Globals stay registered once.
    options = options.withOptionsFrom(self.bypassRelay);
    options = options.withOptionsFrom(self.inputGainAutoRelay);
    // Per-engine toggles get both A and B registered.
    options = options.withOptionsFrom(self.punchEnabledRelayA);
    options = options.withOptionsFrom(self.punchEnabledRelayB);
    options = options.withOptionsFrom(self.midiTriggerRelayA);
    options = options.withOptionsFrom(self.midiTriggerRelayB);
    options = options.withOptionsFrom(self.midiGateReleaseRelayA);
    options = options.withOptionsFrom(self.midiGateReleaseRelayB);

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
                obj->setProperty("input",   inputBins);
                obj->setProperty("output",  outputBins);

                // Per-engine spectra are only consumed by the JS Split-mode
                // renderer. Skip the two FFTs (one per engine, kFftSize=8192)
                // when the user is in Combined mode — saves significant CPU at
                // the ~30Hz UI poll cadence. The viewMode key still travels
                // unconditionally so JS knows which renderer to dispatch to.
                const bool needPerEngine =
                    self.processor.getSpectrumViewMode() == PhantomProcessor::SpectrumViewMode::Split;

                if (needPerEngine)
                {
                    // Per-engine spectra — computed UI-side from the ring buffers
                    // populated in processBlock (Task 3). Same Hann window + FFT +
                    // log-binning pipeline as input/output, on the engine's mono
                    // channel-0 output.
                    std::array<float, PhantomProcessor::kSpectrumBins> engineABins {};
                    std::array<float, PhantomProcessor::kSpectrumBins> engineBBins {};
                    self.processor.computeEngineSpectrum(
                        PhantomProcessor::SpectrumEngineId::A, engineABins);
                    self.processor.computeEngineSpectrum(
                        PhantomProcessor::SpectrumEngineId::B, engineBBins);

                    juce::Array<juce::var> engineAArr, engineBArr;
                    for (int i = 0; i < PhantomProcessor::kSpectrumBins; ++i)
                    {
                        engineAArr.add(engineABins[(size_t) i]);
                        engineBArr.add(engineBBins[(size_t) i]);
                    }

                    obj->setProperty("engineA", engineAArr);
                    obj->setProperty("engineB", engineBArr);
                }

                obj->setProperty("viewMode",
                    self.processor.getSpectrumViewMode() == PhantomProcessor::SpectrumViewMode::Combined
                        ? "Combined" : "Split");
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
                const int presetIdx = (int) self.processor.apvts.getRawParameterValue(ParamID::A_RECIPE_PRESET)->load();
                static const char* presetNames[] = { "Warm","Aggressive","Hollow","Dense","Stable","Weird","Custom" };
                obj->setProperty("preset", juce::String(presetNames[juce::jlimit(0, 6, presetIdx)]));
                complete(juce::var(obj));
            })
        .withNativeFunction("getOscilloscopeData",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto& engine = self.processor.getActiveEngine();
                juce::Array<juce::var> inArr, synthArr, outArr;
                for (int i = 0; i < PhantomEngine::kOscBufSize; ++i)
                {
                    inArr  .add((double) self.processor.oscInputBuf [(size_t) i]);
                    synthArr.add((double) engine.oscSynthBuf[(size_t) i]);
                    outArr .add((double) self.processor.oscOutputBuf[(size_t) i]);
                }
                auto* obj = new juce::DynamicObject();
                obj->setProperty("input",       inArr);
                obj->setProperty("synth",       synthArr);
                obj->setProperty("output",      outArr);
                obj->setProperty("inputWrPos",  (int) self.processor.oscInputWrPos .load(std::memory_order_relaxed));
                obj->setProperty("synthWrPos",  (int) engine.oscSynthWrPos.load(std::memory_order_relaxed));
                obj->setProperty("outputWrPos", (int) self.processor.oscOutputWrPos.load(std::memory_order_relaxed));
                obj->setProperty("sampleRate",   (double) self.processor.getSampleRate());
                obj->setProperty("synthPeak",    (double) engine.getSynthInputPeak());
                complete(juce::var(obj));
            })
        .withNativeFunction("setEditorHeight",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                const int height = args.size() > 0 ? (int) args[0] : 970;
                const int clamped = juce::jlimit(400, 2000, height);
                juce::MessageManager::callAsync([weakSelf = juce::Component::SafePointer<PhantomEditor>(&self), clamped]
                {
                    if (auto* p = weakSelf.getComponent())
                        p->setSize(1300, clamped);
                });
                complete(juce::var(true));
            })
        .withNativeFunction("setInputFocused",
            // Tells the Win32 subclass whether a text field inside the WebView
            // has focus — determines whether keyboard events are forwarded to
            // the DAW (false) or handled by the WebView (true).
            []([[maybe_unused]] const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
               #if JUCE_WINDOWS
                const bool focused = args.size() > 0 && args[0].isBool() && (bool) args[0];
                sWebViewInputFocused.store(focused, std::memory_order_relaxed);
               #endif
                complete(juce::var(true));
            })
        .withNativeFunction("returnFocusToHost",
            // Called from JS after any mouseup on a non-text element. WebView2
            // grabs keyboard focus internally during interaction (through its
            // own focus routing that bypasses Windows focus APIs), so we have
            // to explicitly hand focus back to the DAW's top-level window.
            [&self]([[maybe_unused]] const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
               #if JUCE_WINDOWS
                juce::MessageManager::callAsync([weakSelf = juce::Component::SafePointer<PhantomEditor>(&self)]
                {
                    if (auto* ed = weakSelf.getComponent())
                    {
                        if (auto* peer = ed->getPeer())
                        {
                            HWND pluginHwnd = (HWND) peer->getNativeHandle();
                            HWND top = GetAncestor(pluginHwnd, GA_ROOT);
                            if (top != nullptr) SetFocus(top);
                        }
                    }
                });
               #endif
                complete(juce::var(true));
            })
        .withNativeFunction("forwardKeyToHost",
            // JS calls this for every keydown/keyup that lands on a non-input
            // DOM element. We PostMessage the key to the DAW's top-level window
            // so transport controls (spacebar) and MIDI keyboard (A-L) work
            // regardless of whether the WebView stole keyboard focus.
            //
            // Args: [virtualKeyCode: int, isDown: bool]
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
               #if JUCE_WINDOWS
                if (args.size() >= 2)
                {
                    const WPARAM vk = (WPARAM) (int) args[0];
                    const bool isDown = args[1].isBool() && (bool) args[1];

                    if (auto* peer = self.getPeer())
                    {
                        HWND pluginHwnd = (HWND) peer->getNativeHandle();
                        HWND top = GetAncestor(pluginHwnd, GA_ROOT);
                        if (top != nullptr && top != pluginHwnd)
                        {
                            // Minimal, well-formed lParam:
                            //   bits 0-15  = repeat count (1)
                            //   bit  30    = previous key state (1 on release)
                            //   bit  31    = transition state  (1 on release)
                            const LPARAM lp = isDown ? LPARAM(1)
                                                     : LPARAM(0xC0000001u);
                            const UINT msg = isDown ? WM_KEYDOWN : WM_KEYUP;
                            PostMessage(top, msg, vk, lp);
                        }
                    }
                }
               #endif
                complete(juce::var(true));
            })
        // ── Preset system ──────────────────────────────────────────────
        .withNativeFunction("getAllPresets",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto& pm = self.processor.getPresetManager();
                auto all = pm.getAllPresets();

                auto* root = new juce::DynamicObject();
                for (const auto& [packName, presets] : all)
                {
                    juce::Array<juce::var> arr;
                    for (const auto& p : presets)
                    {
                        auto* meta = new juce::DynamicObject();
                        meta->setProperty("name",        p.metadata.name);
                        meta->setProperty("type",        p.metadata.type);
                        meta->setProperty("designer",    p.metadata.designer);
                        meta->setProperty("description", p.metadata.description);
                        meta->setProperty("isFavorite",  p.metadata.isFavorite);
                        meta->setProperty("isFactory",   p.metadata.isFactory);
                        meta->setProperty("presetKind",
                            kaigen::phantom::presetKindToString(p.metadata.presetKind));

                        // Preview: 7 harmonic weights + crossover Hz + skip count
                        auto* preview = new juce::DynamicObject();
                        juce::Array<juce::var> hArr;
                        for (int i = 0; i < 7; ++i)
                            hArr.add(juce::var(p.preview.h[i]));
                        preview->setProperty("h",         juce::var(hArr));
                        preview->setProperty("crossover", juce::var(p.preview.crossover));
                        preview->setProperty("skip",      juce::var(p.preview.skip));

                        auto* item = new juce::DynamicObject();
                        item->setProperty("metadata", juce::var(meta));
                        item->setProperty("preview",  juce::var(preview));
                        arr.add(juce::var(item));
                    }
                    root->setProperty(packName, juce::var(arr));
                }

                complete(juce::JSON::toString(juce::var(root)));
            })
        .withNativeFunction("loadPreset",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 2)
                {
                    complete(juce::var(false));
                    return;
                }

                const auto presetName = args[0].toString();
                const auto packName   = args[1].toString();

                // APVTS mutation must run on the message thread.
                // PR1: ABSlotManager is no longer owned by the processor; load
                // via the simple APVTS-only path. PR2 will reintroduce
                // tab/A-B-aware preset routing as part of the new dual-engine UX.
                juce::MessageManager::callAsync(
                    [weakSelf = juce::Component::SafePointer<PhantomEditor>(&self), presetName, packName]
                    {
                        if (auto* ed = weakSelf.getComponent())
                        {
                            ed->processor.getPresetManager().loadPreset(
                                ed->processor.apvts, presetName, packName);
                        }
                    });

                complete(juce::var(true));
            })
        .withNativeFunction("savePreset",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1)
                {
                    complete(juce::var(juce::String{}));
                    return;
                }

                const auto name        = args[0].toString();
                const auto type        = args.size() > 1 ? args[1].toString() : juce::String("Experimental");
                const auto designer    = args.size() > 2 ? args[2].toString() : juce::String("User");
                const auto description = args.size() > 3 ? args[3].toString() : juce::String();
                const bool overwrite   = args.size() > 4 && args[4].isBool() && (bool) args[4];

                // PR1: every save is Single — PresetManager's API was
                // simplified after ABSlotManager was retired. PR2 will
                // reintroduce AB-style saves through the new dual-engine
                // APVTS layout.
                auto savedName = self.processor.getPresetManager().savePreset(
                    self.processor.apvts,
                    name, type, designer, description, overwrite);
                complete(juce::var(savedName));
            })
        .withNativeFunction("setFavorite",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 3)
                {
                    complete(juce::var(false));
                    return;
                }

                const auto name    = args[0].toString();
                const auto pack    = args[1].toString();
                const bool isFav   = args[2].isBool() && (bool) args[2];

                self.processor.getPresetManager().setFavorite(name, pack, isFav);
                complete(juce::var(true));
            })
        .withNativeFunction("deletePreset",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 2)
                {
                    complete(juce::var(false));
                    return;
                }

                const auto name = args[0].toString();
                const auto pack = args[1].toString();

                const bool ok = self.processor.getPresetManager().deletePreset(name, pack);
                complete(juce::var(ok));
            })
        .withNativeFunction("getAllPacks",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto packs = self.processor.getPresetManager().getAllPacks();
                juce::Array<juce::var> arr;
                for (const auto& p : packs)
                {
                    auto* obj = new juce::DynamicObject();
                    obj->setProperty("name",         p.name);
                    obj->setProperty("displayName",  p.displayName);
                    obj->setProperty("description",  p.description);
                    obj->setProperty("designer",     p.designer);
                    obj->setProperty("hasCoverArt",  p.hasCoverArt);
                    obj->setProperty("presetCount",  p.presetCount);
                    arr.add(juce::var(obj));
                }
                complete(juce::JSON::toString(juce::var(arr)));
            })
        // PR1: A/B compare and arc-morph native bindings have been removed.
        // The processor no longer owns ABSlotManager or the legacy MorphEngine.
        // The morph_amount slider drives the audio crossfader directly via APVTS.
        // PR2 will reintroduce engine-tab-aware UI bindings.
        .withNativeFunction("engineGetFocus",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                const auto f = self.processor.getEngineFocus();
                auto* obj = new juce::DynamicObject();
                obj->setProperty("activeTab", f.activeTab == PhantomProcessor::ActiveTab::B ? "B" : "A");
                obj->setProperty("linkOn", f.linkOn);
                complete(juce::var(obj));
            })
        .withNativeFunction("engineSetFocus",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1 || ! args[0].isObject()) { complete({}); return; }
                auto* obj = args[0].getDynamicObject();
                if (obj == nullptr) { complete({}); return; }
                PhantomProcessor::EngineFocus f;
                f.activeTab = (obj->getProperty("activeTab").toString() == "B")
                              ? PhantomProcessor::ActiveTab::B
                              : PhantomProcessor::ActiveTab::A;
                f.linkOn    = (bool) obj->getProperty("linkOn");
                self.processor.setEngineFocus(f);
                complete({});
            })
        .withNativeFunction("spectrumGetViewMode",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                const auto m = self.processor.getSpectrumViewMode();
                complete(juce::var(m == PhantomProcessor::SpectrumViewMode::Combined
                                    ? "Combined" : "Split"));
            })
        .withNativeFunction("spectrumSetViewMode",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1) { complete({}); return; }
                const auto s = args[0].toString();
                self.processor.setSpectrumViewMode(s == "Combined"
                    ? PhantomProcessor::SpectrumViewMode::Combined
                    : PhantomProcessor::SpectrumViewMode::Split);
                complete({});
            })
        // ── Matrix view UI state (mode + per-engine expanded categories) ──
        .withNativeFunction("matrixGetState",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                const auto s = self.processor.getMatrixView();
                auto* obj = new juce::DynamicObject();
                obj->setProperty("mode", s.mode == kaigen::phantom::MatrixMode::Matrix ? "Matrix" : "Slots");
                obj->setProperty("expandedA", s.expandedA);
                obj->setProperty("expandedB", s.expandedB);
                complete(juce::var(obj));
            })
        .withNativeFunction("matrixSetState",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1 || ! args[0].isObject()) { complete({}); return; }
                auto* obj = args[0].getDynamicObject();
                if (obj == nullptr) { complete({}); return; }

                kaigen::phantom::MatrixViewState s;
                s.mode = (obj->getProperty("mode").toString() == "Matrix")
                         ? kaigen::phantom::MatrixMode::Matrix : kaigen::phantom::MatrixMode::Slots;
                if (obj->hasProperty("expandedA")) s.expandedA = obj->getProperty("expandedA").toString();
                if (obj->hasProperty("expandedB")) s.expandedB = obj->getProperty("expandedB").toString();
                self.processor.setMatrixView(s);
                complete(juce::var(true));
            })
        // ── Modulation: routing CRUD + macro metadata (PR3b Task 3) ──────
        .withNativeFunction("modulationGetState",
            [&self](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                juce::DynamicObject::Ptr root = new juce::DynamicObject();

                auto buildEngine = [&](kaigen::phantom::ModulationEngine& eng, const juce::String& prefix) -> juce::var
                {
                    juce::DynamicObject::Ptr engObj = new juce::DynamicObject();

                    // Modulators (macros for PR3b — hardcoded by id since this PR knows the inventory)
                    juce::Array<juce::var> mods;
                    const auto macroIds = (prefix == "a_")
                                        ? std::vector<const char*>{ "macro1", "macro2" }
                                        : std::vector<const char*>{ "macro3", "macro4" };
                    for (const auto& mid : macroIds)
                    {
                        auto* m = eng.findModulator(mid);
                        if (m == nullptr) continue;
                        juce::DynamicObject::Ptr mObj = new juce::DynamicObject();
                        mObj->setProperty("id", juce::String(mid));
                        if (auto* macro = dynamic_cast<kaigen::phantom::Macro*>(m))
                            mObj->setProperty("name", macro->getName());
                        else
                            mObj->setProperty("name", juce::String(mid));
                        mods.add(juce::var(mObj.get()));
                    }
                    engObj->setProperty("modulators", mods);

                    // Routings
                    juce::Array<juce::var> routes;
                    auto snapshot = eng.getRoutingsSnapshot();
                    for (const auto& r : *snapshot)
                    {
                        juce::DynamicObject::Ptr rObj = new juce::DynamicObject();
                        rObj->setProperty("source", r.sourceId);
                        rObj->setProperty("param",  r.paramId);
                        rObj->setProperty("depth",  r.depth);
                        rObj->setProperty("invert", r.polarityInverted);
                        routes.add(juce::var(rObj.get()));
                    }
                    engObj->setProperty("routings", routes);

                    // Eligible params (for the "+ Add destination" picker)
                    juce::Array<juce::var> eligible;
                    for (const auto& leaf : kaigen::phantom::PresetMigration::getPerEngineLeaves())
                    {
                        const juce::String pid = prefix + leaf;
                        if (auto* p = self.processor.apvts.getParameter(pid))
                        {
                            juce::DynamicObject::Ptr pObj = new juce::DynamicObject();
                            pObj->setProperty("id",   pid);
                            pObj->setProperty("name", p->getName(64));
                            eligible.add(juce::var(pObj.get()));
                        }
                    }
                    engObj->setProperty("eligible", eligible);

                    return juce::var(engObj.get());
                };

                root->setProperty("engineA", buildEngine(self.processor.getModulationEngineA(), "a_"));
                root->setProperty("engineB", buildEngine(self.processor.getModulationEngineB(), "b_"));

                complete(juce::var(root.get()));
            })
        .withNativeFunction("modulationGetLiveState", [&self]
            (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
        {
            juce::DynamicObject::Ptr root = new juce::DynamicObject();

            auto buildEngine = [&](kaigen::phantom::ModulationEngine& eng) -> juce::var
            {
                juce::Array<juce::var> rows;
                auto snapshot = eng.getRoutingsSnapshot();
                for (const auto& r : *snapshot)
                {
                    juce::DynamicObject::Ptr rObj = new juce::DynamicObject();
                    rObj->setProperty("source", r.sourceId);
                    rObj->setProperty("param",  r.paramId);

                    // Base value from APVTS
                    float base = 0.0f;
                    if (auto* p = self.processor.apvts.getRawParameterValue(r.paramId))
                        base = p->load();
                    rObj->setProperty("base", base);

                    // Modulated value from the engine
                    rObj->setProperty("modulated", eng.getModulatedValue(r.paramId, base));

                    // Modulator's live value
                    auto* m = eng.findModulator(r.sourceId);
                    rObj->setProperty("modValue", m ? m->getCurrentValue() : 0.0f);

                    rows.add(juce::var(rObj.get()));
                }
                return juce::var(rows);
            };

            root->setProperty("engineA", buildEngine(self.processor.getModulationEngineA()));
            root->setProperty("engineB", buildEngine(self.processor.getModulationEngineB()));

            // Per-macro live values (used by the slot-view conic rings in
            // live-modulation.js / modulation-panel.js). Keyed by macro id;
            // each entry has { value } so future per-macro fields can be
            // added without breaking subscribers. Macros 1-2 live on engine
            // A, 3-4 on engine B.
            juce::DynamicObject::Ptr macrosObj = new juce::DynamicObject();
            auto addMacro = [&](kaigen::phantom::ModulationEngine& eng, const juce::String& id)
            {
                if (auto* m = eng.findModulator(id))
                {
                    juce::DynamicObject::Ptr mObj = new juce::DynamicObject();
                    mObj->setProperty("value", m->getCurrentValue());
                    macrosObj->setProperty(id, juce::var(mObj.get()));
                }
            };
            addMacro(self.processor.getModulationEngineA(), "macro1");
            addMacro(self.processor.getModulationEngineA(), "macro2");
            addMacro(self.processor.getModulationEngineB(), "macro3");
            addMacro(self.processor.getModulationEngineB(), "macro4");
            root->setProperty("macros", juce::var(macrosObj.get()));

            // Morph amount (RT-safe atomic load from APVTS).
            float morphAmt = 0.0f;
            if (auto* p = self.processor.apvts.getRawParameterValue("morph_amount"))
                morphAmt = p->load();
            root->setProperty("morph_amount", morphAmt);

            complete(juce::var(root.get()));
        })
        .withNativeFunction("modulationAddRouting",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1 || ! args[0].isObject()) { complete(juce::var(false)); return; }
                auto* obj = args[0].getDynamicObject();
                if (obj == nullptr) { complete(juce::var(false)); return; }

                kaigen::phantom::Routing r;
                r.sourceId = obj->getProperty("source").toString();
                r.paramId  = obj->getProperty("param").toString();
                {
                    const auto depthVar = obj->getProperty("depth");
                    r.depth = depthVar.isVoid() ? 0.5f : (float) depthVar;
                }

                const bool isA = r.paramId.startsWith("a_");
                auto& eng = isA ? self.processor.getModulationEngineA()
                               : self.processor.getModulationEngineB();
                const bool ok = eng.addRouting(r);
                if (ok)
                    self.webView.emitEventIfBrowserIsVisible("modulationStateChanged", juce::var{});
                complete(juce::var(ok));
            })
        .withNativeFunction("modulationRemoveRouting",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1 || ! args[0].isObject()) { complete({}); return; }
                auto* obj = args[0].getDynamicObject();
                if (obj == nullptr) { complete({}); return; }
                const auto src = obj->getProperty("source").toString();
                const auto pid = obj->getProperty("param").toString();
                const bool isA = pid.startsWith("a_");
                auto& eng = isA ? self.processor.getModulationEngineA()
                               : self.processor.getModulationEngineB();
                eng.removeRouting(src, pid);
                self.webView.emitEventIfBrowserIsVisible("modulationStateChanged", juce::var{});
                complete({});
            })
        .withNativeFunction("modulationSetRoutingDepth",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1 || ! args[0].isObject()) { complete(juce::var(false)); return; }
                auto* obj = args[0].getDynamicObject();
                if (obj == nullptr) { complete(juce::var(false)); return; }
                const auto src = obj->getProperty("source").toString();
                const auto pid = obj->getProperty("param").toString();
                const auto depthVar = obj->getProperty("depth");
                const float depth = depthVar.isVoid() ? 0.5f : (float) depthVar;
                const bool isA = pid.startsWith("a_");
                auto& eng = isA ? self.processor.getModulationEngineA()
                               : self.processor.getModulationEngineB();
                const bool ok = eng.setRoutingDepth(src, pid, depth);
                if (ok)
                    self.webView.emitEventIfBrowserIsVisible("modulationStateChanged", juce::var{});
                complete(juce::var(ok));
            })
        .withNativeFunction("modulationSetMacroName",
            [&self](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() < 1 || ! args[0].isObject()) { complete({}); return; }
                auto* obj = args[0].getDynamicObject();
                if (obj == nullptr) { complete({}); return; }
                const auto sourceId = obj->getProperty("source").toString();
                const auto name     = obj->getProperty("name").toString();

                const bool isA = (sourceId == "macro1" || sourceId == "macro2");
                auto& eng = isA ? self.processor.getModulationEngineA()
                               : self.processor.getModulationEngineB();
                auto* m = eng.findModulator(sourceId);
                if (auto* macro = dynamic_cast<kaigen::phantom::Macro*>(m))
                {
                    macro->setName(name);
                    self.webView.emitEventIfBrowserIsVisible("modulationStateChanged", juce::var{});
                }
                complete({});
            })
        .withResourceProvider([&self](const auto& url) { return self.getResource(url); });

    return options;
}

PhantomEditor::PhantomEditor(PhantomProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      webView(buildWebViewOptions(*this))
{
    // Plugin never wants keyboard focus by default; clicks on JUCE components
    // (the editor, the WebBrowserComponent wrapper) must not steal keyboard
    // focus from the DAW. The WebView's internal focus model is handled
    // separately in preset-system.js (mousedown preventDefault on non-inputs)
    // and the Win32 subclass in webViewFocusSubclass.
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    webView.setWantsKeyboardFocus(false);
    webView.setMouseClickGrabsKeyboardFocus(false);
    // Initial editor height = wrap (820) + always-visible modulation panel
    // (150) = 970. Matches BASE_HEIGHT in modulation-panel.js. The JS still
    // calls setEditorHeight(970) on load as a belt-and-braces fallback;
    // setting it here prevents a brief flicker before that JS fires.
    setSize(1300, 970);
    addAndMakeVisible(webView);

    juce::MessageManager::callAsync([this]()
    {
        webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    });

    // ── Slider attachments ────────────────────────────────────────────
    // PR2 Task 3: every per-engine param now has TWO attachments (A and B),
    // each bound to its own a_*/b_* relay. The JS dispatch layer (Task 4)
    // will pick which side to bind to a given DOM control based on
    // window.__kaigenActiveTab (and __kaigenLinkOn for LINK fan-out).
    // Globals (input_gain, morph_amount) stay single-attached.
    struct SliderBinding { const char* paramId; juce::WebSliderRelay& relay; };
    SliderBinding sliderBindings[] = {
        { ParamID::INPUT_GAIN,                inputGainRelay },

        { ParamID::A_GHOST,                   ghostRelayA },
        { ParamID::B_GHOST,                   ghostRelayB },
        { ParamID::A_PHANTOM_THRESHOLD,       phantomThresholdRelayA },
        { ParamID::B_PHANTOM_THRESHOLD,       phantomThresholdRelayB },
        { ParamID::A_PHANTOM_STRENGTH,        phantomStrengthRelayA },
        { ParamID::B_PHANTOM_STRENGTH,        phantomStrengthRelayB },
        { ParamID::A_OUTPUT_GAIN,             outputGainRelayA },
        { ParamID::B_OUTPUT_GAIN,             outputGainRelayB },
        { ParamID::A_RECIPE_H2,               recipeH2RelayA },
        { ParamID::B_RECIPE_H2,               recipeH2RelayB },
        { ParamID::A_RECIPE_H3,               recipeH3RelayA },
        { ParamID::B_RECIPE_H3,               recipeH3RelayB },
        { ParamID::A_RECIPE_H4,               recipeH4RelayA },
        { ParamID::B_RECIPE_H4,               recipeH4RelayB },
        { ParamID::A_RECIPE_H5,               recipeH5RelayA },
        { ParamID::B_RECIPE_H5,               recipeH5RelayB },
        { ParamID::A_RECIPE_H6,               recipeH6RelayA },
        { ParamID::B_RECIPE_H6,               recipeH6RelayB },
        { ParamID::A_RECIPE_H7,               recipeH7RelayA },
        { ParamID::B_RECIPE_H7,               recipeH7RelayB },
        { ParamID::A_RECIPE_H8,               recipeH8RelayA },
        { ParamID::B_RECIPE_H8,               recipeH8RelayB },
        { ParamID::A_HARMONIC_SATURATION,     harmonicSaturationRelayA },
        { ParamID::B_HARMONIC_SATURATION,     harmonicSaturationRelayB },
        { ParamID::A_SYNTH_STEP,              synthStepRelayA },
        { ParamID::B_SYNTH_STEP,              synthStepRelayB },
        { ParamID::A_SYNTH_DUTY,              synthDutyRelayA },
        { ParamID::B_SYNTH_DUTY,              synthDutyRelayB },
        { ParamID::A_SYNTH_SKIP,              synthSkipRelayA },
        { ParamID::B_SYNTH_SKIP,              synthSkipRelayB },
        { ParamID::A_ENV_ATTACK_MS,           envAttackRelayA },
        { ParamID::B_ENV_ATTACK_MS,           envAttackRelayB },
        { ParamID::A_ENV_RELEASE_MS,          envReleaseRelayA },
        { ParamID::B_ENV_RELEASE_MS,          envReleaseRelayB },
        { ParamID::A_BINAURAL_WIDTH,          binauralWidthRelayA },
        { ParamID::B_BINAURAL_WIDTH,          binauralWidthRelayB },
        { ParamID::A_STEREO_WIDTH,            stereoWidthRelayA },
        { ParamID::B_STEREO_WIDTH,            stereoWidthRelayB },
        { ParamID::A_SYNTH_LPF_HZ,            synthLPFRelayA },
        { ParamID::B_SYNTH_LPF_HZ,            synthLPFRelayB },
        { ParamID::A_SYNTH_HPF_HZ,            synthHPFRelayA },
        { ParamID::B_SYNTH_HPF_HZ,            synthHPFRelayB },
        { ParamID::A_SYNTH_WAVELET_LENGTH,    synthWaveletLengthRelayA },
        { ParamID::B_SYNTH_WAVELET_LENGTH,    synthWaveletLengthRelayB },
        { ParamID::A_SYNTH_GATE_THRESHOLD,    synthGateThresholdRelayA },
        { ParamID::B_SYNTH_GATE_THRESHOLD,    synthGateThresholdRelayB },
        { ParamID::A_SYNTH_H1,                synthH1RelayA },
        { ParamID::B_SYNTH_H1,                synthH1RelayB },
        { ParamID::A_SYNTH_SUB,               synthSubRelayA },
        { ParamID::B_SYNTH_SUB,               synthSubRelayB },
        { ParamID::A_SYNTH_MIN_SAMPLES,       synthMinSamplesRelayA },
        { ParamID::B_SYNTH_MIN_SAMPLES,       synthMinSamplesRelayB },
        { ParamID::A_SYNTH_MAX_SAMPLES,       synthMaxSamplesRelayA },
        { ParamID::B_SYNTH_MAX_SAMPLES,       synthMaxSamplesRelayB },
        { ParamID::A_TRACKING_SPEED,          trackingSpeedRelayA },
        { ParamID::B_TRACKING_SPEED,          trackingSpeedRelayB },
        { ParamID::A_PUNCH_AMOUNT,            punchAmountRelayA },
        { ParamID::B_PUNCH_AMOUNT,            punchAmountRelayB },
        { ParamID::A_SYNTH_BOOST_THRESHOLD,   synthBoostThresholdRelayA },
        { ParamID::B_SYNTH_BOOST_THRESHOLD,   synthBoostThresholdRelayB },
        { ParamID::A_SYNTH_BOOST_AMOUNT,      synthBoostAmountRelayA },
        { ParamID::B_SYNTH_BOOST_AMOUNT,      synthBoostAmountRelayB },

        { ParamID::MORPH_AMOUNT,              morphAmountRelay },

        { ParamID::MACRO1,                    macro1Relay },
        { ParamID::MACRO2,                    macro2Relay },
        { ParamID::MACRO3,                    macro3Relay },
        { ParamID::MACRO4,                    macro4Relay },
    };
    for (auto& b : sliderBindings)
        sliderAttachments.push_back(std::make_unique<juce::WebSliderParameterAttachment>(
            *processor.apvts.getParameter(b.paramId), b.relay, nullptr));

    // ── Combo attachments ─────────────────────────────────────────────
    struct ComboBinding { const char* paramId; juce::WebComboBoxRelay& relay; };
    ComboBinding comboBindings[] = {
        { ParamID::A_MODE,                modeRelayA },
        { ParamID::B_MODE,                modeRelayB },
        { ParamID::A_GHOST_MODE,          ghostModeRelayA },
        { ParamID::B_GHOST_MODE,          ghostModeRelayB },
        { ParamID::A_RECIPE_PRESET,       recipePresetRelayA },
        { ParamID::B_RECIPE_PRESET,       recipePresetRelayB },
        { ParamID::A_BINAURAL_MODE,       binauralModeRelayA },
        { ParamID::B_BINAURAL_MODE,       binauralModeRelayB },
        { ParamID::A_SYNTH_FILTER_SLOPE,  filterSlopeRelayA },
        { ParamID::B_SYNTH_FILTER_SLOPE,  filterSlopeRelayB },
    };
    for (auto& b : comboBindings)
        comboAttachments.push_back(std::make_unique<juce::WebComboBoxParameterAttachment>(
            *processor.apvts.getParameter(b.paramId), b.relay, nullptr));

    // ── Toggle attachments ────────────────────────────────────────────
    // Globals (single attachment each).
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::BYPASS), bypassRelay, nullptr);
    inputGainAutoAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::INPUT_GAIN_AUTO), inputGainAutoRelay, nullptr);

    // Per-engine toggles — paired A/B attachments, each bound to its respective param.
    punchEnabledAttachmentA = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::A_PUNCH_ENABLED), punchEnabledRelayA, nullptr);
    punchEnabledAttachmentB = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::B_PUNCH_ENABLED), punchEnabledRelayB, nullptr);
    midiTriggerAttachmentA = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::A_MIDI_TRIGGER_ENABLED), midiTriggerRelayA, nullptr);
    midiTriggerAttachmentB = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::B_MIDI_TRIGGER_ENABLED), midiTriggerRelayB, nullptr);
    midiGateReleaseAttachmentA = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::A_MIDI_GATE_RELEASE), midiGateReleaseRelayA, nullptr);
    midiGateReleaseAttachmentB = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processor.apvts.getParameter(ParamID::B_MIDI_GATE_RELEASE), midiGateReleaseRelayB, nullptr);
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
    // Pack cover art served from disk: /pack-cover/<urlencoded packName>
    if (url.startsWith("/pack-cover/"))
    {
        auto packNameEnc = url.fromFirstOccurrenceOf("/pack-cover/", false, false);
        auto packName    = juce::URL::removeEscapeChars(packNameEnc);
        auto coverFile   = processor.getPresetManager().getPackCoverFile(packName);
        if (coverFile.existsAsFile())
        {
            juce::MemoryBlock mb;
            if (coverFile.loadFileAsData(mb))
            {
                std::vector<std::byte> bytes(
                    reinterpret_cast<const std::byte*>(mb.getData()),
                    reinterpret_cast<const std::byte*>(mb.getData()) + mb.getSize());
                const auto ext = coverFile.getFileExtension().substring(1).toLowerCase();
                return juce::WebBrowserComponent::Resource{
                    std::move(bytes),
                    juce::String(getMimeForExtension(ext))
                };
            }
        }
        return std::nullopt;
    }

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
